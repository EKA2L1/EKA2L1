import SwiftUI
import AVFoundation
import UniformTypeIdentifiers

// Home surface:
//   - No device installed → ContentUnavailableView prompting a device import
//     (Android `no_device_installed`). The CTA opens the import Form.
//   - One or more devices → app grid for the current device. The navigation
//     title doubles as a device menu (tap the title) holding the device
//     switcher and, below a divider, "Install device" and "Manage devices";
//     the ellipsis menu holds Settings, the system-apps toggle and help; the
//     "+" menu installs a SIS, a classic N-Gage game card, or an N-Gage 2.0
//     package onto the device. Pulling the grid down re-scans the registry.

// SIS files only — device ROM / RPKG go through ImportDeviceView's own picker.
private let sisTypes: [UTType] = {
    var types = importTypes(extension: "sis", declaredAs: "com.eka2l1.sis")
    for type in importTypes(extension: "sisx", declaredAs: "com.eka2l1.sisx")
    where !types.contains(type) {
        types.append(type)
    }
    return types
}()

// Classic N-Gage game cards: a folder tree, but usually passed around packed.
// The installer unpacks an archive itself, sniffing the container by content.
private let ngageCardTypes: [UTType] = {
    var types: [UTType] = [.folder, .zip]
    for type in archiveTypes + rarTypes where !types.contains(type) {
        types.append(type)
    }
    return types
}()

// N-Gage 2.0 game packages (.n-gage). Copied onto the E drive for the N-Gage
// launcher to install from; see handleNGage2Import.
private let ngage2Types: [UTType] =
    importTypes(extension: "n-gage", declaredAs: "com.eka2l1.ngage")

// Guest fonts. A TrueType font serves every device: the font store backs the
// device's own faces with it, converting its glyphs to the monochrome format
// where the device only understands that one. Symbian's own .gdr bitmap fonts
// are deliberately not offered — they would only ever suit the devices whose
// fonts are already .gdr, and only at the sizes they happen to carry.
private let fontTypes: [UTType] = ["ttf"].compactMap {
    UTType(filenameExtension: $0)
}

private enum HomeImportTarget {
    case sis
    case ngage    // classic N-Gage game card folder (installed onto E:)
    case ngage2   // N-Gage 2.0 .n-gage package (staged into E:\n-gage)
    case font     // .ttf copied into the user font folder
}

// iOS 16 fallback for ContentUnavailableView (which is iOS 17+). Mimics its
// centered icon + title + description + optional action layout so the home
// surface looks consistent across deployment targets.
private struct FallbackUnavailableView<Actions: View>: View {
    let title: String
    let systemImage: String
    let message: String
    @ViewBuilder var actions: () -> Actions

    var body: some View {
        VStack(spacing: 12) {
            Image(systemName: systemImage)
                .font(.system(size: 52))
                .foregroundColor(.secondary)
            Text(title)
                .font(.title2).bold()
                .multilineTextAlignment(.center)
            Text(message)
                .font(.body)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
            actions()
                .padding(.top, 4)
        }
        .padding(40)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

struct ContentView: View {
    @State private var booted = false
    // Devices, booted index, app list and the busy flag live in one store so
    // the device-manager page works off exactly the same state and operations.
    @StateObject private var store = DeviceStore()
    @State private var bootError: String?
    @State private var banner: String?
    // Device the previous run crashed on while booting (corrupt ROM/RPKG).
    // Auto-boot skipped it, so say why instead of leaving the user wondering
    // which device they ended up on.
    @State private var failedBootDevice: String?

    @State private var showingImportDevice = false
    @State private var showingDeviceManager = false
    @State private var homeImportTarget: HomeImportTarget = .sis
    @State private var showingHomeImporter = false
    @State private var showingSettings = false
    @State private var showingOnboarding = false
    @AppStorage("ios.onboarding.completed") private var onboardingCompleted = false

    // Whether to show built-in ROM/system apps alongside user-installed ones.
    // Defaults to false so the list shows only what the user installed.
    @AppStorage("appListShowSystemApps") private var showSystemApps = false

    // Dev/testing convenience: choose a firmware, then launch straight into a
    // given app on startup, skipping the scroll-and-tap. Pass launch arguments
    // as iOS NSArgumentDomain pairs, e.g.
    //   xcrun simctl launch booted com.eka2l1.emulator \
    //     -LaunchROMCode rm-409 -LaunchAppUID 0x2000023D
    // (decimal UID also accepted).
    @State private var autoLaunchUID: UInt32?
    @State private var showingAutoLaunch = false
    @State private var autoLaunchHandled = false
    @State private var launchRomHandled = false

    // App pending uninstall confirmation (set from the long-press context menu).
    @State private var pendingUninstall: EKA2L1AppItem?

    // Files handed over by the system ("Open in EKA2L1" from the Files app /
    // share sheet). On a cold launch onOpenURL can fire before the emulator
    // has booted, so URLs are queued here and drained once boot completes.
    @State private var pendingOpenURLs: [URL] = []

    // Apps shown in the list, honouring the "show system apps" toggle.
    private var visibleApps: [EKA2L1AppItem] {
        showSystemApps ? store.apps : store.apps.filter { !$0.system }
    }

    // Hint shown when the visible list is empty. If system apps are hidden but
    // some exist, point the user at the toggle instead of the install prompt.
    private var emptyAppsHint: String {
        if !showSystemApps && !store.apps.isEmpty {
            return String(localized: "home.empty.hiddenSystemApps")
        }
        return String(localized: "home.empty.noApps")
    }

    private var homeImporterTypes: [UTType] {
        switch homeImportTarget {
        case .sis:
            return sisTypes
        case .ngage:
            return ngageCardTypes
        case .ngage2:
            return ngage2Types
        case .font:
            return fontTypes
        }
    }

    private var homeImporterAllowsMultipleSelection: Bool {
        // A classic N-Gage install takes one game card at a time; SIS packages,
        // .n-gage packages and fonts can be batch-imported.
        homeImportTarget != .ngage
    }

    var body: some View {
        NavigationStack {
            Group {
                if let bootError {
                    if #available(iOS 17, *) {
                        ContentUnavailableView(String(localized: "home.error.initTitle"),
                                               systemImage: "exclamationmark.triangle",
                                               description: Text(bootError))
                    } else {
                        FallbackUnavailableView(title: String(localized: "home.error.initTitle"),
                                                systemImage: "exclamationmark.triangle",
                                                message: bootError) { EmptyView() }
                    }
                } else if store.devices.isEmpty {
                    emptyState
                } else {
                    appList
                }
            }
            // Status messages ride above the home content only; EmulatorView is
            // a pushed destination, so a toast never covers the guest screen.
            .toast(message: $banner)
            .navigationTitle(store.currentDevice?.displayName ?? "EKA2L1")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar { toolbarContent }
            .toolbarTitleMenu { titleMenuContent }
            .navigationDestination(isPresented: $showingSettings) { SettingsView() }
            .navigationDestination(isPresented: $showingDeviceManager) {
                DeviceManagerView(store: store) { showingImportDevice = true }
            }
            .navigationDestination(isPresented: $showingAutoLaunch) {
                if let uid = autoLaunchUID { EmulatorView(uid: uid) }
            }
            .sheet(isPresented: $showingImportDevice) {
                ImportDeviceView { installed in
                    guard installed else { return }
                    Task {
                        if await store.bootNewestDevice() {
                            banner = String(localized: "common.completed")
                        }
                    }
                }
            }
            .sheet(isPresented: $showingOnboarding) {
                OnboardingView {
                    onboardingCompleted = true
                }
            }
            .alert(String(localized: "home.error.deviceBootCrashedTitle"),
                   isPresented: Binding(get: { failedBootDevice != nil },
                                        set: { if !$0 { failedBootDevice = nil } })) {
                Button("common.ok", role: .cancel) { failedBootDevice = nil }
            } message: {
                Text("home.error.deviceBootCrashed \(failedBootDevice ?? "")")
            }
            .fileImporter(isPresented: $showingHomeImporter,
                          allowedContentTypes: homeImporterTypes,
                          allowsMultipleSelection: homeImporterAllowsMultipleSelection) {
                handleHomeImport($0)
            }
        }
        .onAppear {
            bootIfNeeded()
            if !onboardingCompleted && !Self.isAutomationLaunch {
                showingOnboarding = true
            }
        }
        .onOpenURL { url in
            pendingOpenURLs.append(url)
            processPendingOpenURLs()
        }
        .onReceive(NotificationCenter.default.publisher(for: .eka2l1AppListInvalidated)) { _ in
            // A settings action (e.g. a system-language switch) changed how the
            // app list should render; re-scan so the new captions show.
            guard booted else { return }
            store.reloadApps()
        }
        .onReceive(NotificationCenter.default.publisher(for: .eka2l1DevicesChanged)) { _ in
            // A device was renamed in Settings: only the titles changed, so
            // re-read the list to refresh the nav title and device switcher.
            store.reloadDevices()
        }
        .onReceive(NotificationCenter.default.publisher(for: AVAudioSession.interruptionNotification)) { note in
            guard let rawType = note.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt,
                  let type = AVAudioSession.InterruptionType(rawValue: rawType) else {
                return
            }
            switch type {
            case .began:
                EKA2L1Bridge.shared.pause()
            case .ended:
                let rawOptions = note.userInfo?[AVAudioSessionInterruptionOptionKey] as? UInt ?? 0
                let options = AVAudioSession.InterruptionOptions(rawValue: rawOptions)
                if options.contains(.shouldResume) {
                    EKA2L1Bridge.shared.resume()
                }
            @unknown default:
                break
            }
        }
    }

    // Context-menu content for an app. Every app exposes a "copy UID" entry
    // (the hex UID doubles as the label); only user-installed apps additionally
    // offer uninstall — ROM/system apps cannot be removed.
    @ViewBuilder
    private func appContextMenu(for app: EKA2L1AppItem) -> some View {
        Button {
            UIPasteboard.general.string = uidHexString(app.uid)
            banner = String(localized: "home.banner.copied \(uidHexString(app.uid))")
        } label: {
            Label(uidHexString(app.uid), systemImage: "doc.on.doc")
        }

        if !app.system {
            Button(role: .destructive) {
                pendingUninstall = app
            } label: {
                Label("home.uninstall.action", systemImage: "trash")
            }
        }
    }

    // Canonical hex form used both on the menu label and on the clipboard.
    private func uidHexString(_ uid: UInt32) -> String {
        String(format: "0x%08X", uid)
    }

    @ViewBuilder
    private var emptyState: some View {
        if #available(iOS 17, *) {
            ContentUnavailableView {
                Label("home.noDevice.title", systemImage: "iphone.slash")
            } description: {
                Text("import.noDeviceInstalled")
            } actions: {
                Button("import.title") { showingImportDevice = true }
                    .buttonStyle(.borderedProminent)
            }
        } else {
            FallbackUnavailableView(title: String(localized: "home.noDevice.title"),
                                    systemImage: "iphone.slash",
                                    message: String(localized: "import.noDeviceInstalled")) {
                Button("import.title") { showingImportDevice = true }
                    .buttonStyle(.borderedProminent)
            }
        }
    }

    private var appList: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                Text("home.appsCount \(visibleApps.count)")
                    .font(.headline)

                if visibleApps.isEmpty {
                    Text(emptyAppsHint)
                        .font(.caption).foregroundColor(.secondary)
                } else {
                    // Narrower columns so a phone-width screen fits four icons
                    // per row (72pt icon + label, ~4pt slack).
                    LazyVGrid(columns: [GridItem(.adaptive(minimum: 76), spacing: 12)],
                              spacing: 16) {
                        ForEach(visibleApps, id: \.uid) { app in
                            NavigationLink(destination: EmulatorView(uid: app.uid)) {
                                AppGridCell(uid: app.uid, name: app.name)
                            }
                            .buttonStyle(.plain)
                            .contextMenu { appContextMenu(for: app) }
                            // Attached to the icon itself (rather than the
                            // NavigationStack root) so the confirmation is
                            // anchored to the pressed app's cell, matching
                            // iOS 26's popover-style presentation instead of
                            // detaching to a generic bottom sheet.
                            .confirmationDialog(Text("home.uninstall.title \(app.name)"),
                                                isPresented: Binding(
                                                    get: { pendingUninstall?.uid == app.uid },
                                                    set: { if !$0 { pendingUninstall = nil } }),
                                                titleVisibility: .visible) {
                                Button("home.uninstall.confirm", role: .destructive) { uninstall(app) }
                                Button("common.cancel", role: .cancel) { pendingUninstall = nil }
                            } message: {
                                Text("home.uninstall.message")
                            }
                        }
                    }
                    .id(store.currentIndex)
                }
            }
            .padding()
        }
        // Pull down on the grid to re-scan the device's app registry.
        .refreshable { await store.refreshApps() }
    }

    // The navigation title's tap menu (SwiftUI toolbarTitleMenu): the device
    // switcher with a checkmark on the active device, then "Install device"
    // and "Manage devices" below a divider.
    @ViewBuilder
    private var titleMenuContent: some View {
        ForEach(store.devices) { dev in
            Button {
                Task { await store.switchDevice(to: dev.index) }
            } label: {
                if dev.index == store.currentIndex {
                    Label(dev.displayName, systemImage: "checkmark")
                } else {
                    Text(dev.displayName)
                }
            }
            .disabled(store.busy)
        }

        Divider()

        Button {
            showingImportDevice = true
        } label: {
            Label("import.title", systemImage: "square.and.arrow.down")
        }
        .disabled(store.busy)

        Button {
            showingDeviceManager = true
        } label: {
            Label("devices.title", systemImage: "list.bullet")
        }
        .disabled(store.busy)
    }

    private var statusToolbarItem: some ToolbarContent {
        ToolbarItem(placement: .status) {
            ProgressView()
                .controlSize(.small)
                .fixedSize()
        }
    }

    @ToolbarContentBuilder
    private var toolbarContent: some ToolbarContent {
        // A spinner occupies the bottom-center status slot while a long-running
        // task (device switch, SIS/N-Gage install) is in flight; its result is
        // reported by the toast instead. On iOS 26 the shared glass background
        // is hidden so the spinner sits directly on the content.
        if store.busy {
            if #available(iOS 26.0, *) {
                statusToolbarItem.sharedBackgroundVisibility(.hidden)
            } else {
                statusToolbarItem
            }
        }

        if !store.devices.isEmpty {
            ToolbarItemGroup(placement: .topBarTrailing) {
                Menu("home.install", systemImage: "plus") {
                    Button {
                        homeImportTarget = .sis
                        showingHomeImporter = true
                    } label: {
                        Label("home.install.sis", systemImage: "square.and.arrow.down")
                    }
                    
                    // A second Text in a menu button's label renders as the item
                    // subtitle (UIMenuElement.subtitle), spelling out the ROM /
                    // launcher prerequisite for each N-Gage flavour.
                    Button {
                        homeImportTarget = .ngage
                        showingHomeImporter = true
                    } label: {
                        Text("home.installNGage")
                        Text("home.installNGage.subtitle")
                        Image(systemName: "folder.badge.plus")
                    }
                    
                    Button {
                        homeImportTarget = .ngage2
                        showingHomeImporter = true
                    } label: {
                        Text("home.installNGage2")
                        Text("home.installNGage2.subtitle")
                        Image(systemName: "arrow.down.doc")
                    }

                    Divider()

                    Button {
                        homeImportTarget = .font
                        showingHomeImporter = true
                    } label: {
                        Text("home.installFonts")
                        Text("home.installFonts.subtitle")
                        Image(systemName: "textformat")
                    }
                }
                .disabled(store.busy)

                Menu("home.more", systemImage: "ellipsis.circle") {
                    Button {
                        showingSettings = true
                    } label: {
                        Label("settings.title", systemImage: "gearshape")
                    }
                    
                    Button {
                        showSystemApps.toggle()
                    } label: {
                        if showSystemApps {
                            Label("home.hideSystemApps", systemImage: "eye.slash")
                        } else {
                            Label("home.showSystemApps", systemImage: "eye")
                        }
                    }
                    
                    Divider()
                    
                    Button {
                        showingOnboarding = true
                    } label: {
                        Label("onboarding.title", systemImage: "questionmark.circle")
                    }
                }
                .disabled(store.busy)
            }
        }
    }

    private func bootIfNeeded() {
        guard !booted else { return }
        if EKA2L1Bridge.shared.start(documentsPath: documentsRoot()) {
            booted = true
            store.refresh()
            reportFailedBootDevice()
            selectLaunchRomThenAutoLaunch()
            processPendingOpenURLs()
        } else {
            bootError = String(localized: "home.error.checkConsole")
        }
    }

    // Drains system-opened files once the emulator is up. SIS packages are
    // auto-installed onto the current device; .n-gage packages are staged onto
    // the E drive (both mirror the "+" importer). Those are the only types
    // Info.plist registers, so anything else is ignored.
    private func processPendingOpenURLs() {
        guard booted, !pendingOpenURLs.isEmpty else { return }
        let urls = pendingOpenURLs
        pendingOpenURLs = []

        let sisURLs = urls.filter { ["sis", "sisx"].contains($0.pathExtension.lowercased()) }
        let ngage2URLs = urls.filter { $0.pathExtension.lowercased() == "n-gage" }

        if !ngage2URLs.isEmpty {
            handleNGage2Import(.success(ngage2URLs))
        }
        guard !sisURLs.isEmpty else { return }
        guard store.currentIndex >= 0 else {
            banner = String(localized: "home.banner.installDeviceFirst")
            return
        }
        handleSisImport(.success(sisURLs))
    }

    // If launched with -LaunchROMCode, boot that device before any auto app
    // navigation. EmulatorViewController waits for the graphics driver before
    // calling launchApp.
    private func selectLaunchRomThenAutoLaunch() {
        guard !launchRomHandled, let code = Self.launchRomCodeArgument() else {
            maybeAutoLaunch()
            return
        }
        launchRomHandled = true

        guard let target = store.device(withFirmwareCode: code) else {
            bootError = String(localized: "home.error.launchRomMissing \(code)")
            return
        }
        guard target.index != store.currentIndex else {
            maybeAutoLaunch()
            return
        }

        Task {
            guard await store.boot(at: target.index) else {
                bootError = String(localized: "home.error.bootRomFailed \(target.firmwareCode)")
                return
            }
            banner = String(localized: "home.banner.booted \(target.displayName) \(target.firmwareCode)")
            maybeAutoLaunch()
        }
    }

    private func maybeAutoLaunch() {
        guard !autoLaunchHandled, store.currentIndex >= 0, let uid = Self.launchAppUIDArgument() else { return }
        autoLaunchHandled = true
        autoLaunchUID = uid
        Task { @MainActor in
            try? await Task.sleep(until: .now + .seconds(1))
            self.showingAutoLaunch = true
        }
    }

    private static func launchAppUIDArgument() -> UInt32? {
        guard let raw = UserDefaults.standard.string(forKey: "LaunchAppUID")?
            .trimmingCharacters(in: .whitespaces), !raw.isEmpty else { return nil }
        if raw.lowercased().hasPrefix("0x") {
            return UInt32(raw.dropFirst(2), radix: 16)
        }
        return UInt32(raw)
    }

    private static var isAutomationLaunch: Bool {
        launchAppUIDArgument() != nil || UserDefaults.standard.bool(forKey: "EKA2L1RegressionMode")
    }

    private static func launchRomCodeArgument() -> String? {
        for key in ["LaunchROMCode", "LaunchROM", "LaunchDeviceCode", "LaunchDevice"] {
            if let raw = UserDefaults.standard.string(forKey: key)?
                .trimmingCharacters(in: .whitespacesAndNewlines), !raw.isEmpty {
                return raw
            }
        }
        return nil
    }

    // The bridge keeps a device that took the previous run down out of the
    // auto-boot; name it (with the model, when it is still installed) so the
    // user knows which dump to reinstall or delete. Skipped for automated
    // launches, where an alert would sit on top of the app list.
    private func reportFailedBootDevice() {
        guard let code = EKA2L1Bridge.shared.takeFailedBootDeviceCode(), !Self.isAutomationLaunch else { return }
        let device = store.device(withFirmwareCode: code)
        failedBootDevice = device.map { "\($0.displayName) (\(code))" } ?? code
    }

    private func uninstall(_ app: EKA2L1AppItem) {
        pendingUninstall = nil
        let ok = EKA2L1Bridge.shared.uninstallApp(uid: app.uid)
        store.reloadApps()
        banner = ok ? String(localized: "home.banner.uninstalled \(app.name)")
                    : String(localized: "home.banner.uninstallFailed \(app.name)")
    }

    private func handleHomeImport(_ result: Result<[URL], Error>) {
        switch homeImportTarget {
        case .sis:
            handleSisImport(result)
        case .ngage:
            handleNGageImport(result)
        case .ngage2:
            handleNGage2Import(result)
        case .font:
            handleFontImport(result)
        }
    }

    private func handleSisImport(_ result: Result<[URL], Error>) {
        switch result {
        case .failure(let err):
            banner = String(localized: "home.banner.importFailed \(err.localizedDescription)")
        case .success(let urls):
            // Install straight from the picked/opened location. The installer
            // extracts everything onto the device drives, so the package file
            // itself is not needed afterwards — no staging copy. The URLs are
            // security-scoped, so hold the scope across the install call.
            // Extraction can take a while for large packages, so run it off
            // the main thread with the busy indicator up.
            banner = String(localized: "home.banner.installingPackages \(urls.count)")
            Task {
                let installed = await store.perform {
                    var installed = 0
                    for url in urls {
                        let ext = url.pathExtension.lowercased()
                        guard ext == "sis" || ext == "sisx" else { continue }
                        let scoped = url.startAccessingSecurityScopedResource()
                        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
                        if EKA2L1Bridge.installSis(atPath: url.path) { installed += 1 }
                    }
                    return installed
                }
                store.reloadApps()
                banner = String(localized: "home.banner.installedPackages \(installed)")
            }
        }
    }

    private func handleNGageImport(_ result: Result<[URL], Error>) {
        switch result {
        case .failure(let err):
            banner = String(localized: "home.ngage.importFailed \(err.localizedDescription)")
        case .success(let urls):
            guard let url = urls.first else { return }
            banner = String(localized: "home.ngage.installing")
            Task {
                let report = await store.perform { () -> EKA2L1NGageInstallItem in
                    let scoped = url.startAccessingSecurityScopedResource()
                    defer { if scoped { url.stopAccessingSecurityScopedResource() } }
                    return EKA2L1Bridge.installNGageGame(cardPath: url.path)
                }
                store.reloadApps()
                if report.succeeded {
                    let name = report.gameName.trimmingCharacters(in: .whitespacesAndNewlines)
                    banner = name.isEmpty ? String(localized: "home.ngage.installed")
                                          : String(localized: "home.ngage.installedNamed \(name)")
                } else {
                    banner = ngageErrorMessage(report.result)
                }
            }
        }
    }

    // N-Gage 2.0 install directory on the E drive. The mounted physical path is
    // <Documents>/data/drives/e/ (see IosEmulator mount), and the N-Gage
    // launcher reads packages from E:\n-gage.
    private static func ngage2StagingDir() -> String {
        (documentsRoot() as NSString).appendingPathComponent("data/drives/e/n-gage")
    }

    // N-Gage 2.0 packages aren't installed by us — they're just copied onto the
    // E drive so the (separately installed) N-Gage launcher can pick them up.
    // Same security-scoped copy pattern as the SIS importer.
    private func handleNGage2Import(_ result: Result<[URL], Error>) {
        switch result {
        case .failure(let err):
            banner = String(localized: "home.ngage.importFailed \(err.localizedDescription)")
        case .success(let urls):
            banner = String(localized: "home.ngage2.importing \(urls.count)")
            let dir = Self.ngage2StagingDir()
            Task {
                let outcome = await store.perform { () -> Result<Int, Error> in
                    let fm = FileManager.default
                    var imported = 0
                    do {
                        try fm.createDirectory(atPath: dir, withIntermediateDirectories: true)
                        for url in urls {
                            guard url.pathExtension.lowercased() == "n-gage" else { continue }
                            let scoped = url.startAccessingSecurityScopedResource()
                            defer { if scoped { url.stopAccessingSecurityScopedResource() } }
                            let dst = (dir as NSString).appendingPathComponent(url.lastPathComponent)
                            if fm.fileExists(atPath: dst) { try fm.removeItem(atPath: dst) }
                            try fm.copyItem(at: url, to: URL(fileURLWithPath: dst))
                            imported += 1
                        }
                    } catch {
                        return .failure(error)
                    }
                    return .success(imported)
                }
                switch outcome {
                case .success(let imported):
                    banner = String(localized: "home.ngage2.imported \(imported)")
                case .failure(let error):
                    banner = String(localized: "home.ngage.importFailed \(error.localizedDescription)")
                }
            }
        }
    }

    // User font folder, scanned by fbs_server::load_custom_fonts. It sits next
    // to the drives rather than inside one, so a font is shared by every
    // installed device and survives a device reinstall (which rewrites its Z
    // drive). `data` is the emulator storage root on iOS.
    private static func fontInstallDir() -> String {
        (documentsRoot() as NSString).appendingPathComponent("data/fonts")
    }

    // Fonts are only read when the font store is built (fbs_server's
    // constructor), so an import while a device is booted needs a reboot to
    // take effect — the banner says so.
    private func handleFontImport(_ result: Result<[URL], Error>) {
        switch result {
        case .failure(let err):
            banner = String(localized: "home.banner.importFailed \(err.localizedDescription)")
        case .success(let urls):
            let fonts = urls.filter { $0.pathExtension.lowercased() == "ttf" }
            guard !fonts.isEmpty else {
                banner = String(localized: "home.fonts.unsupported")
                return
            }
            banner = String(localized: "home.fonts.importing \(fonts.count)")
            let dir = Self.fontInstallDir()
            Task {
                let outcome = await store.perform { () -> (imported: Int, failure: String?) in
                    let fm = FileManager.default
                    var imported = 0
                    var failure: String?
                    for url in fonts {
                        let scoped = url.startAccessingSecurityScopedResource()
                        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
                        do {
                            try fm.createDirectory(atPath: dir, withIntermediateDirectories: true)
                            let dst = (dir as NSString).appendingPathComponent(url.lastPathComponent)
                            if fm.fileExists(atPath: dst) { try fm.removeItem(atPath: dst) }
                            try fm.copyItem(at: url, to: URL(fileURLWithPath: dst))
                            imported += 1
                        } catch {
                            failure = error.localizedDescription
                        }
                    }
                    return (imported, failure)
                }
                if let failure = outcome.failure, outcome.imported == 0 {
                    banner = String(localized: "home.banner.importFailed \(failure)")
                } else {
                    banner = String(localized: "home.fonts.imported \(outcome.imported)")
                }
            }
        }
    }

    private func ngageErrorMessage(_ code: Int) -> String {
        switch code {
        case 1:
            return String(localized: "home.ngage.error.chooseFolder")
        case 2:
            return String(localized: "home.ngage.error.multipleGames")
        case 3:
            return String(localized: "home.ngage.error.regMissing")
        case 4:
            return String(localized: "home.ngage.error.regCorrupt")
        default:
            return String(localized: "home.ngage.error.generic \(code)")
        }
    }
}

// Content types an importer accepts for one file extension. The identifier
// comes first and has to be one this app declares (Info.plist
// UTImportedTypeDeclarations): a document picker only matches declared types,
// so filtering on the dynamic type the system synthesises for an unregistered
// extension greys out every file it was supposed to offer. types(tag:) then
// adds anything else the system already associates with the extension, so a
// file typed by another app's declaration still comes through. Extension lookup
// is case-insensitive — .ROM and .rom land on the same types.
//
// Never fall back to .data: that re-enables every file in the picker, which is
// exactly what filtering is meant to prevent. An empty list (only possible if
// our own declaration went missing) leaves the picker offering nothing, which
// is the honest failure.
private func importTypes(extension ext: String, declaredAs identifier: String) -> [UTType] {
    var types: [UTType] = []
    if let declared = UTType(identifier) { types.append(declared) }
    for type in UTType.types(tag: ext, tagClass: .filenameExtension, conformingTo: nil)
    where !types.contains(type) {
        types.append(type)
    }
    return types
}

private let romTypes: [UTType] = importTypes(extension: "rom", declaredAs: "com.eka2l1.rom")
private let rpkgTypes: [UTType] = importTypes(extension: "rpkg", declaredAs: "com.eka2l1.rpkg")
// 7z is not one of ours to name — org.7-zip.7-zip-archive is the identifier 7-Zip
// and everything that reads its archives use, so we import that one rather than
// minting a com.eka2l1.* type nothing else would recognise.
private let archiveTypes: [UTType] = importTypes(extension: "7z", declaredAs: "org.7-zip.7-zip-archive")
private let rarTypes: [UTType] = importTypes(extension: "rar", declaredAs: "com.rarlab.rar-archive")

// Shared between the main queue (which sets it from the Stop button) and the
// install thread (which polls it between files), so the accesses are locked.
private final class InstallCancelFlag: @unchecked Sendable {
    private let lock = NSLock()
    private var requested = false

    func request() {
        lock.lock()
        requested = true
        lock.unlock()
    }

    var isRequested: Bool {
        lock.lock()
        defer { lock.unlock() }
        return requested
    }
}

// Device-install Form, in two flavours the user picks between.
//
// Loose files: ROM is mandatory; RPKG is supplied only when the installer needs
// it (S60v2+ dumps). A .7z instead: one file holding either of those same two,
// or a device that someone already unpacked into the emulator's folder layout —
// the installer works out which from the archive's contents.
//
// Everything is read in place at install time. The installer copies the ROM onto
// the device folder and extracts the rest itself, so staging a sandbox copy first
// would only duplicate hundreds of MB. The picked URLs are security-scoped, so
// the scope is opened around the install call (same pattern as the SIS importer).
struct ImportDeviceView: View {
    var onFinish: (Bool) -> Void

    @Environment(\.dismiss) private var dismiss

    private struct PickedFile { let name: String; let url: URL }
    private enum PickTarget { case rom, rpkg, archive }

    private enum SourceKind: Hashable {
        case looseFiles
        case archive
    }

    @State private var source: SourceKind = .looseFiles
    @State private var rom: PickedFile?
    @State private var rpkg: PickedFile?
    @State private var archive: PickedFile?
    // A single fileImporter driven by which row was tapped. Stacking two
    // .fileImporter modifiers on one view makes SwiftUI drop one of them, so
    // we multiplex through this instead. `pickTarget` is read in the
    // completion handler, so it is left set until then (only the bool drives
    // presentation).
    @State private var pickTarget: PickTarget = .rom
    @State private var showingImporter = false
    @State private var installing = false
    @State private var installProgress: Double = 0
    // Set by the Stop button and polled by the installer between files. A class
    // so the installer thread reads the same box the main thread wrote; the
    // separate flag drives the UI, which a reference type would not refresh.
    @State private var cancelFlag: InstallCancelFlag?
    @State private var cancelRequested = false
    @State private var errorMessage: String?

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    Picker("import.source", selection: $source) {
                        Text("import.source.looseFiles").tag(SourceKind.looseFiles)
                        Text("import.source.archive").tag(SourceKind.archive)
                    }
                    .pickerStyle(.menu)
                    .disabled(installing)
                }

                Section {
                    if source == .looseFiles {
                        Button { pickTarget = .rom; showingImporter = true } label: {
                            fileRow(title: String(localized: "import.romFile"), value: rom?.name)
                        }
                        Button { pickTarget = .rpkg; showingImporter = true } label: {
                            fileRow(title: String(localized: "import.rpkgFile"), value: rpkg?.name)
                        }
                    } else {
                        Button { pickTarget = .archive; showingImporter = true } label: {
                            fileRow(title: String(localized: "import.archiveFile"), value: archive?.name)
                        }
                    }
                } footer: {
                    // Both take a LocalizedStringKey rather than a resolved String, so the wiki link in
                    // the loose-files text keeps rendering as markdown.
                    if source == .looseFiles {
                        Text("import.recommendedDevices")
                    } else {
                        Text("import.archiveHint")
                    }
                }

                if let errorMessage {
                    Section { Text(errorMessage).foregroundColor(.red) }
                }

                Section {
                    if installing {
                        VStack(alignment: .leading, spacing: 8) {
                            ProgressView(value: installProgress)
                            Text(cancelRequested
                                 ? String(localized: "import.cancelling")
                                 : String(localized: "import.installing \(Int(installProgress * 100))"))
                                .font(.footnote)
                                .foregroundColor(.secondary)
                        }
                        Button(role: .destructive) {
                            cancelRequested = true
                            cancelFlag?.request()
                        } label: {
                            Text("import.stopInstall")
                        }
                        .disabled(cancelRequested)
                    } else {
                        Button(action: install) {
                            Text("common.install")
                        }
                        .disabled(source == .looseFiles ? (rom == nil) : (archive == nil))
                    }
                }
            }
            .navigationTitle("import.title")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("common.cancel") { dismiss() }
                        .disabled(installing)
                }
            }
            .fileImporter(isPresented: $showingImporter,
                          allowedContentTypes: contentTypes(for: pickTarget),
                          allowsMultipleSelection: false) { result in
                pick(result, target: pickTarget)
            }
            .interactiveDismissDisabled(installing)
        }
    }

    private func fileRow(title: String, value: String?) -> some View {
        HStack {
            Text(title).foregroundColor(.primary)
            Spacer()
            Text(value ?? String(localized: "import.noFileSelected"))
                .foregroundColor(.secondary)
                .lineLimit(1)
                .truncationMode(.middle)
        }
    }

    private func contentTypes(for target: PickTarget) -> [UTType] {
        switch target {
        case .rom: return romTypes
        case .rpkg: return rpkgTypes
        case .archive: return archiveTypes
        }
    }

    private func expectedExtension(for target: PickTarget) -> String {
        switch target {
        case .rom: return "rom"
        case .rpkg: return "rpkg"
        case .archive: return "7z"
        }
    }

    private func pick(_ result: Result<[URL], Error>, target: PickTarget) {
        guard case .success(let urls) = result, let url = urls.first else { return }
        let kind = expectedExtension(for: target)
        // The picker filters by content type, which a file from a provider that
        // types everything as generic data can slip past. The extension is the
        // only thing the installers key off, so hold the line here too rather
        // than feeding a ROM to the RPKG reader (and vice versa).
        guard url.pathExtension.caseInsensitiveCompare(kind) == .orderedSame else {
            errorMessage = String(localized: "import.error.wrongExtension \(kind.uppercased())")
            return
        }

        errorMessage = nil
        let picked = PickedFile(name: url.lastPathComponent, url: url)
        switch target {
        case .rom: rom = picked
        case .rpkg: rpkg = picked
        case .archive: archive = picked
        }
    }

    private func install() {
        // Both flavours run the same way: hold the security scope open across a
        // synchronous install on a background queue, then report on the main one.
        // Only which bridge call sits in the middle differs.
        let urls: [URL]
        let run: @Sendable (@escaping @Sendable (Double) -> Void,
                            @escaping @Sendable () -> Bool) -> EKA2L1InstallResult

        switch source {
        case .looseFiles:
            guard let rom else { return }
            let romPath = rom.url.path
            let rpkgPath = rpkg?.url.path
            urls = [rom.url] + (rpkg.map { [$0.url] } ?? [])
            run = { progress, cancel in
                EKA2L1Bridge.installDevice(romPath: romPath, rpkgPath: rpkgPath,
                                           progress: progress, cancelCheck: cancel)
            }

        case .archive:
            guard let archive else { return }
            let archivePath = archive.url.path
            urls = [archive.url]
            run = { progress, cancel in
                EKA2L1Bridge.installDevice(archivePath: archivePath, progress: progress, cancelCheck: cancel)
            }
        }

        installing = true
        installProgress = 0
        errorMessage = nil
        cancelRequested = false
        let flag = InstallCancelFlag()
        cancelFlag = flag

        DispatchQueue.global(qos: .userInitiated).async {
            let scoped = urls.filter { $0.startAccessingSecurityScopedResource() }
            defer { scoped.forEach { $0.stopAccessingSecurityScopedResource() } }

            let result = run({ fraction in
                DispatchQueue.main.async { installProgress = fraction }
            }, { flag.isRequested })

            DispatchQueue.main.async {
                installing = false
                cancelFlag = nil
                cancelRequested = false
                if result == .success {
                    onFinish(true)
                    dismiss()
                } else if result == .cancelled {
                    dismiss()
                } else {
                    errorMessage = installMessage(for: result)
                }
            }
        }
    }

    private func installMessage(for result: EKA2L1InstallResult) -> String {
        switch result {
        case .success:
            return String(localized: "common.completed")
        case .alreadyExist:
            return String(localized: "import.error.alreadyExists")
        case .determineProductFailure:
            return String(localized: "import.error.determineProduct")
        case .insufficient:
            return String(localized: "import.error.insufficient")
        case .notExist:
            return String(localized: "import.error.notExist")
        case .rpkgCorrupt:
            return String(localized: "import.error.rpkgCorrupt")
        case .vplInvalid:
            return String(localized: "import.error.vplInvalid")
        case .romCorrupt:
            return String(localized: "import.error.romCorrupt")
        case .rofsCorrupt:
            return String(localized: "import.error.rofsCorrupt")
        case .fpsxCorrupt:
            return String(localized: "import.error.fpsxCorrupt")
        case .romFailToCopy:
            return String(localized: "import.error.romCopy")
        case .needRpkg:
            return String(localized: "import.error.needRpkg")
        case .cancelled:
            return String(localized: "import.cancelled")
        case .archiveCorrupt:
            return String(localized: "import.error.archiveCorrupt")
        case .archiveNoDevice:
            return String(localized: "import.error.archiveNoDevice")
        case .generalFailure:
            return String(localized: "common.error")
        @unknown default:
            return String(localized: "common.error")
        }
    }
}

// App icon shared by the grid and row cells. The registered icon
// (MIF/MBM/SVGB/NVG → RGBA → PNG) is lazily decoded off the main queue so
// scrolling stays smooth; the bridge returns nil for apps without a usable
// icon, which falls back to a generic SF Symbol placeholder.
private struct AppIconView: View {
    let uid: UInt32
    // Pixel size requested from the icon decoder; iconSide/placeholderSide are
    // the on-screen point sizes for the decoded icon and the fallback symbol.
    let decodePx: UInt
    let iconSide: CGFloat
    let placeholderSide: CGFloat

    @State private var icon: UIImage?
    @State private var loadedUID: UInt32?

    var body: some View {
        ZStack {
            if let icon {
                Image(uiImage: icon)
                    .resizable()
                    .interpolation(.high)
                    .frame(width: iconSide, height: iconSide)
            } else {
                Image(systemName: "app.dashed")
                    .resizable()
                    .scaledToFit()
                    .frame(width: placeholderSide, height: placeholderSide)
                    .foregroundColor(.secondary)
            }
        }
        .onAppear(perform: loadIcon)
        .onChange(of: uid) { _ in
            icon = nil
            loadedUID = nil
            loadIcon()
        }
    }

    private func loadIcon() {
        guard loadedUID != uid else { return }
        let uid = self.uid
        loadedUID = uid
        DispatchQueue.global(qos: .userInitiated).async {
            let data = EKA2L1Bridge.iconPNGData(uid: uid, sizePx: decodePx)
            let image = data.flatMap { UIImage(data: $0) }
            DispatchQueue.main.async {
                guard self.uid == uid, self.loadedUID == uid else { return }
                self.icon = image
            }
        }
    }
}

// A large centered icon with the app name beneath, sized to fit the adaptive
// LazyVGrid cells.
struct AppGridCell: View {
    let uid: UInt32
    let name: String

    var body: some View {
        VStack(spacing: 8) {
            AppIconView(uid: uid, decodePx: 144, iconSide: 64, placeholderSide: 48)
                .frame(width: 72, height: 72)

            Text(name)
                .font(.caption)
                .multilineTextAlignment(.center)
                .lineLimit(2)
                .frame(maxWidth: .infinity)
        }
        .padding(.vertical, 8)
    }
}

#Preview {
    ContentView()
}

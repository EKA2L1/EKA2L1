import SwiftUI

// A direct-IP netplay peer (config.yml internet-bluetooth-friends entry).
struct BTNetFriend: Identifiable, Hashable {
    let id = UUID()
    var addr: String
    var port: Int
}

struct SettingsView: View {
    @ObservedObject private var peripheralManager = PeripheralManager.shared
    @State private var mappingTarget: PeripheralManager.Peripheral?
    @AppStorage("ios.showFPSOverlay") private var showFPSOverlay = true

    @State private var audioMasterVolume = 100.0
    @State private var integerScaling = true
    @State private var nearestNeighborFiltering = true
    @State private var hideSystemApps = true
    @State private var useJIT = false
    @State private var availableLanguages: [EKA2L1LanguageItem] = []
    @State private var systemLanguageCode = -1

    // Installed ROMs shown in the Storage section, with swipe-to-delete.
    @State private var installedDevices: [EKA2L1DeviceItem] = []

    // Editable name of the currently-booted device. Committed to
    // device_manager when the settings page closes (mirrors the Android
    // rename dialog, but applied on dismiss). -1 = no device booted yet.
    @State private var currentDeviceIndex = -1
    @State private var deviceName = ""
    @State private var originalDeviceName = ""

    // BT netplay. Mirrors the Android BTNetplaySettingsFragment surface; the
    // bluetooth midman reads these at device boot, so edits apply from the
    // next app launch.
    @State private var btDiscoveryMode = 0
    @State private var btListenPort = 35689
    @State private var btPassword = ""
    @State private var btServerUrl = ""
    @State private var btFriends: [BTNetFriend] = []
    @State private var newFriendAddress = ""
    @State private var newFriendPort = ""
    @State private var storageBytes: UInt64 = 0
    @State private var clearDataMessage: String?
    @State private var showingClearDataConfirmation = false
    // A ROM delete / data wipe stops the emulator loop and takes the session
    // lock, which must not happen on the main thread (see storageBusy use
    // below), so both run off it and gate the section while they do.
    @State private var storageBusy = false

    private var logURL: URL {
        URL(fileURLWithPath: documentsRoot()).appendingPathComponent("data/EKA2L1.log")
    }

    private var storageText: String {
        ByteCountFormatter.string(fromByteCount: Int64(storageBytes), countStyle: .file)
    }

    var body: some View {
        Form {
            Section("settings.device") {
                if currentDeviceIndex >= 0 {
                    HStack {
                        Text("settings.deviceName")
                        Spacer()
                        TextField("settings.deviceName", text: $deviceName)
                            .multilineTextAlignment(.trailing)
                            .foregroundStyle(.secondary)
                            .submitLabel(.done)
                    }
                }
                if !availableLanguages.isEmpty {
                    Picker("settings.systemLanguage", selection: $systemLanguageCode) {
                        ForEach(availableLanguages) { language in
                            Text(language.name).tag(language.code)
                        }
                    }
                }
            }
            // Only sideload/simulator builds carry the dynarmic JIT; App Store /
            // TestFlight builds compile without it and never show this section.
            if EKA2L1Bridge.shared.jitCompiledIn {
                Section {
                    Toggle("settings.jit", isOn: $useJIT)
                } header: {
                    Text("settings.system")
                } footer: {
                    if !EKA2L1Bridge.shared.jitAvailable {
                        Text("settings.jit.unavailable")
                    } else {
                        Text("settings.jit.hint")
                    }
                }
            }
            Section("settings.graphics") {
                Toggle("settings.integerScaling", isOn: $integerScaling)
                Toggle("settings.nearestFiltering", isOn: $nearestNeighborFiltering)
                Toggle("settings.fpsOverlay", isOn: $showFPSOverlay)
            }
            Section("settings.audio") {
                Slider(value: $audioMasterVolume, in: 0...100, step: 1)
                Text(verbatim: "\(Int(audioMasterVolume))%")
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(.secondary)
            }
            Section {
                Picker("settings.netplay.discoveryMode", selection: $btDiscoveryMode) {
                    Text("settings.netplay.mode.off").tag(0)
                    Text("settings.netplay.mode.directIp").tag(1)
                    Text("settings.netplay.mode.lan").tag(2)
                    Text("settings.netplay.mode.server").tag(3)
                }
                // LAN discovery uses its fixed harbour port. Direct IP and a
                // current central server can advertise a configurable port.
                if btDiscoveryMode == 1 || btDiscoveryMode == 3 {
                    LabeledContent("settings.netplay.listenPort") {
                        TextField(String("35689"), value: $btListenPort, format: .number.grouping(.never))
                            .keyboardType(.numberPad)
                            .multilineTextAlignment(.trailing)
                            .frame(maxWidth: 100)
                    }
                }
                // Direct IP peers are configured by hand, so the matching
                // password is only meaningful for the discovery modes that
                // negotiate it.
                if btDiscoveryMode == 2 || btDiscoveryMode == 3 {
                    TextField("settings.netplay.password", text: $btPassword)
                        .autocorrectionDisabled()
                        .textInputAutocapitalization(.never)
                }
                if btDiscoveryMode == 3 {
                    TextField("settings.netplay.serverUrl", text: $btServerUrl)
                        .autocorrectionDisabled()
                        .textInputAutocapitalization(.never)
                        .keyboardType(.URL)
                }
                if btDiscoveryMode == 1 {
                    ForEach(btFriends) { friendEntry in
                        Text(verbatim: "\(friendEntry.addr) : \(String(friendEntry.port))")
                            .font(.callout.monospacedDigit())
                    }
                    .onDelete { offsets in
                        btFriends.remove(atOffsets: offsets)
                        save()
                    }
                    HStack {
                        TextField("settings.netplay.friendAddress", text: $newFriendAddress)
                            .autocorrectionDisabled()
                            .textInputAutocapitalization(.never)
                        TextField("settings.netplay.friendPort", text: $newFriendPort)
                            .keyboardType(.numberPad)
                            .frame(maxWidth: 70)
                        Button {
                            addFriendAddress()
                        } label: {
                            Image(systemName: "plus.circle.fill")
                        }
                        .buttonStyle(.borderless)
                        .disabled(newFriendAddress.trimmingCharacters(in: .whitespaces).isEmpty)
                    }
                }
            } header: {
                Text("settings.netplay")
            } footer: {
                if btDiscoveryMode != 0 {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("settings.netplay.hint")
                        if btDiscoveryMode == 1 {
                            Text("settings.netplay.directIpHint")
                        }
                    }
                }
            }
            Section("settings.peripherals") {
                if peripheralManager.peripherals.isEmpty {
                    Text("controllerMapping.noController")
                        .foregroundStyle(.secondary)
                } else {
                    ForEach(peripheralManager.peripherals) { peripheral in
                        peripheralRow(peripheral)
                    }
                }
            }
            Section("settings.library") {
                Toggle("settings.hideSystemApps", isOn: $hideSystemApps)
            }
            Section("settings.storage") {
                if installedDevices.isEmpty {
                    Text("settings.storage.noRoms")
                        .foregroundStyle(.secondary)
                } else {
                    ForEach(installedDevices) { device in
                        VStack(alignment: .leading, spacing: 2) {
                            Text(device.displayName)
                            Text(device.firmwareCode)
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                    }
                    .onDelete { offsets in
                        deleteROMs(at: offsets)
                    }
                    .deleteDisabled(storageBusy)
                }
                Button(role: .destructive) {
                    showingClearDataConfirmation = true
                } label: {
                    Label {
                        Text("settings.clearData")
                        Text(storageText)
                    } icon: {
                        Image(systemName: "trash")
                    }
                }
                // Attach the confirmation to the button so iOS 26 can present it
                // in its newer button-anchored style.
                .confirmationDialog("settings.clearData.title",
                                    isPresented: $showingClearDataConfirmation,
                                    titleVisibility: .visible) {
                    Button("settings.clearData.confirm", role: .destructive, action: clearData)
                    Button("common.cancel", role: .cancel) {}
                } message: {
                    Text("settings.clearData.message")
                }
                if let clearDataMessage {
                    Text(clearDataMessage)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            Section("settings.support") {
                if FileManager.default.fileExists(atPath: logURL.path) {
                    ShareLink(item: logURL) {
                        Label("settings.exportLog", systemImage: "square.and.arrow.up")
                    }
                } else {
                    Label("settings.noLog", systemImage: "doc")
                        .foregroundStyle(.secondary)
                }
                Link(destination: URL(string: "https://eka2l1.miraheze.org/wiki/Main_Page")!) {
                    Label("settings.wiki", systemImage: "book")
                }
                Link(destination: URL(string: "https://discord.gg/5Bm5SJ9")!) {
                    Label("settings.discord", systemImage: "bubble.left.and.bubble.right")
                }
                Link(destination: URL(string: "https://github.com/EKA2L1/EKA2L1")!) {
                    Label("settings.license", systemImage: "curlybraces")
                }
            }
        }
        .navigationTitle("settings.title")
        .navigationDestination(isPresented: Binding(
            get: { mappingTarget != nil },
            set: { if !$0 { mappingTarget = nil } }
        )) {
            if let mappingTarget {
                KeyMappingView(peripheral: mappingTarget)
            }
        }
        .onAppear {
            load()
            installedDevices = EKA2L1Bridge.shared.installedDevices()
            loadDeviceName()
            refreshStorageUsage()
        }
        .onDisappear {
            commitDeviceRename()
        }
        .onChange(of: audioMasterVolume) { _ in save() }
        .onChange(of: useJIT) { _ in save() }
        .onChange(of: integerScaling) { _ in save() }
        .onChange(of: nearestNeighborFiltering) { _ in save() }
        .onChange(of: hideSystemApps) { _ in save() }
        .onChange(of: btDiscoveryMode) { _ in save() }
        .onChange(of: btListenPort) { _ in save() }
        .onChange(of: btPassword) { _ in save() }
        .onChange(of: btServerUrl) { _ in save() }
        .onChange(of: systemLanguageCode) { newCode in
            // -1 = load() hasn't found a booted device yet; don't write it back.
            if newCode >= 0, newCode != EKA2L1Bridge.shared.currentLanguageCode() {
                EKA2L1Bridge.shared.setSystemLanguage(code: newCode)
                // The bridge drops the applist caption cache; tell the home
                // surface to re-scan so the app names reload in the new language.
                NotificationCenter.default.post(name: .eka2l1AppListInvalidated, object: nil)
            }
        }
    }

    // Connected-peripheral row: tapping the name area makes the device the
    // active input source, the info button opens its key mapping editor.
    // Borderless styles keep the two buttons independently tappable in the
    // same Form row.
    private func peripheralRow(_ peripheral: PeripheralManager.Peripheral) -> some View {
        let isActive = peripheral.id == peripheralManager.activeID
        return HStack {
            Button {
                peripheralManager.setActive(peripheral.id)
            } label: {
                HStack {
                    Image(systemName: isActive ? "checkmark.circle.fill" : "circle")
                        .foregroundStyle(isActive ? Color.accentColor : Color.secondary)
                    Text(peripheral.name)
                        .foregroundStyle(Color.primary)
                }
            }
            .buttonStyle(.borderless)
            Spacer()
            Button {
                mappingTarget = peripheral
            } label: {
                Image(systemName: "info.circle")
            }
            .buttonStyle(.borderless)
        }
    }

    private func load() {
        let snapshot = EKA2L1Bridge.shared.currentConfigSnapshot()
        if let volume = snapshot["audioMasterVolume"] as? NSNumber {
            audioMasterVolume = Double(truncating: volume)
        }
        if let value = snapshot["integerScaling"] as? NSNumber {
            integerScaling = value.boolValue
        }
        if let value = snapshot["nearestNeighborFiltering"] as? NSNumber {
            nearestNeighborFiltering = value.boolValue
        }
        if let value = snapshot["hideSystemApps"] as? NSNumber {
            hideSystemApps = value.boolValue
        }
        if let value = snapshot["jitEnabled"] as? NSNumber {
            useJIT = value.boolValue
        }
        availableLanguages = EKA2L1Bridge.shared.availableLanguages()
        systemLanguageCode = EKA2L1Bridge.shared.currentLanguageCode()
        if let value = snapshot["btnetDiscoveryMode"] as? NSNumber {
            btDiscoveryMode = value.intValue
        }
        if let value = snapshot["btnetListenPort"] as? NSNumber {
            btListenPort = value.intValue
        }
        if let value = snapshot["btnetPassword"] as? String {
            btPassword = value
        }
        if let value = snapshot["btCentralServerUrl"] as? String {
            btServerUrl = value
        }
        if let entries = snapshot["btnetFriendAddresses"] as? [[String: Any]] {
            btFriends = entries.compactMap { entry in
                guard let addr = entry["addr"] as? String,
                      let port = entry["port"] as? NSNumber else { return nil }
                return BTNetFriend(addr: addr, port: port.intValue)
            }
        }
    }

    // Seed the device-name field from the booted device's current title, so
    // the field shows exactly what the home surface displays.
    private func loadDeviceName() {
        currentDeviceIndex = EKA2L1Bridge.shared.currentDeviceIndex()
        let current = installedDevices.first { $0.index == currentDeviceIndex }
        deviceName = current?.displayName ?? ""
        originalDeviceName = deviceName
    }

    // Persist a device rename on page close. Skips no-ops and blank names so a
    // device never ends up with an empty title.
    private func commitDeviceRename() {
        guard currentDeviceIndex >= 0 else { return }
        let trimmed = deviceName.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty, trimmed != originalDeviceName else { return }
        guard EKA2L1Bridge.shared.renameDevice(at: currentDeviceIndex, to: trimmed) else { return }
        originalDeviceName = trimmed
        // Tell the home surface to refresh its title / device switcher. The
        // "renamed" flag keeps it from treating this like a delete (which
        // would reboot a different device).
        NotificationCenter.default.post(name: .eka2l1DevicesChanged,
                                        object: nil,
                                        userInfo: ["renamed": true])
    }

    private func save() {
        let snapshot: [String: Any] = [
            "audioMasterVolume": Int(audioMasterVolume),
            "integerScaling": integerScaling,
            "nearestNeighborFiltering": nearestNeighborFiltering,
            "hideSystemApps": hideSystemApps,
            "jitEnabled": useJIT && EKA2L1Bridge.shared.jitCompiledIn,
            "btnetDiscoveryMode": btDiscoveryMode,
            "btnetListenPort": min(max(btListenPort, 1), 65535),
            "btnetPassword": btPassword,
            "btCentralServerUrl": btServerUrl,
            "btnetFriendAddresses": btFriends.map { ["addr": $0.addr, "port": $0.port] }
        ]
        _ = EKA2L1Bridge.shared.applyConfigSnapshot(snapshot)
    }

    private func addFriendAddress() {
        let addr = newFriendAddress.trimmingCharacters(in: .whitespaces)
        guard !addr.isEmpty else { return }
        // 35689 is the default direct-connect port the Qt frontend seeds too.
        let port = Int(newFriendPort.trimmingCharacters(in: .whitespaces)) ?? 35689
        btFriends.append(BTNetFriend(addr: addr, port: port))
        newFriendAddress = ""
        newFriendPort = ""
        save()
    }

    private func refreshStorageUsage() {
        DispatchQueue.global(qos: .utility).async {
            let bytes = directorySize(at: URL(fileURLWithPath: documentsRoot()))
            DispatchQueue.main.async {
                storageBytes = bytes
            }
        }
    }

    // Swipe-to-delete on the ROM list. Resolve each row back to a live
    // device_manager index at delete time (indices shift as devices are
    // removed), delete the ROM, and tell the home surface to reboot to the
    // previous ROM (or drop to the empty state) via the shared notification.
    //
    // deleteDevice blocks on the emulator's session lock, and the reboot the
    // notification kicks off holds that same lock while it bounces the render
    // layer onto the main queue — running the delete on the main thread would
    // deadlock the two against each other on a second swipe. So the deletes run
    // on a background queue, one notification per device, back on the main
    // queue so the home surface reboots between them.
    private func deleteROMs(at offsets: IndexSet) {
        guard !storageBusy else { return }
        let firmcodes = offsets.map { installedDevices[$0].firmwareCode }
        storageBusy = true
        // Drop the rows now: the list is rebuilt from device_manager once the
        // deletes land, and leaving them until then would let a second swipe
        // target a row that is already on its way out.
        installedDevices.removeAll { firmcodes.contains($0.firmwareCode) }
        DispatchQueue.global(qos: .userInitiated).async {
            for firmcode in firmcodes {
                guard let liveIndex = EKA2L1Bridge.installedDevices()
                    .first(where: { $0.firmwareCode == firmcode })?.index else { continue }
                _ = EKA2L1Bridge.deleteDevice(at: liveIndex)
                DispatchQueue.main.async {
                    NotificationCenter.default.post(name: .eka2l1DevicesChanged,
                                                    object: nil,
                                                    userInfo: ["firmcode": firmcode])
                }
            }
            DispatchQueue.main.async {
                installedDevices = EKA2L1Bridge.shared.installedDevices()
                storageBusy = false
                refreshStorageUsage()
            }
        }
    }

    private func clearData() {
        guard !storageBusy else { return }
        EKA2L1Bridge.shared.pause()
        EKA2L1Bridge.shared.closeRunningApp()
        storageBusy = true
        installedDevices = []
        clearDataMessage = nil
        // resetDevicesState blocks on the session lock and the wipe walks the
        // whole sandbox tree; neither belongs on the main thread (see
        // deleteROMs). Drop the in-memory device list first so the home surface
        // can return to the empty state, then wipe the storage tree (data/
        // holds the drives, config and logs, roms/ the installed ROM images;
        // sis/ and import_tmp/ are staging folders older versions left behind).
        DispatchQueue.global(qos: .userInitiated).async {
            EKA2L1Bridge.resetDevicesState()
            let root = URL(fileURLWithPath: documentsRoot())
            let fm = FileManager.default
            for name in ["data", "sis", "roms", "import_tmp"] {
                try? fm.removeItem(at: root.appendingPathComponent(name))
            }
            DispatchQueue.main.async {
                storageBusy = false
                NotificationCenter.default.post(name: .eka2l1DevicesChanged, object: nil)
                clearDataMessage = String(localized: "settings.clearData.done")
                refreshStorageUsage()
            }
        }
    }
}

private func directorySize(at url: URL) -> UInt64 {
    let keys: Set<URLResourceKey> = [.isRegularFileKey, .fileSizeKey, .totalFileAllocatedSizeKey]
    guard let enumerator = FileManager.default.enumerator(
        at: url,
        includingPropertiesForKeys: Array(keys),
        options: [.skipsHiddenFiles]
    ) else {
        return 0
    }

    var total: UInt64 = 0
    for case let fileURL as URL in enumerator {
        guard let values = try? fileURL.resourceValues(forKeys: keys),
              values.isRegularFile == true else {
            continue
        }
        total += UInt64(values.totalFileAllocatedSize ?? values.fileSize ?? 0)
    }
    return total
}

import SwiftUI

// A direct-IP netplay peer (config.yml internet-bluetooth-friends entry).
struct BTNetFriend: Identifiable, Hashable {
    let id = UUID()
    var addr: String
    var port: Int
}

struct SettingsView: View {
    @Environment(\.openURL) private var openURL
    @ObservedObject private var peripheralManager = PeripheralManager.shared
    @State private var mappingTarget: PeripheralManager.Peripheral?
    @State private var showMailUnavailable = false
    @AppStorage("ios.showFPSOverlay") private var showFPSOverlay = false

    private static let developerEmail = "iyeatse@gmail.com"

    @State private var extensiveLogging = false
    @State private var hideSystemApps = true
    @State private var cpuBackend = "ekalynx"
    @State private var performanceMode = "balanced"
    @State private var availableLanguages: [EKA2L1LanguageItem] = []
    @State private var systemLanguageCode = -1

    @State private var friendlyPhoneName = ""
    @State private var imei = ""
    @State private var mmcId = ""

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

    private var logURL: URL {
        URL(fileURLWithPath: documentsRoot()).appendingPathComponent("data/EKA2L1.log")
    }

    var body: some View {
        Form {
            Section {
                HStack {
                    Text("settings.friendlyPhoneName")
                    Spacer()
                    TextField("settings.friendlyPhoneName", text: $friendlyPhoneName)
                        .multilineTextAlignment(.trailing)
                        .foregroundStyle(.secondary)
                        .submitLabel(.done)
                }
                deviceIdentifierLink(.imei, value: $imei)
                deviceIdentifierLink(.mmcId, value: $mmcId)
                if !availableLanguages.isEmpty {
                    Picker("settings.systemLanguage", selection: $systemLanguageCode) {
                        ForEach(availableLanguages) { language in
                            Text(language.name).tag(language.code)
                        }
                    }
                }
            } header: {
                Text("settings.device")
            }
            Section {
                Picker("settings.performanceMode", selection: $performanceMode) {
                    Text("settings.performanceMode.high").tag("high-performance")
                    Text("settings.performanceMode.balanced").tag("balanced")
                }
                Toggle("settings.hideSystemApps", isOn: $hideSystemApps)
                Picker("settings.cpuBackend", selection: Binding(
                    get: { cpuBackend },
                    set: { backend in
                        let snapshot: [String: Any] = backend == "jit"
                            ? ["jitEnabled": true]
                            : ["cpuBackend": backend, "jitEnabled": false]
                        if EKA2L1Bridge.shared.applyConfigSnapshot(snapshot) {
                            cpuBackend = backend
                        }
                    }
                )) {
                    Text(verbatim: "Dyncom").tag("dyncom")
                    Text("settings.interpreter.ekalynx").tag("ekalynx")
                    if EKA2L1Bridge.shared.jitCompiledIn {
                        Text("settings.jit").tag("jit")
                    }
                }
                .accessibilityIdentifier("settings.cpuBackend")
            } header: {
                Text("settings.system")
            } footer: {
                if cpuBackend == "jit" {
                    if !EKA2L1Bridge.shared.jitAvailable {
                        Text("settings.jit.unavailable")
                    } else {
                        Text("settings.jit.hint")
                    }
                }
            }
            Section {
                Text("settings.airplay.hint")
                    .foregroundStyle(.secondary)
            } header: {
                Text(verbatim: "AirPlay")
            }
            Section("settings.network") {
                NavigationLink("settings.hosts.title") {
                    HostOverridesView()
                }
            }
            Section {
                Picker("settings.netplay.discoveryMode", selection: $btDiscoveryMode) {
                    Text("settings.netplay.mode.off").tag(0)
                    Text("settings.netplay.mode.directIp").tag(1)
                    Text("settings.netplay.mode.lan").tag(2)
                    Text("settings.netplay.mode.server").tag(3)
                }
                // LAN discovery advertises its port over mDNS. Direct IP
                // and a current central server can use a configurable port.
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
            Section("settings.diagnostics") {
                Toggle("settings.fpsOverlay", isOn: $showFPSOverlay)
                Toggle("settings.extensiveLogging", isOn: $extensiveLogging)
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
                Button(action: contactDeveloper) {
                    Label("settings.contactDeveloper", systemImage: "envelope")
                }
                Link(destination: URL(string: "https://github.com/EKA2L1/EKA2L1")!) {
                    Label("settings.license", systemImage: "curlybraces")
                }
            }
        }
        .navigationTitle("settings.title")
        .alert("settings.mailUnavailable", isPresented: $showMailUnavailable) {
            Button("common.ok", role: .cancel) {}
        } message: {
            Text(verbatim: Self.developerEmail)
        }
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
        }
        .onChange(of: friendlyPhoneName) { _ in save() }
        .onChange(of: performanceMode) { _ in save() }
        .onChange(of: extensiveLogging) { _ in save() }
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

    private func deviceIdentifierLink(_ field: DeviceIdentifierEditor.Field, value: Binding<String>) -> some View {
        NavigationLink {
            DeviceIdentifierEditor(field: field, value: value)
        } label: {
            LabeledContent {
                Text(verbatim: value.wrappedValue)
                    .foregroundStyle(.secondary)
                    .monospacedDigit()
                    .lineLimit(1)
                    .truncationMode(.middle)
                    .frame(maxWidth: 200, alignment: .trailing)
            } label: {
                Text(verbatim: field.title)
            }
        }
    }

    private func contactDeveloper() {
        let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "?"
        let build = Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "?"
        var info = utsname()
        uname(&info)
        let hardware = withUnsafeBytes(of: info.machine) { bytes in
            String(decoding: bytes.prefix(while: { $0 != 0 }), as: UTF8.self)
        }
        let device = ProcessInfo.processInfo.environment["SIMULATOR_MODEL_IDENTIFIER"] ?? hardware
        let body = [
            "", "", "---",
            "App Version: \(version) (\(build))",
            "Device: \(device)",
            "System: iOS \(UIDevice.current.systemVersion)"
        ].joined(separator: "\r\n")

        var draft = URLComponents()
        draft.scheme = "mailto"
        draft.path = Self.developerEmail
        draft.queryItems = [
            URLQueryItem(name: "subject", value: String(localized: "settings.feedbackSubject")),
            URLQueryItem(name: "body", value: body)
        ]
        guard let url = draft.url else {
            showMailUnavailable = true
            return
        }
        openURL(url) { accepted in
            showMailUnavailable = !accepted
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
        if let value = snapshot["extensiveLogging"] as? NSNumber {
            extensiveLogging = value.boolValue
        }
        if let value = snapshot["hideSystemApps"] as? NSNumber {
            hideSystemApps = value.boolValue
        }
        if EKA2L1Bridge.shared.jitCompiledIn, (snapshot["jitEnabled"] as? NSNumber)?.boolValue == true {
            cpuBackend = "jit"
        } else {
            switch (snapshot["cpuBackend"] as? String)?.lowercased() {
            case "dyncom": cpuBackend = "dyncom"
            default: cpuBackend = "ekalynx"
            }
        }
        // Anything the bridge doesn't recognise runs as balanced.
        performanceMode = snapshot["performanceMode"] as? String == "high-performance" ? "high-performance" : "balanced"
        friendlyPhoneName = snapshot["deviceDisplayName"] as? String ?? ""
        imei = snapshot["imei"] as? String ?? ""
        mmcId = snapshot["mmcId"] as? String ?? "00000000-00000000-00000000-00000000"
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

    private func save() {
        let snapshot: [String: Any] = [
            "deviceDisplayName": friendlyPhoneName,
            "extensiveLogging": extensiveLogging,
            "hideSystemApps": hideSystemApps,
            "performanceMode": performanceMode,
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
}

private struct DeviceIdentifierEditor: View {
    enum Field: String {
        case imei, mmcId

        var title: String {
            self == .imei ? "IMEI" : "MMC ID"
        }
    }

    @Environment(\.dismiss) private var dismiss
    @Binding private var value: String
    @State private var draft: String
    let field: Field

    init(field: Field, value: Binding<String>) {
        self.field = field
        _value = value
        _draft = State(initialValue: value.wrappedValue)
    }

    private var input: String? {
        switch field {
        case .imei:
            // UIQ registration can require a specific serial despite a checksum warning.
            return draft.isEmpty ? nil : draft
        case .mmcId:
            let trimmed = draft.trimmingCharacters(in: .whitespacesAndNewlines)
            guard trimmed.range(of: "^([0-9a-fA-F]{32}|[0-9a-fA-F]{8}(-[0-9a-fA-F]{8}){3})$",
                                options: .regularExpression) != nil else { return nil }
            return trimmed
        }
    }

    var body: some View {
        Form {
            Section {
                TextField(field.title, text: $draft)
                    .font(field == .imei ? .body.monospaced() : .subheadline.monospaced())
                    .keyboardType(field == .imei ? .numberPad : .asciiCapable)
                    .autocorrectionDisabled()
                    .textInputAutocapitalization(.never)
                    .submitLabel(.done)
                    .onSubmit(save)
                    .accessibilityLabel(Text(verbatim: field.title))
                    .accessibilityIdentifier("settings.\(field.rawValue)")
                    .onChange(of: draft) { newValue in
                        guard field == .imei else { return }
                        let digits = String(newValue.filter { $0.isASCII && $0.isNumber }.prefix(15))
                        if digits != newValue {
                            draft = digits
                        }
                    }
            } footer: {
                VStack(alignment: .leading, spacing: 4) {
                    switch field {
                    case .imei:
                        Text("settings.imei.hint")
                        switch EKA2L1Bridge.shared.validateIMEI(draft) {
                        case 0:
                            EmptyView()
                        case -1:
                            Text("settings.imei.invalidLength")
                        default:
                            Text("settings.imei.invalidChecksum")
                        }
                    case .mmcId:
                        Text("settings.mmcId.hint")
                        if input == nil {
                            Text("settings.mmcId.invalid")
                                .foregroundStyle(.red)
                        }
                    }
                }
            }
        }
        .navigationTitle(Text(verbatim: field.title))
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                Button("common.done", action: save)
                    .disabled(input == nil)
            }
        }
    }

    private func save() {
        guard let input else { return }
        if input != value {
            guard EKA2L1Bridge.shared.applyConfigSnapshot([field.rawValue: input]) else { return }
            value = EKA2L1Bridge.shared.currentConfigSnapshot()[field.rawValue] as? String ?? input
        }
        dismiss()
    }
}

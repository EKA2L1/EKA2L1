import Network
import SwiftUI

private struct HostOverride: Identifiable {
    let id: UUID
    var hostname: String
    var address: String

    init(id: UUID = UUID(), hostname: String = "", address: String = "") {
        self.id = id
        self.hostname = hostname
        self.address = address
    }
}

struct HostOverridesView: View {
    @State private var entries: [HostOverride] = []
    @State private var editing: HostOverride?
    @State private var saveFailed = false

    var body: some View {
        List {
            Section {
                ForEach(entries) { entry in
                    Button {
                        editing = entry
                    } label: {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(verbatim: entry.hostname)
                                .foregroundStyle(Color.primary)
                            Text(verbatim: entry.address)
                                .font(.callout.monospaced())
                                .foregroundStyle(Color.secondary)
                        }
                    }
                }
                .onDelete { offsets in
                    var updated = entries
                    updated.remove(atOffsets: offsets)
                    saveFailed = !save(updated)
                }
                Button {
                    editing = HostOverride()
                } label: {
                    Label("settings.hosts.add", systemImage: "plus")
                }
            } footer: {
                Text("settings.hosts.hint")
            }
        }
        .navigationTitle("settings.hosts.title")
        .onAppear {
            let hosts = EKA2L1Bridge.shared.currentConfigSnapshot()["hosts"] as? [String: String] ?? [:]
            entries = hosts.map { HostOverride(hostname: $0.key, address: $0.value) }
                .sorted { $0.hostname < $1.hostname }
        }
        .sheet(item: $editing) { entry in
            HostOverrideEditor(entry: entry, existing: entries) { updated in
                let remaining = entries.filter { $0.id != updated.id }
                return save(remaining + [updated])
            }
        }
        .alert("common.error", isPresented: $saveFailed) {
            Button("common.ok", role: .cancel) {}
        } message: {
            Text("settings.hosts.saveError")
        }
    }

    private func save(_ updated: [HostOverride]) -> Bool {
        let hosts = Dictionary(uniqueKeysWithValues: updated.map { ($0.hostname, $0.address) })
        guard EKA2L1Bridge.shared.applyConfigSnapshot(["hosts": hosts]) else {
            return false
        }
        entries = updated.sorted { $0.hostname < $1.hostname }
        return true
    }
}

private struct HostOverrideEditor: View {
    @Environment(\.dismiss) private var dismiss
    @State var entry: HostOverride
    @State private var saveFailed = false
    let existing: [HostOverride]
    let save: (HostOverride) -> Bool

    private var hostname: String {
        Self.normalize(entry.hostname)
    }

    private var address: String {
        Self.normalize(entry.address)
    }

    private var validationError: LocalizedStringKey? {
        if !Self.validHostPattern(hostname) {
            return "settings.hosts.invalidHostname"
        }
        if existing.contains(where: { $0.id != entry.id && Self.normalize($0.hostname) == hostname }) {
            return "settings.hosts.duplicate"
        }
        if !Self.validTarget(address) {
            return "settings.hosts.invalidAddress"
        }
        return nil
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("settings.hosts.hostname", text: $entry.hostname)
                        .keyboardType(.URL)
                        .accessibilityLabel(Text("settings.hosts.hostname"))
                        .accessibilityIdentifier("hosts.hostname")
                    TextField("settings.hosts.address", text: $entry.address)
                        .keyboardType(.asciiCapable)
                        .accessibilityLabel(Text("settings.hosts.address"))
                        .accessibilityIdentifier("hosts.address")
                } footer: {
                    if let validationError, !entry.hostname.isEmpty || !entry.address.isEmpty {
                        Text(validationError)
                            .foregroundStyle(.red)
                    } else {
                        Text("settings.hosts.addressHint")
                    }
                }
                .autocorrectionDisabled()
                .textInputAutocapitalization(.never)
            }
            .navigationTitle("settings.hosts.entry")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("common.cancel") { dismiss() }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("common.done") {
                        if save(HostOverride(id: entry.id, hostname: hostname, address: address)) {
                            dismiss()
                        } else {
                            saveFailed = true
                        }
                    }
                    .disabled(validationError != nil)
                }
            }
            .alert("common.error", isPresented: $saveFailed) {
                Button("common.ok", role: .cancel) {}
            } message: {
                Text("settings.hosts.saveError")
            }
        }
    }

    private static func validHostname(_ hostname: String) -> Bool {
        let allowed = CharacterSet(charactersIn: "abcdefghijklmnopqrstuvwxyz0123456789-_")
        let labels = hostname.split(separator: ".", omittingEmptySubsequences: false)
        return !hostname.isEmpty && hostname.utf8.count <= 253 && labels.allSatisfy {
            !$0.isEmpty && $0.utf8.count <= 63 && $0.unicodeScalars.allSatisfy(allowed.contains)
        }
    }

    private static func validHostPattern(_ hostname: String) -> Bool {
        if hostname.hasPrefix("*.") {
            return validHostname(String(hostname.dropFirst(2)))
        }
        return validHostname(hostname)
    }

    private static func validTarget(_ target: String) -> Bool {
        var hostname = target
        var port: String?
        if target.hasPrefix("[") {
            guard let end = target.firstIndex(of: "]"), end != target.index(after: target.startIndex),
                  target.index(after: end) < target.endIndex,
                  target[target.index(after: end)] == ":" else { return false }
            hostname = String(target[target.index(after: target.startIndex)..<end])
            port = String(target[target.index(end, offsetBy: 2)...])
        } else if IPv6Address(target) == nil, let colon = target.lastIndex(of: ":") {
            hostname = String(target[..<colon])
            port = String(target[target.index(after: colon)...])
        }
        if let port {
            guard !port.isEmpty, port.allSatisfy(\.isNumber), let value = UInt16(port), value != 0 else { return false }
        }
        return IPv4Address(hostname) != nil || IPv6Address(hostname) != nil
            || (validHostname(hostname) && !hostname.allSatisfy({ "0123456789.".contains($0) }))
    }

    private static func normalize(_ value: String) -> String {
        var host = value.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        if host.hasSuffix(".") { host.removeLast() }
        return host
    }
}

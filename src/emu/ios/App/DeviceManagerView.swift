import SwiftUI

// Device management surface, pushed from the home title menu ("Manage
// devices"): switch to a device by tapping it, add one through the shared
// import sheet, remove one by swiping, or rebuild the list from drive Z
// ("Rescan devices", mirroring the Android device-list screen).
//
// The page holds no device state of its own: it observes the same DeviceStore
// the home surface owns and drives it with the same operations the title menu
// uses, so the two surfaces can never disagree about which devices exist or
// which one is running. Installing is the exception — the import sheet is
// presented by the home surface, so that one stays a callback.
struct DeviceManagerView: View {
    @ObservedObject var store: DeviceStore
    let onInstall: () -> Void

    var body: some View {
        List {
            Section {
                if store.devices.isEmpty {
                    Text("devices.none")
                        .foregroundStyle(.secondary)
                } else {
                    ForEach(store.devices) { device in
                        deviceRow(device)
                    }
                    .onDelete { offsets in
                        Task { await store.deleteDevices(at: offsets) }
                    }
                    .deleteDisabled(store.busy)
                }
            } header: {
                Text("devices.installed")
            } footer: {
                if store.busy {
                    HStack(spacing: 8) {
                        ProgressView().controlSize(.small)
                        Text("devices.working")
                    }
                } else if !store.devices.isEmpty {
                    Text("devices.hint")
                }
            }

            Section {
                Button(action: onInstall) {
                    Label("import.title", systemImage: "square.and.arrow.down")
                }
                .disabled(store.busy)

                Button {
                    Task { await store.rescanDevices() }
                } label: {
                    Label("device.rescan", systemImage: "arrow.clockwise")
                }
                .disabled(store.busy)
            } footer: {
                Text("devices.rescanHint")
            }
        }
        .navigationTitle("devices.title")
        .navigationBarTitleDisplayMode(.inline)
    }

    // Tapping a row boots that device. The plain button style keeps the row
    // looking like a list row rather than tinted text, and the content shape
    // makes the whole row (including the gap before the checkmark) tappable.
    private func deviceRow(_ device: EKA2L1DeviceItem) -> some View {
        Button {
            Task { await store.switchDevice(to: device.index) }
        } label: {
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text(device.displayName)
                    Text(device.firmwareCode)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                Spacer()
                if device.index == store.currentIndex {
                    Image(systemName: "checkmark")
                        .foregroundStyle(Color.accentColor)
                }
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(store.busy)
    }
}

import SwiftUI

// Emulator session state shared by every frontend surface that shows or
// changes what is running: the installed devices, which one is booted, its app
// list, and whether a long emulator operation is in flight.
//
// ContentView owns it as a @StateObject and the device-manager page observes
// the same instance, so the home title menu and the management list can never
// disagree about the device set or the booted device. Every heavy call
// (boot, delete, rescan, package install) hops off the main thread — they take
// the emulator's session lock and stop the os loop — while the published state
// is only ever written here, on the main actor.
@MainActor
final class DeviceStore: ObservableObject {
    @Published private(set) var devices: [EKA2L1DeviceItem] = []
    @Published private(set) var currentIndex = -1
    @Published private(set) var apps: [EKA2L1AppItem] = []
    @Published private(set) var busy = false

    var currentDevice: EKA2L1DeviceItem? {
        devices.first { $0.index == currentIndex } ?? devices.first
    }

    func device(withFirmwareCode code: String) -> EKA2L1DeviceItem? {
        devices.first { $0.firmwareCode.caseInsensitiveCompare(code) == .orderedSame }
    }

    // Full re-read after boot, used once the emulator is up.
    func refresh() {
        devices = EKA2L1Bridge.shared.installedDevices()
        currentIndex = EKA2L1Bridge.shared.currentDeviceIndex()
        reloadApps()
    }

    // Re-scan the booted device's registry (after a package install/uninstall
    // or a system-language switch).
    func reloadApps() {
        apps = currentIndex >= 0 ? EKA2L1Bridge.shared.rescanApps() : []
    }

    // Titles only (a rename in Settings): the device set and booted device are
    // unchanged, so neither a reboot nor an app re-scan is needed.
    func reloadDevices() {
        devices = EKA2L1Bridge.shared.installedDevices()
    }

    // Run a blocking emulator operation off the main thread with the busy flag
    // up. Shared by the device operations below and by the home surface's
    // package installs, so one flag drives every spinner and disabled control.
    @discardableResult
    func perform<T: Sendable>(_ work: @escaping @Sendable () -> T) async -> T {
        busy = true
        defer { busy = false }
        return await Task.detached(priority: .userInitiated, operation: work).value
    }

    // Boot a device by device_manager index, adopting it as the current one.
    @discardableResult
    func boot(at index: Int) async -> Bool {
        let ok = await perform { EKA2L1Bridge.bootDevice(at: index) }
        if ok {
            currentIndex = index
            reloadApps()
        }
        return ok
    }

    // Device switcher, shared by the home title menu and the management list.
    @discardableResult
    func switchDevice(to index: Int) async -> Bool {
        guard index != currentIndex, !busy else { return false }
        return await boot(at: index)
    }

    // Called after a successful device install. installedDevices() appends the
    // newly-added device last, so boot that one.
    @discardableResult
    func bootNewestDevice() async -> Bool {
        reloadDevices()
        guard let newest = devices.last else { return false }
        return await boot(at: newest.index)
    }

    // Mirrors the Android device-list screen's "Rescan devices" action: rebuild
    // device_manager from what's on drive Z (recovers devices dropped from
    // devices.yml), then boot the resulting current device (always index 0
    // when the scan finds anything).
    func rescanDevices() async {
        guard !busy else { return }
        let bootedOK = await perform {
            EKA2L1Bridge.rescanDevices() && EKA2L1Bridge.bootDevice(at: 0)
        }
        devices = EKA2L1Bridge.shared.installedDevices()
        if bootedOK {
            currentIndex = 0
            reloadApps()
        } else if devices.isEmpty {
            currentIndex = -1
            apps = []
        }
    }

    // Swipe-to-delete on the management list. Each row is resolved back to a
    // live device_manager index at delete time (indices shift as devices are
    // removed), then the list is re-synced: reboot to the surviving device when
    // the running one was the one deleted, or drop to the empty state when
    // nothing is left.
    func deleteDevices(at offsets: IndexSet) async {
        guard !busy else { return }
        let firmcodes = offsets.map { devices[$0].firmwareCode }
        let deletedCurrent = firmcodes.contains { code in
            currentDevice?.firmwareCode.caseInsensitiveCompare(code) == .orderedSame
        }
        // The deleted device's position, needed to pick its replacement below.
        let previousIndex = currentIndex
        // Drop the rows now so no surface keeps offering a device that is on
        // its way out.
        devices.removeAll { firmcodes.contains($0.firmwareCode) }

        await perform {
            for firmcode in firmcodes {
                guard let liveIndex = EKA2L1Bridge.installedDevices()
                    .first(where: { $0.firmwareCode == firmcode })?.index else { continue }
                _ = EKA2L1Bridge.deleteDevice(at: liveIndex)
            }
        }

        devices = EKA2L1Bridge.shared.installedDevices()
        if devices.isEmpty {
            currentIndex = -1
            apps = []
            return
        }
        guard deletedCurrent else {
            // The booted device is unchanged; only indices shifted. Re-sync
            // from device_manager's adjusted current.
            currentIndex = EKA2L1Bridge.shared.currentDeviceIndex()
            reloadApps()
            return
        }
        // The booted device was deleted: fall back to the previous device in
        // the list (clamped into range), so repeated deletes walk backwards
        // until the list is empty.
        await boot(at: min(max(0, previousIndex - 1), devices.count - 1))
    }
}

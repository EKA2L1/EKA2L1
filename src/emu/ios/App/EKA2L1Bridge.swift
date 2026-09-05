import Foundation
import QuartzCore

// Sandbox Documents directory hosting the emulator's file tree (roms/, data/,
// sis/, ...). Shared by every view that stages files or reads emulator output.
func documentsRoot() -> String {
    NSSearchPathForDirectoriesInDomains(.documentDirectory, .userDomainMask, true).first ?? NSHomeDirectory()
}

struct EKA2L1AppItem: Identifiable, Hashable {
    let uid: UInt32
    let name: String
    // True for built-in ROM/system apps; false for user-installed packages.
    let system: Bool

    var id: UInt32 { uid }
}

struct EKA2L1DeviceItem: Identifiable, Hashable {
    let index: Int
    let firmwareCode: String
    let manufacturer: String
    let model: String

    var id: Int { index }

    // Title shown on the app list / device switcher. Prefer the model
    // (e.g. "Nokia N97") and fall back to the firmware code.
    var displayName: String {
        let trimmed = model.trimmingCharacters(in: .whitespaces)
        return trimmed.isEmpty ? firmwareCode : trimmed
    }
}

struct EKA2L1NGageInstallItem {
    let result: Int
    let gameName: String

    var succeeded: Bool { result == 0 }
}

// A guest system language shipped by the current device's ROM.
struct EKA2L1LanguageItem: Identifiable, Hashable {
    let code: Int
    let name: String

    var id: Int { code }
}

// Cross-view frontend signals. Posted by the settings page when it mutates
// emulator state the home surface owns, so ContentView can refresh without a
// shared store: the app list after a system-language switch, and the device
// list after a device rename. (The device-manager page needs neither — it runs
// on the home surface's own state and actions.)
extension Notification.Name {
    static let eka2l1AppListInvalidated = Notification.Name("eka2l1AppListInvalidated")
    static let eka2l1DevicesChanged = Notification.Name("eka2l1DevicesChanged")
}

@MainActor
final class EKA2L1Bridge {
    static let shared = EKA2L1Bridge()

    private let emulator = EKA2L1Emulator.shared()

    private init() {}

    func start(documentsPath: String) -> Bool {
        emulator.start(withDocumentsPath: documentsPath)
    }

    func installedDevices() -> [EKA2L1DeviceItem] {
        Self.installedDevices()
    }

    // Reading the device list only takes device_manager's own lock, so the
    // background paths that delete ROMs can resolve live indices without
    // hopping to the main queue.
    nonisolated static func installedDevices() -> [EKA2L1DeviceItem] {
        EKA2L1Emulator.shared().installedDevices().map {
            EKA2L1DeviceItem(index: Int($0.index), firmwareCode: $0.firmwareCode,
                             manufacturer: $0.manufacturer, model: $0.model)
        }
    }

    func currentDeviceIndex() -> Int {
        emulator.currentDeviceIndex()
    }

    // Rename an installed device (updates its model + devices.yml). The home
    // surface refreshes its title / device list off the eka2l1DevicesChanged
    // notification the caller posts on success.
    @discardableResult
    func renameDevice(at index: Int, to name: String) -> Bool {
        emulator.renameDevice(at: UInt(index), to: name)
    }

    func availableLanguages() -> [EKA2L1LanguageItem] {
        emulator.availableLanguages().map {
            EKA2L1LanguageItem(code: Int($0.code), name: $0.name)
        }
    }

    func currentLanguageCode() -> Int {
        emulator.currentLanguageCode()
    }

    func setSystemLanguage(code: Int) {
        emulator.setSystemLanguageCode(code)
    }

    // Heavy operations (ROM dump / system rebuild) — exposed as nonisolated so
    // the frontend can run them off the main queue while a spinner shows. The
    // Obj-C side serialises against the emulator loop internally.
    // `progress` (0…1) and `cancelCheck` are both called on this thread while
    // the install runs — see installDeviceWithRomPath:rpkgPath:progress:cancelCheck:.
    nonisolated static func installDevice(romPath: String, rpkgPath: String?,
                                          progress: (@Sendable (Double) -> Void)? = nil,
                                          cancelCheck: (@Sendable () -> Bool)? = nil) -> EKA2L1InstallResult {
        EKA2L1Emulator.shared().installDevice(romPath: romPath, rpkgPath: rpkgPath,
                                              progress: progress, cancelCheck: cancelCheck)
    }

    // Same, from a .7z holding either a ROM/RPKG pair or an already-unpacked
    // device — see installDeviceWithArchivePath:progress:cancelCheck:.
    nonisolated static func installDevice(archivePath: String,
                                          progress: (@Sendable (Double) -> Void)? = nil,
                                          cancelCheck: (@Sendable () -> Bool)? = nil) -> EKA2L1InstallResult {
        EKA2L1Emulator.shared().installDevice(archivePath: archivePath,
                                              progress: progress, cancelCheck: cancelCheck)
    }

    nonisolated static func bootDevice(at index: Int) -> Bool {
        EKA2L1Emulator.shared().bootDevice(at: UInt(index))
    }

    // Firmware code of the device the previous run of the app died on while
    // booting (a corrupt ROM/RPKG only fails there, and it fails hard). Reading
    // it clears it, so the home screen warns about it once per launch.
    func takeFailedBootDeviceCode() -> String? {
        emulator.takeFailedBootDeviceCode()
    }

    // Remove an installed device (ROM) from devices.yml and delete its files.
    // Does not reboot; the caller decides which device to boot next.
    nonisolated static func deleteDevice(at index: Int) -> Bool {
        EKA2L1Emulator.shared().deleteDevice(at: UInt(index))
    }

    // Rebuild the device list from what's on drive Z (recovers devices dropped
    // from devices.yml). Does not reboot; the caller boots the resulting
    // current device (index 0) when this returns true, mirroring installDevice.
    nonisolated static func rescanDevices() -> Bool {
        EKA2L1Emulator.shared().rescanDevices()
    }

    func rescanApps() -> [EKA2L1AppItem] {
        let apps = emulator.rescanApps().map {
            EKA2L1AppItem(uid: $0.uid, name: $0.name, system: $0.system)
        }
        // Asking for the app list is the frontend's last step after a device
        // boot, so it doubles as the boot's sign-off (see confirmDeviceBoot).
        // It sits here rather than inside rescanApps so a scan that had to hand
        // back its cached list — the session lock is busy decoding icons often
        // enough — still confirms: the device is up either way.
        emulator.confirmDeviceBoot()
        return apps
    }

    // Launch runs off the main thread inside the bridge (it drives synchronous
    // graphics commands that would deadlock a main-thread caller against the
    // graphics worker's main-queue CAEAGLLayer attach). The completion, when
    // provided, is delivered back on the main queue with the launch result.
    func launchApp(uid: UInt32, completion: ((Bool) -> Void)? = nil) {
        emulator.launchApp(withUID: uid, completion: completion)
    }

    // Set/clear the callback fired (on the main queue) when the running app's
    // process exits — used to close the emulator screen when the guest app
    // leaves via Exit soft key, panic, or normal termination.
    func setAppExitHandler(_ handler: ((String?) -> Void)?) {
        emulator.appExitHandler = handler
    }

    // Kill the running app in lockstep with the frontend closing its screen.
    func closeRunningApp() {
        emulator.closeRunningApp()
    }

    // SIS extraction can take a while for large packages, so — like the other
    // heavy operations — it is called off the main queue; the Obj-C side
    // serialises against the emulator loop itself.
    nonisolated static func installSis(atPath path: String) -> Bool {
        EKA2L1Emulator.shared().installSis(atPath: path)
    }

    nonisolated static func installNGageGame(cardPath: String) -> EKA2L1NGageInstallItem {
        let report = EKA2L1Emulator.shared().installNGageGame(atPath: cardPath)
        return EKA2L1NGageInstallItem(result: report.result, gameName: report.gameName)
    }

    func uninstallApp(uid: UInt32) -> Bool {
        emulator.uninstallApp(withUID: uid)
    }

    func attach(layer: CAEAGLLayer, pixelSize: CGSize, scale: CGFloat) {
        emulator.attach(layer: layer, pixelSize: pixelSize, scale: scale)
    }

    func detachLayer() {
        emulator.detachLayer()
    }

    func pause() {
        emulator.pause()
    }

    func resume() {
        emulator.resume()
    }

    func guestScreenModeSnapshot() -> (modes: [Int], current: Int) {
        let snapshot = emulator.guestScreenModeSnapshot()
        let modes = (snapshot["modes"] as? [NSNumber])?.map(\.intValue) ?? []
        let current = (snapshot["current"] as? NSNumber)?.intValue ?? -1
        return (modes, current)
    }

    func setGuestScreenMode(appUID: UInt32, mode: Int, completion: @escaping @Sendable (Int) -> Void) {
        emulator.setGuestScreenMode(forAppUID: appUID, mode: mode, completion: completion)
    }

    // Per-app guest frame-rate cap: 15 / 30 / 60, or 0 for unlimited.
    func setGuestFrameLimit(appUID: UInt32, limit: Int) {
        emulator.setGuestFrameLimit(forAppUID: appUID, limit: limit)
    }

    func guestFrameLimit(appUID: UInt32) -> Int {
        emulator.guestFrameLimit(forAppUID: appUID)
    }

    func submitPointer(x: CGFloat, y: CGFloat, phase: EKA2L1PointerPhase, pointerId: UInt) {
        emulator.submitPointer(x: x, y: y, phase: phase, pointerId: pointerId)
    }

    // True when the booted ROM drives its UI by touch (S60v5 / Symbian^3+);
    // those default to the fullscreen keypad layout.
    func currentDeviceIsTouchScreen() -> Bool {
        emulator.currentDeviceIsTouchScreen()
    }

    // Anchor the presented guest picture's top edge at `pixels` from the top of
    // the render surface (negative = centred). Used to keep the picture clear
    // of a bottom keypad overlay.
    func setDisplayAnchorTop(pixels: Int) {
        emulator.setDisplayAnchorTopPixels(pixels)
    }

    // Report the emulator screen's interface orientation so accelerometer
    // samples can follow the displayed guest (see IosEmulator.h).
    func setHostInterfaceRotation(degrees: Int) {
        emulator.setHostInterfaceRotationDegrees(degrees)
    }

    func submitRawKey(_ scanCode: UInt32, pressed: Bool) {
        emulator.submitRawKey(scanCode, pressed: pressed)
    }

    nonisolated static func submitRawKey(_ scanCode: UInt32, pressed: Bool) {
        EKA2L1Emulator.shared().submitRawKey(scanCode, pressed: pressed)
    }

    func tapRawKey(_ scanCode: UInt32) {
        emulator.tapRawKey(scanCode)
    }

    func currentConfigSnapshot() -> [String: Any] {
        emulator.currentConfigSnapshot()
    }

    func applyConfigSnapshot(_ snapshot: [String: Any]) -> Bool {
        emulator.applyConfigSnapshot(snapshot)
    }

    // YES when this build carries the dynarmic JIT (sideload/simulator builds
    // only; App Store / TestFlight builds compile without it).
    var jitCompiledIn: Bool {
        emulator.jitCompiledIn
    }

    // YES when the running process additionally has JIT permission (debugger /
    // JIT enabler). Without it the emulator falls back to the interpreter.
    var jitAvailable: Bool {
        emulator.jitAvailable
    }

    func renderedFrameCount() -> UInt64 {
        emulator.renderedFrameCount()
    }

    nonisolated static func iconPNGData(uid: UInt32, sizePx: UInt) -> Data? {
        EKA2L1Emulator.shared().iconPNGData(forUID: uid, sizePx: sizePx)
    }
}

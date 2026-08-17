import GameController
import SwiftUI
import UIKit

// Peripheral (game controller / hardware keyboard) management and per-device
// key mapping.
//
// PeripheralManager tracks every connected extended gamepad plus the hardware
// keyboard (a single entry — individual keyboards are not distinguished) and
// which one is *active*. Only the active peripheral drives emulator input;
// the most recently connected device becomes active automatically and the
// user can switch in Settings. There is no wireless discovery scan: paired
// controllers surface through the regular GameController notifications.
//
// Key mappings are per device. Storage is a nested UserDefaults dict
// [device key: [host token: guest scan]] — keyboards share the "keyboard"
// device key, controllers use their vendor name so the same model keeps its
// mapping across reconnects. Within a device the relation is one-to-one:
// a host input drives at most one guest key (re-capturing steals it, last
// edit wins) and each guest key holds at most one binding.
//
// Host tokens: controller buttons use HostButton raw values; keyboard keys
// use "kb.<HID usage>", the raw value GCKeyCode carries at both capture and
// runtime input time.

// Every bindable controller button on an extended gamepad. The left
// thumbstick is deliberately not listed: it aliases the d-pad tokens at both
// capture and input time, so a stick flick and a d-pad press are the same
// binding.
enum HostButton: String, CaseIterable {
    case buttonA, buttonB, buttonX, buttonY
    case leftShoulder, rightShoulder, leftTrigger, rightTrigger
    case buttonMenu, buttonOptions
    case leftThumbstickButton, rightThumbstickButton
    case dpadUp = "dpad.up"
    case dpadDown = "dpad.down"
    case dpadLeft = "dpad.left"
    case dpadRight = "dpad.right"

    var genericName: String {
        switch self {
        case .buttonA: return "A"
        case .buttonB: return "B"
        case .buttonX: return "X"
        case .buttonY: return "Y"
        case .leftShoulder: return "L1"
        case .rightShoulder: return "R1"
        case .leftTrigger: return "L2"
        case .rightTrigger: return "R2"
        case .buttonMenu: return "Menu"
        case .buttonOptions: return "Options"
        case .leftThumbstickButton: return "L3"
        case .rightThumbstickButton: return "R3"
        case .dpadUp: return "D-pad Up"
        case .dpadDown: return "D-pad Down"
        case .dpadLeft: return "D-pad Left"
        case .dpadRight: return "D-pad Right"
        }
    }

    // Controller-specific name when a gamepad is available (e.g. "Cross
    // Button" on DualSense, "A Button" on Xbox), generic name otherwise.
    func displayName(on gamepad: GCExtendedGamepad?) -> String {
        guard let gamepad, let name = buttonInput(on: gamepad)?.localizedName,
              !name.isEmpty else {
            return genericName
        }
        return name
    }

    func buttonInput(on gamepad: GCExtendedGamepad) -> GCControllerButtonInput? {
        switch self {
        case .buttonA: return gamepad.buttonA
        case .buttonB: return gamepad.buttonB
        case .buttonX: return gamepad.buttonX
        case .buttonY: return gamepad.buttonY
        case .leftShoulder: return gamepad.leftShoulder
        case .rightShoulder: return gamepad.rightShoulder
        case .leftTrigger: return gamepad.leftTrigger
        case .rightTrigger: return gamepad.rightTrigger
        case .buttonMenu: return gamepad.buttonMenu
        case .buttonOptions: return gamepad.buttonOptions
        case .leftThumbstickButton: return gamepad.leftThumbstickButton
        case .rightThumbstickButton: return gamepad.rightThumbstickButton
        case .dpadUp: return gamepad.dpad.up
        case .dpadDown: return gamepad.dpad.down
        case .dpadLeft: return gamepad.dpad.left
        case .dpadRight: return gamepad.dpad.right
        }
    }

    func isPressed(on gamepad: GCExtendedGamepad, threshold: Float) -> Bool {
        switch self {
        // Triggers are analog; use the shared threshold instead of the
        // system's notion of "pressed".
        case .leftTrigger: return gamepad.leftTrigger.value > threshold
        case .rightTrigger: return gamepad.rightTrigger.value > threshold
        default: return buttonInput(on: gamepad)?.isPressed ?? false
        }
    }
}

// Hardware-keyboard host inputs, addressed by HID usage.
enum KeyboardKey {
    private static let tokenPrefix = "kb."

    static func token(forUsage usage: Int) -> String {
        "\(tokenPrefix)\(usage)"
    }

    static func usage(fromToken token: String) -> Int? {
        guard token.hasPrefix(tokenPrefix),
              let usage = Int(token.dropFirst(tokenPrefix.count)),
              (0x04...0xE7).contains(usage) else {
            return nil
        }
        return usage
    }

    static func isKeyboardToken(_ token: String) -> Bool {
        usage(fromToken: token) != nil
    }

    static func displayName(forUsage usage: Int) -> String {
        switch usage {
        case 0x04...0x1D:
            return String(UnicodeScalar(UInt8(65 + usage - 0x04)))       // A-Z
        case 0x1E...0x26:
            return String(UnicodeScalar(UInt8(49 + usage - 0x1E)))       // 1-9
        case 0x27: return "0"
        case 0x28: return "Return"
        case 0x29: return "Esc"
        case 0x2A: return "Delete"
        case 0x2B: return "Tab"
        case 0x2C: return "Space"
        case 0x3A...0x45:
            return "F\(usage - 0x39)"                                    // F1-F12
        case 0x4F: return "\u{2192}"
        case 0x50: return "\u{2190}"
        case 0x51: return "\u{2193}"
        case 0x52: return "\u{2191}"
        case 0x54: return "Keypad /"
        case 0x55: return "Keypad *"
        case 0x56: return "Keypad -"
        case 0x57: return "Keypad +"
        case 0x58: return "Keypad Enter"
        case 0x59...0x61:
            return "Keypad \(usage - 0x58)"                              // keypad 1-9
        case 0x62: return "Keypad 0"
        case 0xE0, 0xE4: return "Ctrl"
        case 0xE1, 0xE5: return "Shift"
        case 0xE2, 0xE6: return "Alt"
        case 0xE3, 0xE7: return "Cmd"
        default:
            return String(format: "Key 0x%02X", usage)
        }
    }
}

// One guest (Nokia) key that can receive a binding. Digit keys carry no
// symbol — their name is the glyph already.
struct GuestKey: Identifiable {
    let scan: UInt32
    let name: String
    var symbol: String?
    var symbolColor: Color?

    var id: UInt32 { scan }
}

enum GuestKeys {
    static let directions: [GuestKey] = [
        GuestKey(scan: Scan.up, name: String(localized: "key.up"), symbol: "arrow.up.circle"),
        GuestKey(scan: Scan.down, name: String(localized: "key.down"), symbol: "arrow.down.circle"),
        GuestKey(scan: Scan.left, name: String(localized: "key.left"), symbol: "arrow.left.circle"),
        GuestKey(scan: Scan.right, name: String(localized: "key.right"), symbol: "arrow.right.circle"),
    ]

    static let actions: [GuestKey] = [
        GuestKey(scan: Scan.select, name: String(localized: "key.select"),
                 symbol: "checkmark.circle"),
        GuestKey(scan: Scan.leftSoft, name: String(localized: "key.leftSoft"),
                 symbol: "l.circle"),
        GuestKey(scan: Scan.rightSoft, name: String(localized: "key.rightSoft"),
                 symbol: "r.circle"),
        GuestKey(scan: Scan.call, name: String(localized: "key.call"),
                 symbol: "phone.circle", symbolColor: .green),
        GuestKey(scan: Scan.end, name: String(localized: "key.end"),
                 symbol: "phone.down.circle", symbolColor: .red),
        GuestKey(scan: Scan.clear, name: String(localized: "key.clear"),
                 symbol: "delete.left"),
    ]

    static let digits: [GuestKey] = [
        GuestKey(scan: 0x31, name: "1"), GuestKey(scan: 0x32, name: "2"),
        GuestKey(scan: 0x33, name: "3"), GuestKey(scan: 0x34, name: "4"),
        GuestKey(scan: 0x35, name: "5"), GuestKey(scan: 0x36, name: "6"),
        GuestKey(scan: 0x37, name: "7"), GuestKey(scan: 0x38, name: "8"),
        GuestKey(scan: 0x39, name: "9"), GuestKey(scan: 0x30, name: "0"),
        GuestKey(scan: Scan.star, name: "\u{2217}"),
        GuestKey(scan: Scan.hash, name: "#"),
    ]

    static let all: [GuestKey] = directions + actions + digits
}

// MARK: - Connected peripherals

// Tracks connected input peripherals and which one is active. Lives for the
// whole app so runtime input (PeripheralInputBridge) and the Settings UI
// observe one source of truth. All state is main-thread confined:
// notifications are observed on .main and SwiftUI actions run on main.
final class PeripheralManager: ObservableObject, @unchecked Sendable {
    struct Peripheral: Identifiable, Equatable {
        enum Kind {
            case controller, keyboard
        }

        let id: String
        let deviceKey: String
        let name: String
        let kind: Kind
    }

    static let shared = PeripheralManager()
    static let keyboardID = "keyboard"

    @Published private(set) var peripherals: [Peripheral] = []
    @Published private(set) var activeID: String?

    // Connect order; the keyboard entry is appended after the controllers.
    private var controllers: [GCController] = []
    private var keyboardPresent = false

    private init() {
        let center = NotificationCenter.default
        center.addObserver(forName: .GCControllerDidConnect, object: nil,
                           queue: .main) { [weak self] note in
            guard let controller = note.object as? GCController else { return }
            self?.controllerConnected(controller)
        }
        center.addObserver(forName: .GCControllerDidDisconnect, object: nil,
                           queue: .main) { [weak self] note in
            guard let controller = note.object as? GCController else { return }
            self?.controllerDisconnected(controller)
        }
        center.addObserver(forName: .GCKeyboardDidConnect, object: nil,
                           queue: .main) { [weak self] _ in
            self?.keyboardChanged(present: true)
        }
        center.addObserver(forName: .GCKeyboardDidDisconnect, object: nil,
                           queue: .main) { [weak self] _ in
            self?.keyboardChanged(present: false)
        }

        controllers = GCController.controllers().filter { $0.extendedGamepad != nil }
        keyboardPresent = GCKeyboard.coalesced != nil
        rebuildPeripherals()
        // Connect order of pre-attached devices is unknown; prefer a
        // controller over the keyboard.
        activeID = controllers.last.map(Self.id(for:))
            ?? (keyboardPresent ? Self.keyboardID : nil)
    }

    static func id(for controller: GCController) -> String {
        "gc-\(ObjectIdentifier(controller))"
    }

    // Mapping storage key: same-model controllers share one mapping, so it
    // survives reconnects (GCController has no persistent identity).
    static func deviceKey(for controller: GCController) -> String {
        "gc:\(name(of: controller))"
    }

    static func name(of controller: GCController) -> String {
        controller.vendorName ?? controller.productCategory
    }

    func setActive(_ id: String) {
        if peripherals.contains(where: { $0.id == id }) {
            activeID = id
        }
    }

    func controller(peripheralID: String) -> GCController? {
        controllers.first { Self.id(for: $0) == peripheralID }
    }

    var activeController: GCController? {
        activeID.flatMap(controller(peripheralID:))
    }

    func isActive(_ controller: GCController) -> Bool {
        activeID == Self.id(for: controller)
    }

    // Hardware-keyboard key presses drive the guest unless a controller is
    // the active peripheral.
    var keyboardCanDrive: Bool {
        activeID == nil || activeID == Self.keyboardID
    }

    func activeControllerMapping() -> [String: UInt32] {
        guard let controller = activeController else { return [:] }
        return PeripheralMappingStore.mapping(forDeviceKey: Self.deviceKey(for: controller),
                                              kind: .controller)
    }

    private func rebuildPeripherals() {
        var list = controllers.map { controller in
            Peripheral(id: Self.id(for: controller),
                       deviceKey: Self.deviceKey(for: controller),
                       name: Self.name(of: controller),
                       kind: .controller)
        }
        if keyboardPresent {
            list.append(Peripheral(id: Self.keyboardID,
                                   deviceKey: Self.keyboardID,
                                   name: String(localized: "peripheral.keyboard"),
                                   kind: .keyboard))
        }
        peripherals = list
    }

    private func controllerConnected(_ controller: GCController) {
        guard controller.extendedGamepad != nil else { return }
        if !controllers.contains(where: { $0 === controller }) {
            controllers.append(controller)
        }
        rebuildPeripherals()
        activeID = Self.id(for: controller)
    }

    private func controllerDisconnected(_ controller: GCController) {
        controllers.removeAll { $0 === controller }
        rebuildPeripherals()
        if activeID == Self.id(for: controller) {
            fallbackActive()
        }
    }

    private func keyboardChanged(present: Bool) {
        keyboardPresent = present
        rebuildPeripherals()
        if present {
            activeID = Self.keyboardID
        } else if activeID == Self.keyboardID {
            fallbackActive()
        }
    }

    private func fallbackActive() {
        activeID = controllers.last.map(Self.id(for:))
            ?? (keyboardPresent ? Self.keyboardID : nil)
    }
}

// MARK: - Per-device mapping storage

enum PeripheralMappingStore {
    static let storageKey = "ios.peripheralKeyMappings"

    static let controllerDefaults: [String: UInt32] = [
        HostButton.dpadUp.rawValue: Scan.up,
        HostButton.dpadDown.rawValue: Scan.down,
        HostButton.dpadLeft.rawValue: Scan.left,
        HostButton.dpadRight.rawValue: Scan.right,
        HostButton.buttonA.rawValue: Scan.select,
        HostButton.buttonB.rawValue: Scan.clear,
        HostButton.buttonX.rawValue: 0x35,   // digit 5, common game action key
        HostButton.buttonY.rawValue: 0x37,   // digit 7, common secondary game key
        HostButton.leftShoulder.rawValue: Scan.leftSoft,
        HostButton.rightShoulder.rawValue: Scan.rightSoft,
        HostButton.leftTrigger.rawValue: Scan.star,
        HostButton.rightTrigger.rawValue: Scan.hash,
        HostButton.buttonOptions.rawValue: Scan.call,
        HostButton.buttonMenu.rawValue: Scan.end,
    ]

    // Mirrors the historical hardcoded hardware-keyboard layout.
    static let keyboardDefaults: [String: UInt32] = [
        KeyboardKey.token(forUsage: 0x52): Scan.up,
        KeyboardKey.token(forUsage: 0x51): Scan.down,
        KeyboardKey.token(forUsage: 0x50): Scan.left,
        KeyboardKey.token(forUsage: 0x4F): Scan.right,
        KeyboardKey.token(forUsage: 0x28): Scan.select,     // Return
        KeyboardKey.token(forUsage: 0x3A): Scan.leftSoft,   // F1
        KeyboardKey.token(forUsage: 0x3B): Scan.rightSoft,  // F2
        KeyboardKey.token(forUsage: 0x3C): Scan.call,       // F3
        KeyboardKey.token(forUsage: 0x3D): Scan.end,        // F4
        KeyboardKey.token(forUsage: 0x2A): Scan.clear,      // Delete/Backspace
        KeyboardKey.token(forUsage: 0x1E): 0x31, KeyboardKey.token(forUsage: 0x1F): 0x32,
        KeyboardKey.token(forUsage: 0x20): 0x33, KeyboardKey.token(forUsage: 0x21): 0x34,
        KeyboardKey.token(forUsage: 0x22): 0x35, KeyboardKey.token(forUsage: 0x23): 0x36,
        KeyboardKey.token(forUsage: 0x24): 0x37, KeyboardKey.token(forUsage: 0x25): 0x38,
        KeyboardKey.token(forUsage: 0x26): 0x39, KeyboardKey.token(forUsage: 0x27): 0x30,
    ]

    static func defaults(for kind: PeripheralManager.Peripheral.Kind) -> [String: UInt32] {
        kind == .controller ? controllerDefaults : keyboardDefaults
    }

    static func mapping(forDeviceKey deviceKey: String,
                        kind: PeripheralManager.Peripheral.Kind) -> [String: UInt32] {
        guard let all = UserDefaults.standard.dictionary(forKey: storageKey),
              let stored = all[deviceKey] as? [String: Any] else {
            return defaults(for: kind)
        }
        var mapping: [String: UInt32] = [:]
        for (token, value) in stored {
            guard let number = value as? NSNumber else { continue }
            let scan = UInt32(truncating: number)
            guard isValid(token: token, kind: kind),
                  GuestKeys.all.contains(where: { $0.scan == scan }) else {
                continue
            }
            mapping[token] = scan
        }
        return mapping
    }

    static func save(_ mapping: [String: UInt32], forDeviceKey deviceKey: String) {
        var all = UserDefaults.standard.dictionary(forKey: storageKey) ?? [:]
        all[deviceKey] = mapping.mapValues { Int($0) }
        UserDefaults.standard.set(all, forKey: storageKey)
    }

    static func reset(deviceKey: String) {
        var all = UserDefaults.standard.dictionary(forKey: storageKey) ?? [:]
        all.removeValue(forKey: deviceKey)
        UserDefaults.standard.set(all, forKey: storageKey)
    }

    // Host→scan for hardware-keyboard runtime lookups, keyed by HID usage.
    static func keyboardScanMapping() -> [Int: UInt32] {
        var result: [Int: UInt32] = [:]
        for (token, scan) in mapping(forDeviceKey: PeripheralManager.keyboardID, kind: .keyboard) {
            if let usage = KeyboardKey.usage(fromToken: token) {
                result[usage] = scan
            }
        }
        return result
    }

    // Keeps the relation one-to-one: the captured host input is stolen from
    // whatever guest key held it (last edit wins), and this guest key's old
    // binding is dropped.
    static func bind(hostToken: String, toScan scan: UInt32, in mapping: inout [String: UInt32]) {
        unbind(scan: scan, in: &mapping)
        mapping[hostToken] = scan
    }

    static func unbind(scan: UInt32, in mapping: inout [String: UInt32]) {
        for (token, value) in mapping where value == scan {
            mapping.removeValue(forKey: token)
        }
    }

    static func hostTokens(boundToScan scan: UInt32, in mapping: [String: UInt32]) -> [String] {
        let controller = HostButton.allCases
            .filter { mapping[$0.rawValue] == scan }
            .map(\.rawValue)
        let keyboard = mapping
            .filter { $0.value == scan && KeyboardKey.isKeyboardToken($0.key) }
            .keys.sorted()
        return controller + keyboard
    }

    private static func isValid(token: String, kind: PeripheralManager.Peripheral.Kind) -> Bool {
        switch kind {
        case .controller: return HostButton(rawValue: token) != nil
        case .keyboard: return KeyboardKey.isKeyboardToken(token)
        }
    }
}

// MARK: - Capture

// Watches one peripheral while its mapping editor is open. During a capture
// it reports the first host input that transitions to pressed after the
// capture started (controller buttons already held when it starts are
// ignored). The emulator screen is never visible at the same time as
// Settings, so this never races the emulator's input paths.
@MainActor
private final class CaptureMonitor: ObservableObject {
    @Published private(set) var deviceConnected = true
    @Published private(set) var capturedToken: String?
    /// Bumped on every accepted capture; drives the confirmation tap.
    @Published private(set) var captures = 0

    private let peripheral: PeripheralManager.Peripheral
    private var observers: [NSObjectProtocol] = []
    private var capturing = false
    private var pressedTokens: Set<String> = []
    private let threshold: Float = 0.45

    init(peripheral: PeripheralManager.Peripheral) {
        self.peripheral = peripheral
    }

    func start() {
        let center = NotificationCenter.default
        for name: Notification.Name in [.GCControllerDidConnect, .GCControllerDidDisconnect,
                                        .GCKeyboardDidConnect, .GCKeyboardDidDisconnect] {
            observers.append(center.addObserver(forName: name, object: nil,
                                                queue: .main) { [weak self] _ in
                Task { @MainActor in self?.refresh() }
            })
        }
        refresh()
    }

    func stop() {
        observers.forEach(NotificationCenter.default.removeObserver)
        observers.removeAll()
        detach()
        capturing = false
        capturedToken = nil
    }

    func beginCapture() {
        capturedToken = nil
        capturing = true
    }

    func cancelCapture() {
        capturing = false
        capturedToken = nil
    }

    private func detach() {
        switch peripheral.kind {
        case .controller:
            PeripheralManager.shared.controller(peripheralID: peripheral.id)?
                .extendedGamepad?.valueChangedHandler = nil
        case .keyboard:
            GCKeyboard.coalesced?.keyboardInput?.keyChangedHandler = nil
        }
    }

    private func refresh() {
        switch peripheral.kind {
        case .controller:
            let gamepad = PeripheralManager.shared
                .controller(peripheralID: peripheral.id)?.extendedGamepad
            deviceConnected = gamepad != nil
            let threshold = self.threshold
            gamepad?.valueChangedHandler = { [weak self] pad, _ in
                let pressed = Self.pressedTokens(on: pad, threshold: threshold)
                Task { @MainActor in self?.update(pressed: pressed) }
            }
        case .keyboard:
            deviceConnected = GCKeyboard.coalesced != nil
            GCKeyboard.coalesced?.keyboardInput?.keyChangedHandler = { [weak self] _, _, keyCode, pressed in
                guard pressed else { return }
                let token = KeyboardKey.token(forUsage: Int(keyCode.rawValue))
                Task { @MainActor in self?.capture(token: token) }
            }
        }
    }

    private func update(pressed: Set<String>) {
        let newlyPressed = pressed.subtracting(pressedTokens)
        pressedTokens = pressed
        if let token = newlyPressed.first {
            capture(token: token)
        }
    }

    private func capture(token: String) {
        guard capturing else { return }
        capturing = false
        capturedToken = token
        captures += 1
    }

    private nonisolated static func pressedTokens(on gamepad: GCExtendedGamepad,
                                                  threshold: Float) -> Set<String> {
        var tokens = Set(
            HostButton.allCases
                .filter { $0.isPressed(on: gamepad, threshold: threshold) }
                .map(\.rawValue)
        )
        // The left thumbstick captures as the d-pad, mirroring runtime input.
        if gamepad.leftThumbstick.yAxis.value > threshold { tokens.insert(HostButton.dpadUp.rawValue) }
        if gamepad.leftThumbstick.yAxis.value < -threshold { tokens.insert(HostButton.dpadDown.rawValue) }
        if gamepad.leftThumbstick.xAxis.value < -threshold { tokens.insert(HostButton.dpadLeft.rawValue) }
        if gamepad.leftThumbstick.xAxis.value > threshold { tokens.insert(HostButton.dpadRight.rawValue) }
        return tokens
    }
}

// MARK: - Key mapping editor

struct KeyMappingView: View {
    let peripheral: PeripheralManager.Peripheral

    @State private var mapping: [String: UInt32]
    @StateObject private var monitor: CaptureMonitor
    @State private var captureTarget: GuestKey?

    init(peripheral: PeripheralManager.Peripheral) {
        self.peripheral = peripheral
        _mapping = State(initialValue: PeripheralMappingStore.mapping(
            forDeviceKey: peripheral.deviceKey, kind: peripheral.kind))
        _monitor = StateObject(wrappedValue: CaptureMonitor(peripheral: peripheral))
    }

    var body: some View {
        Form {
            Section {
                ForEach(GuestKeys.directions, content: row)
            } header: {
                Text("controllerMapping.directions")
            } footer: {
                if peripheral.kind == .controller {
                    Text("controllerMapping.directions.hint")
                }
            }
            Section("controllerMapping.actions") {
                ForEach(GuestKeys.actions, content: row)
            }
            Section("controllerMapping.numbers") {
                ForEach(GuestKeys.digits, content: row)
            }
            Section {
                Button("controllerMapping.reset", role: .destructive) {
                    PeripheralMappingStore.reset(deviceKey: peripheral.deviceKey)
                    mapping = PeripheralMappingStore.defaults(for: peripheral.kind)
                }
            } footer: {
                if !monitor.deviceConnected {
                    Text("controllerMapping.deviceDisconnected")
                }
            }
        }
        .navigationTitle(peripheral.name)
        .hapticImpact(.medium, trigger: monitor.captures)
        .onAppear(perform: monitor.start)
        .onDisappear(perform: monitor.stop)
        .sheet(item: $captureTarget, onDismiss: monitor.cancelCapture) { key in
            CaptureSheet(
                key: key,
                kind: peripheral.kind,
                currentBinding: bindingText(for: key),
                monitor: monitor,
                onClear: {
                    PeripheralMappingStore.unbind(scan: key.scan, in: &mapping)
                    PeripheralMappingStore.save(mapping, forDeviceKey: peripheral.deviceKey)
                    captureTarget = nil
                },
                onCancel: { captureTarget = nil }
            )
        }
        .onChange(of: monitor.capturedToken) { token in
            guard let token, let target = captureTarget else { return }
            PeripheralMappingStore.bind(hostToken: token, toScan: target.scan, in: &mapping)
            PeripheralMappingStore.save(mapping, forDeviceKey: peripheral.deviceKey)
            captureTarget = nil
        }
    }

    private func row(_ key: GuestKey) -> some View {
        Button {
            monitor.beginCapture()
            captureTarget = key
        } label: {
            Label {
                HStack {
                    // Explicit Color.primary/.secondary: the relative styles
                    // would resolve against the button's accent tint here,
                    // not label color.
                    Text(key.name)
                        .foregroundStyle(Color.primary)
                    Spacer()
                    Text(bindingText(for: key) ?? String(localized: "key.none"))
                        .foregroundStyle(Color.secondary)
                        .multilineTextAlignment(.trailing)
                }
            } icon: {
                if let symbol = key.symbol {
                    Image(systemName: symbol)
                        .foregroundStyle(key.symbolColor ?? Color.accentColor)
                }
            }
        }
    }

    private func bindingText(for key: GuestKey) -> String? {
        let tokens = PeripheralMappingStore.hostTokens(boundToScan: key.scan, in: mapping)
        guard !tokens.isEmpty else { return nil }
        let gamepad = PeripheralManager.shared
            .controller(peripheralID: peripheral.id)?.extendedGamepad
        return tokens.map { hostDisplayName(token: $0, gamepad: gamepad) }
            .joined(separator: " \u{00B7} ")
    }

    private func hostDisplayName(token: String, gamepad: GCExtendedGamepad?) -> String {
        if let button = HostButton(rawValue: token) {
            return button.displayName(on: gamepad)
        }
        if let usage = KeyboardKey.usage(fromToken: token) {
            return KeyboardKey.displayName(forUsage: usage)
        }
        return token
    }
}

private struct CaptureSheet: View {
    let key: GuestKey
    let kind: PeripheralManager.Peripheral.Kind
    let currentBinding: String?
    @ObservedObject var monitor: CaptureMonitor
    let onClear: () -> Void
    let onCancel: () -> Void

    var body: some View {
        VStack(spacing: 20) {
            Label {
                Text(key.name)
            } icon: {
                if let symbol = key.symbol {
                    Image(systemName: symbol)
                        .foregroundStyle(key.symbolColor ?? Color.accentColor)
                }
            }
            .font(.title3.weight(.semibold))
            Image(systemName: kind == .controller ? "gamecontroller" : "keyboard")
                .font(.system(size: 52))
                .foregroundStyle(.secondary)
                .symbolEffectIfAvailable()
            Text(kind == .controller
                 ? "controllerMapping.pressButton" : "controllerMapping.pressKey")
                .font(.body)
                .multilineTextAlignment(.center)
            if !monitor.deviceConnected {
                Text("controllerMapping.deviceDisconnected")
                    .font(.footnote)
                    .foregroundStyle(.orange)
            }
            if let currentBinding {
                Text("controllerMapping.currentBinding \(currentBinding)")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
            VStack(spacing: 10) {
                if currentBinding != nil {
                    Button("controllerMapping.clearBinding", role: .destructive, action: onClear)
                }
                Button("common.cancel", action: onCancel)
            }
        }
        .padding(24)
        .presentationDetents([.medium])
    }
}

private extension View {
    // Pulse the glyph while waiting for a press on OSes that have the
    // effect; a static glyph is fine below iOS 17.
    @ViewBuilder
    func symbolEffectIfAvailable() -> some View {
        if #available(iOS 17, *) {
            symbolEffect(.pulse)
        } else {
            self
        }
    }
}

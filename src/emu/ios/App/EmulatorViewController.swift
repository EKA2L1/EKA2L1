import GameController
import QuartzCore
import UIKit

private final class PeripheralInputBridge: @unchecked Sendable {
    private let threshold: Float = 0.45
    private let lock = NSLock()
    private var activeScansByToken: [String: UInt32] = [:]
    private var observers: [NSObjectProtocol] = []
    // Mapping of the active controller peripheral; empty when the keyboard
    // (or nothing) is active. Refreshed whenever the peripheral set changes —
    // that is the only time the active selection can move while the emulator
    // screen is up, since Settings is not reachable then.
    private var mapping: [String: UInt32] = [:]
    // Hardware-keyboard binding, keyed by HID usage. Loaded on start().
    private var keyboardMapping: [Int: UInt32] = [:]
    // Cleared while a host panel is up (see setKeyboardEnabled). Survives
    // start()/stop() so a panel open across a re-appear stays in control.
    private var keyboardEnabled = true

    func start() {
        keyboardMapping = PeripheralMappingStore.keyboardScanMapping()
        // Touch the manager first so its connect/disconnect observers are
        // registered ahead of ours and refresh() reads updated state.
        refresh()
        let center = NotificationCenter.default
        for name: Notification.Name in [.GCControllerDidConnect, .GCControllerDidDisconnect,
                                        .GCKeyboardDidConnect, .GCKeyboardDidDisconnect] {
            observers.append(center.addObserver(forName: name, object: nil,
                                                queue: .main) { [weak self] _ in
                self?.refresh()
            })
        }
    }

    func stop() {
        observers.forEach(NotificationCenter.default.removeObserver)
        observers.removeAll()
        GCController.controllers().forEach { $0.extendedGamepad?.valueChangedHandler = nil }
        GCKeyboard.coalesced?.keyboardInput?.keyChangedHandler = nil
        releaseAll()
    }

    // GameController delivers key events app-wide rather than through the
    // responder chain, so guest key input has to be suspended explicitly while
    // a host panel is visible instead of by dropping first-responder status.
    func setKeyboardEnabled(_ enabled: Bool) {
        guard enabled != keyboardEnabled else { return }
        keyboardEnabled = enabled
        if !enabled {
            releaseKeyboard()
        }
    }

    private func refresh() {
        // The active peripheral may have changed; drop held keys so a key
        // pressed on the previous device can't stay stuck down.
        releaseAll()
        mapping = PeripheralManager.shared.activeControllerMapping()
        for controller in GCController.controllers() {
            attach(controller)
        }
        if let controller = PeripheralManager.shared.activeController,
           let gamepad = controller.extendedGamepad {
            handleAll(gamepad, from: controller)
        }
        GCKeyboard.coalesced?.keyboardInput?.keyChangedHandler = { [weak self] _, _, keyCode, pressed in
            self?.handleKey(usage: Int(keyCode.rawValue), pressed: pressed)
        }
    }

    private func attach(_ controller: GCController) {
        guard let gamepad = controller.extendedGamepad else { return }
        gamepad.valueChangedHandler = { [weak self, weak controller] pad, _ in
            guard let controller else { return }
            self?.handleAll(pad, from: controller)
        }
    }

    // Re-sync every mapped control on each value change. updateButton only
    // emits on state transitions, so this is cheap and keeps the pad-to-scan
    // mapping in one place instead of duplicating it per element. Only the
    // active peripheral drives the guest; refresh() releases anything a
    // previously active pad still held.
    private func handleAll(_ gamepad: GCExtendedGamepad, from controller: GCController) {
        guard PeripheralManager.shared.isActive(controller) else { return }
        for button in HostButton.allCases {
            updateButton(token: button.rawValue,
                         pressed: button.isPressed(on: gamepad, threshold: threshold))
        }
        // The left thumbstick drives the d-pad bindings; separate state
        // tokens so releasing the stick doesn't drop a still-held d-pad key.
        updateButton(token: "leftStick.up", mappingToken: HostButton.dpadUp.rawValue,
                     pressed: gamepad.leftThumbstick.yAxis.value > threshold)
        updateButton(token: "leftStick.down", mappingToken: HostButton.dpadDown.rawValue,
                     pressed: gamepad.leftThumbstick.yAxis.value < -threshold)
        updateButton(token: "leftStick.left", mappingToken: HostButton.dpadLeft.rawValue,
                     pressed: gamepad.leftThumbstick.xAxis.value < -threshold)
        updateButton(token: "leftStick.right", mappingToken: HostButton.dpadRight.rawValue,
                     pressed: gamepad.leftThumbstick.xAxis.value > threshold)
    }

    // Hardware keyboard. Only presses are gated: a key held across a suspend
    // or an active-peripheral switch is released by releaseKeyboard() /
    // releaseAll(), and its release must still be forwarded if it arrives
    // later, so it cannot stay stuck down in the guest.
    private func handleKey(usage: Int, pressed: Bool) {
        if pressed, !keyboardEnabled || !PeripheralManager.shared.keyboardCanDrive {
            return
        }
        updateInput(token: KeyboardKey.token(forUsage: usage),
                    scan: scanCode(forUsage: usage), pressed: pressed)
    }

    // Map a hardware-keyboard HID usage to an EPOC standard scan code (see
    // services/window/keys.h `std_scan_code`). The user mapping wins; keys it
    // doesn't cover fall back to text-style input that is not part of the
    // bindable guest key list: letters use their uppercase ASCII value as the
    // scan code (the EPOC convention, needed for multitap text entry), plus
    // space/tab, the keypad * and ISO # glyphs, and the Esc/Enter conveniences.
    private func scanCode(forUsage usage: Int) -> UInt32 {
        if let mapped = keyboardMapping[usage] {
            return mapped
        }
        switch usage {
        case 0x04...0x1D:           // a-z -> uppercase ASCII == std scan code
            return UInt32(0x41 + usage - 0x04)
        case 0x2B:                  // tab
            return 0x02             // std_key_tab
        case 0x2C:                  // space
            return 0x05             // std_key_space
        case 0x29:                  // escape
            return 0xA5             // std_key_device_1 (right soft key / back)
        case 0x32:                  // non-US # (a bare key only on ISO layouts)
            return 0x7F
        case 0x55:                  // keypad *
            return 0x2A
        case 0x58:                  // keypad enter
            return 0xA7             // std_key_device_3 (select / OK)
        default:
            return 0
        }
    }

    // `token` keys the press state (d-pad and left stick track separately),
    // `mappingToken` keys the user mapping (both share one direction entry).
    private func updateButton(token: String, mappingToken: String? = nil, pressed: Bool) {
        updateInput(token: token, scan: mapping[mappingToken ?? token] ?? 0, pressed: pressed)
    }

    private func updateInput(token: String, scan: UInt32, pressed: Bool) {
        let event: (UInt32, Bool)?
        lock.lock()
        let wasPressed = activeScansByToken[token] != nil
        if pressed == wasPressed || (pressed && scan == 0) {
            event = nil
        } else if pressed {
            activeScansByToken[token] = scan
            event = (scan, true)
        } else if let oldScan = activeScansByToken.removeValue(forKey: token) {
            event = (oldScan, false)
        } else {
            event = nil
        }
        lock.unlock()

        if let event {
            EKA2L1Bridge.submitRawKey(event.0, pressed: event.1)
        }
    }

    private func releaseAll() {
        release { _ in true }
    }

    private func releaseKeyboard() {
        release(where: KeyboardKey.isKeyboardToken)
    }

    private func release(where matches: (String) -> Bool) {
        lock.lock()
        let released = activeScansByToken.filter { matches($0.key) }
        released.keys.forEach { activeScansByToken.removeValue(forKey: $0) }
        lock.unlock()

        for scan in released.values {
            EKA2L1Bridge.submitRawKey(scan, pressed: false)
        }
    }
}

private final class EKA2L1RenderView: UIView {
    var surfaceReady = false

    // When true, the presented guest picture is pinned just below the top safe
    // area instead of centred, so a bottom keypad overlay covers letterbox
    // rather than gameplay. Pushed to the bridge on every layout pass (the
    // anchor is in surface pixels, so it depends on renderScale and insets).
    var anchorsDisplayTop = false {
        didSet {
            if anchorsDisplayTop != oldValue {
                setNeedsLayout()
            }
        }
    }

    // Regions (in this view's coordinates) covered by virtual-keypad elements
    // overlay. The render view fills the whole screen and wins touch
    // hit-testing over the keypad drawn above it — so unless it yields here,
    // every keypad tap is swallowed as a guest pointer touch. Decline touches
    // inside a keypad region so they reach the keys; keep claiming the
    // exposed game area so guest touch input still works.
    var keypadHitRegions: [CGRect] = []

    override class var layerClass: AnyClass {
        CAEAGLLayer.self
    }

    private var eaglLayer: CAEAGLLayer {
        layer as! CAEAGLLayer
    }

    override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
        if keypadHitRegions.contains(where: { $0.contains(point) }) {
            return nil
        }
        return super.hitTest(point, with: event)
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
        contentScaleFactor = UIScreen.main.nativeScale
        isMultipleTouchEnabled = true
        isOpaque = true
        backgroundColor = .black
        eaglLayer.isOpaque = true
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func layoutSubviews() {
        super.layoutSubviews()

        let scale = renderScale
        eaglLayer.contentsScale = scale
        let pixels = CGSize(width: bounds.width * scale, height: bounds.height * scale)
        guard pixels.width > 0, pixels.height > 0 else { return }

        EKA2L1Bridge.shared.attach(layer: eaglLayer, pixelSize: pixels, scale: scale)
        EKA2L1Bridge.shared.setDisplayAnchorTop(
            pixels: anchorsDisplayTop ? Int(safeAreaInsets.top * scale) : -1)
        pushInterfaceRotation()
        surfaceReady = true
    }

    // CoreMotion reports accelerometer samples in the physical device frame;
    // the bridge needs the interface orientation to rotate them into the frame
    // of the emulated device as displayed. Expressed as the content's CCW
    // rotation from the device's natural portrait orientation — landscapeLeft
    // (device turned clockwise, home indicator left) is 90, landscapeRight is
    // 270. Layout runs on every rotation, so pushing here keeps it current.
    private func pushInterfaceRotation() {
        switch window?.windowScene?.interfaceOrientation {
        case .landscapeLeft:
            EKA2L1Bridge.shared.setHostInterfaceRotation(degrees: 90)
        case .portraitUpsideDown:
            EKA2L1Bridge.shared.setHostInterfaceRotation(degrees: 180)
        case .landscapeRight:
            EKA2L1Bridge.shared.setHostInterfaceRotation(degrees: 270)
        case .portrait:
            EKA2L1Bridge.shared.setHostInterfaceRotation(degrees: 0)
        default:
            break
        }
    }

    /// Backing-store scale for the GL surface. On a real device this is the
    /// native screen scale and the GPU does the present blit for free. The
    /// iOS Simulator has no GPU-backed GLES — every present blit is rasterized
    /// in software on the host CPU, and its cost grows with the pixel count.
    /// Since the guest screen is tiny (e.g. 240x320) and the present is just an
    /// upscale, rendering it at full native Retina only burns host CPU on
    /// interpolated pixels with no added detail. Cap the simulator surface so
    /// the software blit stops being the frame bottleneck.
    private var renderScale: CGFloat {
        #if targetEnvironment(simulator)
        return min(contentScaleFactor, 1.5)
        #else
        return contentScaleFactor
        #endif
    }

    private func pointerPhase(for touch: UITouch) -> EKA2L1PointerPhase {
        switch touch.phase {
        case .began:
            return .began
        case .moved, .stationary:
            return .moved
        case .ended:
            return .ended
        case .cancelled:
            return .cancelled
        case .regionEntered, .regionMoved:
            return .moved
        case .regionExited:
            return .ended
        @unknown default:
            return .cancelled
        }
    }

    private func dispatchTouches(_ touches: Set<UITouch>) {
        // Must match the surface scale used in layoutSubviews so guest pointer
        // coordinates line up with the (possibly downscaled) render surface.
        let scale = renderScale
        for touch in touches {
            let point = touch.location(in: self)
            EKA2L1Bridge.shared.submitPointer(
                x: point.x * scale,
                y: point.y * scale,
                phase: pointerPhase(for: touch),
                pointerId: UInt(bitPattern: ObjectIdentifier(touch))
            )
        }
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        dispatchTouches(touches)
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        dispatchTouches(touches)
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        dispatchTouches(touches)
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        dispatchTouches(touches)
    }
}

final class EmulatorViewController: UIViewController {
    private let uid: UInt32
    // Delivered on the main queue after the guest launch path has completed.
    var onAppLaunch: ((Bool) -> Void)?
    // Invoked when the guest app exits on its own (Exit soft key / panic /
    // normal termination) so the SwiftUI host can pop this screen.
    var onAppExit: ((String?) -> Void)?
    // Forwarded to the render view; see EKA2L1RenderView.anchorsDisplayTop.
    var anchorsDisplayTop = false {
        didSet {
            if isViewLoaded {
                gameView.anchorsDisplayTop = anchorsDisplayTop
            }
        }
    }
    // Forwarded to the render view; see EKA2L1RenderView.keypadHitRegions.
    var keypadHitRegions: [CGRect] = [] {
        didSet {
            if isViewLoaded {
                gameView.keypadHitRegions = keypadHitRegions
            }
        }
    }
    private var launched = false
    private let peripheralInput = PeripheralInputBridge()
    private var gameView: EKA2L1RenderView {
        view as! EKA2L1RenderView
    }

    init(uid: UInt32) {
        self.uid = uid
        super.init(nibName: nil, bundle: nil)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    // The emulator screen's permitted orientations are owned by the keypad
    // layout (see DisplayOrientation). requestGeometryUpdate validates against
    // the view-controller chain, so this must widen with the layout — otherwise
    // the SwiftUI host caps the screen at portrait and a fullscreen (touch)
    // layout can't rotate a landscape guest sideways.
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask {
        lockedInterfaceOrientationMask
    }

    override var shouldAutorotate: Bool { true }

    override func loadView() {
        let renderView = EKA2L1RenderView(frame: UIScreen.main.bounds)
        renderView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        renderView.anchorsDisplayTop = anchorsDisplayTop
        renderView.keypadHitRegions = keypadHitRegions
        view = renderView
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .black
    }

    // Suspend guest keyboard input while a host panel is visible, so the panel
    // handles the hardware keyboard on its own.
    func setHardwareKeyboardCaptureEnabled(_ enabled: Bool) {
        peripheralInput.setKeyboardEnabled(enabled)
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        gameView.setNeedsLayout()
        gameView.layoutIfNeeded()
        peripheralInput.start()
        EKA2L1Bridge.shared.resume()
        // Launch once: viewDidAppear can re-fire (e.g. returning frontmost),
        // and re-launching would spawn a second guest instance.
        if gameView.surfaceReady, !launched {
            launched = true
            EKA2L1Bridge.shared.setAppExitHandler { [weak self] fatalDetails in
                self?.handleAppExited(fatalDetails: fatalDetails)
            }
            EKA2L1Bridge.shared.launchApp(uid: uid) { [weak self] success in
                self?.onAppLaunch?(success)
            }
        }
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        peripheralInput.stop()
        EKA2L1Bridge.shared.detachLayer()
    }

    // Closing the screen (and killing the app) is detected at the SwiftUI level
    // in EmulatorView's .onDisappear: UIKit's isMovingFromParent /
    // isBeingDismissed are unreliable for a UIViewControllerRepresentable popped
    // by a SwiftUI NavigationStack (they read false), so viewWillDisappear can't
    // tell a pop from a transient disappear. Backgrounding is handled by
    // scenePhase.
    private func handleAppExited(fatalDetails: String?) {
        EKA2L1Bridge.shared.setAppExitHandler(nil)
        onAppExit?(fatalDetails)
    }
}

import SwiftUI
import UIKit

// Fullscreen emulator screen: the render view fills the whole display (no
// navigation bar / status bar) and the virtual keypad floats above it — the
// keypad never resizes the game picture. Frontend actions that used to live in
// the navigation bar are reachable through the keypad's system menu key.
struct EmulatorView: View {
    let uid: UInt32

    @AppStorage(KeypadDefaults.fullscreenKey) private var fullscreenDisplay = false
    @AppStorage(KeypadDefaults.touchFullscreenKey) private var touchFullscreenDisplay = true
    @AppStorage(KeypadDefaults.orientationLockKey) private var lockGameOrientation = false
    @AppStorage(KeypadDefaults.portraitLayoutKey) private var portraitKeypadLayout = ""
    @AppStorage(KeypadDefaults.landscapeLayoutKey) private var landscapeKeypadLayout = ""
    @AppStorage(KeypadDefaults.opacityKey) private var keypadOpacity = KeypadDefaults.opacity
    @AppStorage("ios.showFPSOverlay") private var showFPSOverlay = true
    @AppStorage("ios.fpsOverlayX") private var fpsOverlayX = -1.0
    @AppStorage("ios.fpsOverlayY") private var fpsOverlayY = -1.0
    @Environment(\.dismiss) private var dismiss
    @State private var guestFatalDetails: String?
    @State private var sessionMessage: String?
    @State private var fpsDragStart: CGPoint?
    @State private var wasIdleTimerDisabled = false
    @State private var hostProxy = EmulatorHostProxy()
    // Per-game guest frame-rate cap (0 = unlimited); loaded on appear and
    // written straight through to the emulator when changed from Game Settings.
    @State private var frameLimit = 0
    // Real Window Server modes are queried once app launch has completed, so
    // the menu reflects the active device and later picks.
    @State private var guestScreenModes: [Int] = []
    @State private var guestScreenMode = -1
    // Whether the booted ROM is touch-driven (S60v5 / Symbian^3+); those use a
    // separate layout preference that defaults to the fullscreen layout.
    @State private var isTouchDevice = false
    // Individual screen-space keypad frames. Keeping them separate lets guest
    // touch input continue in the empty space between customized controls.
    @State private var keypadHitRegions: [CGRect] = []
    @State private var screenSize: CGSize = .zero
    @State private var screenSafeAreaInsets = EdgeInsets()
    @State private var isEditingKeypad = false
    @State private var editingLandscape = false
    @State private var editingKeypadLayout: KeypadLayoutConfiguration?
    @State private var layoutResetImpacts = 0
    @State private var launchFullscreenOverride: Bool?
    // The -LaunchKeypadLayout testing argument seeds the layout only for the
    // first emulator screen of the process; later screens use stored settings.
    @MainActor private static var launchLayoutApplied = false

    private var isFullscreen: Bool {
        if UserDefaults.standard.bool(forKey: "EKA2L1RegressionMode") {
            return false
        }
        return launchFullscreenOverride
            ?? (isTouchDevice ? touchFullscreenDisplay : fullscreenDisplay)
    }

    private var fullscreenSelection: Binding<Bool> {
        Binding(
            get: { isFullscreen },
            set: { value in
                launchFullscreenOverride = nil
                if isTouchDevice {
                    touchFullscreenDisplay = value
                } else {
                    fullscreenDisplay = value
                }
            }
        )
    }

    private var orientationLockSelection: Binding<Bool> {
        Binding(
            get: { lockGameOrientation },
            set: { value in
                lockGameOrientation = value
                DisplayOrientation.apply(isLocked: value)
            }
        )
    }

    // Writing the frame-limit binding pushes the new cap straight to the
    // emulator (which applies it live and persists it per app UID).
    private var frameLimitSelection: Binding<Int> {
        Binding(
            get: { frameLimit },
            set: { newValue in
                frameLimit = newValue
                EKA2L1Bridge.shared.setGuestFrameLimit(appUID: uid, limit: newValue)
            }
        )
    }

    private var guestScreenModeSelection: Binding<Int> {
        Binding(
            get: { guestScreenMode },
            set: { newValue in
                guestScreenMode = newValue
                EKA2L1Bridge.shared.setGuestScreenMode(appUID: uid, mode: newValue) { selectedMode in
                    Task { @MainActor in
                        if selectedMode >= 0 {
                            guestScreenMode = selectedMode
                        }
                    }
                }
            }
        )
    }

    private var menuActions: KeypadMenuActions {
        KeypadMenuActions(
            fullScreen: fullscreenSelection,
            locksOrientation: orientationLockSelection,
            editKeypadLayout: { beginEditingKeypadLayout() },
            guestScreenModes: guestScreenModes,
            guestScreenMode: guestScreenModeSelection,
            frameLimit: frameLimitSelection,
            saveScreenshot: { saveScreenshot() },
            exitGame: {
                EKA2L1Bridge.shared.closeRunningApp()
                dismiss()
            }
        )
    }

    var body: some View {
        GeometryReader { proxy in
            ZStack(alignment: .topLeading) {
                EmulatorControllerView(
                    uid: uid,
                    host: hostProxy,
                    anchorsDisplayTop: !isFullscreen,
                    keypadHitRegions: isEditingKeypad
                        ? [CGRect(origin: .zero, size: proxy.size)]
                        : keypadHitRegions,
                    onAppLaunch: { success in
                        guard success else { return }
                        refreshGuestScreenModes()
                    },
                    onAppExit: { fatalDetails in
                        if let fatalDetails {
                            guestFatalDetails = fatalDetails
                        } else {
                            dismiss()
                        }
                    }
                )
                .ignoresSafeArea()

                if showFPSOverlay && !isEditingKeypad {
                    FPSOverlay()
                        .position(
                            x: overlayPosition(in: proxy.size).x,
                            y: overlayPosition(in: proxy.size).y
                        )
                        .gesture(
                            DragGesture()
                                .onChanged { value in
                                    let start = fpsDragStart ?? overlayPosition(in: proxy.size)
                                    fpsDragStart = start
                                    let point = clampedOverlayPosition(
                                        CGPoint(
                                            x: start.x + value.translation.width,
                                            y: start.y + value.translation.height
                                        ),
                                        in: proxy.size
                                    )
                                    fpsOverlayX = point.x
                                    fpsOverlayY = point.y
                                }
                                .onEnded { _ in
                                    fpsDragStart = nil
                                }
                        )
                        .onAppear {
                            ensureOverlayPosition(in: proxy.size)
                        }
                        .onChange(of: proxy.size) { newSize in
                            ensureOverlayPosition(in: newSize)
                        }
                        .accessibilityLabel(Text("emulator.fps.accessibility"))
                }
            }
            .overlay {
                keypadOverlay(in: proxy.size, safeAreaInsets: proxy.safeAreaInsets)
            }
            .onAppear {
                screenSize = proxy.size
                screenSafeAreaInsets = proxy.safeAreaInsets
            }
            .onChange(of: proxy.size) {
                screenSize = $0
                screenSafeAreaInsets = proxy.safeAreaInsets
                constrainKeypadLayoutToWindow(
                    in: $0,
                    safeAreaInsets: proxy.safeAreaInsets
                )
            }
            .onChange(of: proxy.safeAreaInsets) {
                screenSafeAreaInsets = $0
                constrainKeypadLayoutToWindow(
                    in: proxy.size,
                    safeAreaInsets: $0
                )
            }
        }
        .background(Color.black.ignoresSafeArea())
        .statusBarHidden(true)
        .navigationBarBackButtonHidden(true)
        .toolbar(.hidden, for: .navigationBar)
        .persistentSystemOverlays(.hidden)
        .onAppear {
            wasIdleTimerDisabled = UIApplication.shared.isIdleTimerDisabled
            UIApplication.shared.isIdleTimerDisabled = true
            isTouchDevice = EKA2L1Bridge.shared.currentDeviceIsTouchScreen()
            frameLimit = EKA2L1Bridge.shared.guestFrameLimit(appUID: uid)
            if !Self.launchLayoutApplied,
               let forced = KeypadDefaults.launchArgumentFullscreen() {
                Self.launchLayoutApplied = true
                launchFullscreenOverride = forced
            }
            DisplayOrientation.apply(isLocked: lockGameOrientation)
        }
        .onDisappear {
            // The emulator screen was popped/dismissed (exit menu item or a
            // programmatic dismiss after the app exited). Kill the guest app in
            // lockstep with closing the screen. Drop the exit handler first so
            // the kill's logon doesn't bounce back into a dismiss. SwiftUI's
            // onDisappear fires on navigation removal but not on app
            // backgrounding (that path pauses via scenePhase), so this cleanly
            // means "screen closed". No-op if the app already exited.
            EKA2L1Bridge.shared.setAppExitHandler(nil)
            EKA2L1Bridge.shared.closeRunningApp()
            UIApplication.shared.isIdleTimerDisabled = wasIdleTimerDisabled
            isEditingKeypad = false
            DisplayOrientation.unlock()
        }
        .alert("emulator.guestFatal", isPresented: Binding(
            get: { guestFatalDetails != nil },
            set: { isPresented in
                if !isPresented {
                    guestFatalDetails = nil
                }
            }
        )) {
            Button("common.ok") {
                guestFatalDetails = nil
                dismiss()
            }
        } message: {
            Text(guestFatalDetails ?? "")
        }
        .alert("emulator.notice", isPresented: Binding(
            get: { sessionMessage != nil },
            set: { isPresented in
                if !isPresented {
                    sessionMessage = nil
                }
            }
        )) {
            Button("common.ok") { sessionMessage = nil }
        } message: {
            Text(sessionMessage ?? "")
        }
    }

    // MARK: Keypad overlay

    private func refreshGuestScreenModes() {
        let snapshot = EKA2L1Bridge.shared.guestScreenModeSnapshot()
        guestScreenModes = snapshot.modes
        guestScreenMode = snapshot.current
    }

    @ViewBuilder private func keypadOverlay(
        in size: CGSize,
        safeAreaInsets: EdgeInsets
    ) -> some View {
        let controlSize = CGSize(
            width: size.width + safeAreaInsets.leading + safeAreaInsets.trailing,
            height: size.height + safeAreaInsets.top + safeAreaInsets.bottom
        )

        if isEditingKeypad {
            KeypadLayoutEditor(
                size: size,
                controlSize: controlSize,
                safeAreaInsets: safeAreaInsets,
                configuration: editingLayoutBinding(
                    in: size,
                    safeAreaInsets: safeAreaInsets
                ),
                opacity: $keypadOpacity,
                fullScreen: fullscreenSelection,
                actions: menuActions,
                onReset: {
                    editingKeypadLayout = .classicDefault(
                        in: size,
                        safeAreaInsets: safeAreaInsets
                    )
                    layoutResetImpacts += 1
                },
                onDone: finishEditingKeypadLayout
            )
            .hapticImpact(.medium, trigger: layoutResetImpacts)
        } else {
            VirtualKeypad(
                size: size,
                controlSize: controlSize,
                configuration: keypadConfiguration(
                    in: size,
                    safeAreaInsets: safeAreaInsets
                ),
                fullScreen: isFullscreen,
                actions: menuActions,
                onFramesChange: { frames in
                    if frames != keypadHitRegions {
                        keypadHitRegions = frames
                    }
                }
            )
            .opacity(keypadOpacity)
        }
    }

    private func keypadConfiguration(
        in size: CGSize,
        safeAreaInsets: EdgeInsets
    ) -> KeypadLayoutConfiguration {
        let rawValue = size.width > size.height ? landscapeKeypadLayout : portraitKeypadLayout
        return .decoded(
            rawValue,
            defaultFor: size,
            safeAreaInsets: safeAreaInsets
        )
    }

    private func editingLayoutBinding(
        in size: CGSize,
        safeAreaInsets: EdgeInsets
    ) -> Binding<KeypadLayoutConfiguration> {
        Binding(
            get: {
                editingKeypadLayout ?? keypadConfiguration(
                    in: size,
                    safeAreaInsets: safeAreaInsets
                )
            },
            set: { editingKeypadLayout = $0 }
        )
    }

    private func beginEditingKeypadLayout() {
        let size = screenSize
        guard size.width > 0, size.height > 0 else { return }
        editingLandscape = size.width > size.height
        editingKeypadLayout = keypadConfiguration(
            in: size,
            safeAreaInsets: screenSafeAreaInsets
        )
        isEditingKeypad = true
        hostProxy.viewController?.setHardwareKeyboardCaptureEnabled(false)
        DisplayOrientation.lockCurrent()
    }

    private func finishEditingKeypadLayout() {
        guard let configuration = editingKeypadLayout else { return }
        if editingLandscape {
            landscapeKeypadLayout = configuration.encoded()
        } else {
            portraitKeypadLayout = configuration.encoded()
        }
        editingKeypadLayout = nil
        isEditingKeypad = false
        keypadHitRegions = []
        hostProxy.viewController?.setHardwareKeyboardCaptureEnabled(true)
        DisplayOrientation.apply(isLocked: lockGameOrientation)
    }

    private func constrainKeypadLayoutToWindow(
        in size: CGSize,
        safeAreaInsets: EdgeInsets
    ) {
        guard size.width > 0, size.height > 0 else { return }

        if isEditingKeypad {
            var configuration = editingKeypadLayout ?? keypadConfiguration(
                in: size,
                safeAreaInsets: safeAreaInsets
            )
            configuration.constrainElementsToWindow(
                in: size,
                safeAreaInsets: safeAreaInsets
            )
            editingKeypadLayout = configuration
            return
        }

        let isLandscape = size.width > size.height
        let rawValue = isLandscape ? landscapeKeypadLayout : portraitKeypadLayout
        var configuration = KeypadLayoutConfiguration.decoded(
            rawValue,
            defaultFor: size,
            safeAreaInsets: safeAreaInsets
        )
        configuration.constrainElementsToWindow(
            in: size,
            safeAreaInsets: safeAreaInsets
        )
        let encoded = configuration.encoded()

        if isLandscape {
            if encoded != landscapeKeypadLayout {
                landscapeKeypadLayout = encoded
            }
        } else if encoded != portraitKeypadLayout {
            portraitKeypadLayout = encoded
        }
    }

    // MARK: Actions

    // Snapshot only the render view (no keypad overlay). drawHierarchy uses
    // the window-server snapshot path, which is what captures GL/Metal layer
    // content — CALayer.render(in:) would leave the game picture black.
    private func saveScreenshot() {
        guard let view = hostProxy.viewController?.view else {
            sessionMessage = String(localized: "emulator.screenshot.failed")
            return
        }
        let renderer = UIGraphicsImageRenderer(bounds: view.bounds)
        let image = renderer.image { _ in
            view.drawHierarchy(in: view.bounds, afterScreenUpdates: false)
        }
        UIImageWriteToSavedPhotosAlbum(image, nil, nil, nil)
        sessionMessage = String(localized: "emulator.screenshot.saved")
    }

    // MARK: FPS overlay positioning

    private func overlayPosition(in size: CGSize) -> CGPoint {
        if fpsOverlayX >= 0, fpsOverlayY >= 0 {
            return clampedOverlayPosition(CGPoint(x: fpsOverlayX, y: fpsOverlayY), in: size)
        }
        return defaultOverlayPosition(in: size)
    }

    private func ensureOverlayPosition(in size: CGSize) {
        let point = overlayPosition(in: size)
        if point.x != fpsOverlayX || point.y != fpsOverlayY {
            fpsOverlayX = point.x
            fpsOverlayY = point.y
        }
    }

    private func defaultOverlayPosition(in size: CGSize) -> CGPoint {
        CGPoint(x: max(54, size.width - 56), y: 30)
    }

    private func clampedOverlayPosition(_ point: CGPoint, in size: CGSize) -> CGPoint {
        CGPoint(
            x: min(max(point.x, 54), max(54, size.width - 54)),
            y: min(max(point.y, 30), max(30, size.height - 30))
        )
    }
}

// MARK: - Orientation

// The emulator permits every orientation by default. The in-game switch can
// narrow this to the exact current orientation (including left-vs-right
// landscape). `AppOrientationDelegate` reads this mask from UIKit's
// nonisolated callback, hence the plain global.
nonisolated(unsafe) var lockedInterfaceOrientationMask: UIInterfaceOrientationMask = .portrait

// SwiftUI hosts every screen in system UIHostingControllers we can't subclass,
// and their supportedInterfaceOrientations otherwise caps the window at portrait
// (a navigation push settles there), so requestGeometryUpdate and autorotation
// both refuse landscape even when the app delegate allows it. Route each hosting
// controller in the live chain through the single global authority the first
// time we see it. The concrete UIHostingController<…> classes are only known at
// runtime, hence the per-class swizzle rather than a subclass or Info.plist cap.
@MainActor private var routedOrientationClasses = Set<ObjectIdentifier>()

@MainActor private func routeOrientationThroughLiveControllers(from root: UIViewController?) {
    var vc = root
    var depth = 0
    while let current = vc, depth < 12 {
        let cls: AnyClass = type(of: current)
        if routedOrientationClasses.insert(ObjectIdentifier(cls)).inserted,
           let method = class_getInstanceMethod(cls, #selector(getter: UIViewController.supportedInterfaceOrientations)) {
            let block: @convention(block) (UIViewController) -> UIInterfaceOrientationMask = { _ in
                lockedInterfaceOrientationMask
            }
            method_setImplementation(method, imp_implementationWithBlock(block))
        }
        vc = current.presentedViewController ?? current.children.last
        depth += 1
    }
}

// Pins/releases the interface orientation for the emulator screen (frame +
// keypad).
@MainActor
enum DisplayOrientation {
    static func apply(isLocked: Bool) {
        lockedInterfaceOrientationMask = isLocked ? currentOrientationMask : .all
        routeOrientationThroughLiveControllers(from: activeScene?.keyWindow?.rootViewController)
        rootViewController?.setNeedsUpdateOfSupportedInterfaceOrientations()
    }

    // Layout editing is tied to one orientation, so freeze the current screen
    // for the short lifetime of the editor without changing the user's switch.
    static func lockCurrent() {
        lockedInterfaceOrientationMask = currentOrientationMask
        routeOrientationThroughLiveControllers(from: activeScene?.keyWindow?.rootViewController)
        rootViewController?.setNeedsUpdateOfSupportedInterfaceOrientations()
    }

    // Release back to portrait when leaving the emulator (home screen is
    // portrait-only).
    static func unlock() {
        lockedInterfaceOrientationMask = .portrait
        request(landscape: false)
    }

    private static func request(landscape: Bool) {
        guard let scene = activeScene else { return }
        routeOrientationThroughLiveControllers(from: scene.keyWindow?.rootViewController)
        rootViewController?.setNeedsUpdateOfSupportedInterfaceOrientations()
        scene.requestGeometryUpdate(.iOS(interfaceOrientations: landscape ? .landscape : .portrait))
        // The request is rejected while a navigation transition is running (the
        // emulator screen is usually mid-push), so confirm after it settles and
        // re-request once if it didn't stick.
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.7) {
            guard let scene = activeScene,
                  scene.interfaceOrientation.isLandscape != landscape else { return }
            routeOrientationThroughLiveControllers(from: scene.keyWindow?.rootViewController)
            scene.requestGeometryUpdate(.iOS(interfaceOrientations: landscape ? .landscape : .portrait))
        }
    }

    private static var activeScene: UIWindowScene? {
        let scenes = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }
        return scenes.first { $0.activationState == .foregroundActive } ?? scenes.first
    }

    private static var rootViewController: UIViewController? {
        activeScene?.keyWindow?.rootViewController
    }

    private static var currentOrientationMask: UIInterfaceOrientationMask {
        guard let orientation = activeScene?.interfaceOrientation else {
            return .portrait
        }
        switch orientation {
        case .portrait: return .portrait
        case .portraitUpsideDown: return .portraitUpsideDown
        case .landscapeLeft: return .landscapeLeft
        case .landscapeRight: return .landscapeRight
        default: return .portrait
        }
    }
}

// MARK: - FPS overlay

private struct FPSOverlay: View {
    private let timer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    @State private var fps = 0
    @State private var previousFrameCount: UInt64 = 0
    @State private var previousTimestamp = Date()

    var body: some View {
        Text(verbatim: "\(fps) FPS")
            .font(.system(size: 13, weight: .semibold, design: .monospaced))
            .foregroundStyle(.white)
            .padding(.horizontal, 10)
            .frame(width: 88, height: 34)
            .background(.black.opacity(0.62), in: RoundedRectangle(cornerRadius: 8, style: .continuous))
            .overlay(
                RoundedRectangle(cornerRadius: 8, style: .continuous)
                    .stroke(.white.opacity(0.25), lineWidth: 1)
            )
            .contentShape(Rectangle())
            .onAppear(perform: resetSample)
            .onReceive(timer) { now in
                let frameCount = EKA2L1Bridge.shared.renderedFrameCount()
                let elapsed = max(now.timeIntervalSince(previousTimestamp), 0.001)
                let renderedFrames = frameCount >= previousFrameCount ? frameCount - previousFrameCount : 0
                fps = Int((Double(renderedFrames) / elapsed).rounded())
                previousFrameCount = frameCount
                previousTimestamp = now
            }
    }

    private func resetSample() {
        previousFrameCount = EKA2L1Bridge.shared.renderedFrameCount()
        previousTimestamp = Date()
        fps = 0
    }
}

// MARK: - UIKit bridge

// Lets the SwiftUI screen reach the hosted controller (for the render-view
// screenshot) without retaining it.
@MainActor
final class EmulatorHostProxy {
    weak var viewController: EmulatorViewController?
}

private struct EmulatorControllerView: UIViewControllerRepresentable {
    let uid: UInt32
    let host: EmulatorHostProxy
    let anchorsDisplayTop: Bool
    // Screen-space regions covered by keypad elements; the render view yields
    // touches there so the keys (drawn above it) receive them.
    let keypadHitRegions: [CGRect]
    let onAppLaunch: (Bool) -> Void
    let onAppExit: (String?) -> Void

    func makeUIViewController(context: Context) -> EmulatorViewController {
        let controller = EmulatorViewController(uid: uid)
        controller.onAppLaunch = onAppLaunch
        controller.onAppExit = onAppExit
        controller.anchorsDisplayTop = anchorsDisplayTop
        controller.keypadHitRegions = keypadHitRegions
        host.viewController = controller
        return controller
    }

    func updateUIViewController(_ uiViewController: EmulatorViewController, context: Context) {
        uiViewController.onAppLaunch = onAppLaunch
        uiViewController.onAppExit = onAppExit
        uiViewController.anchorsDisplayTop = anchorsDisplayTop
        uiViewController.keypadHitRegions = keypadHitRegions
        host.viewController = uiViewController
    }
}

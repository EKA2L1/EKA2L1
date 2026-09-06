import QuartzCore
import UIKit

@MainActor
final class ExternalDisplay {
    static let shared = ExternalDisplay()

    private weak var outputView: ExternalGameView?
    private var gameVisible = false
    private var foreground = false
    var onSurfaceChange: (() -> Void)?

    func connect(_ view: ExternalGameView) {
        outputView = view
        refresh()
    }

    func disconnect(_ view: ExternalGameView) {
        guard outputView === view else { return }
        outputView = nil
        refresh()
    }

    func setGameVisible(_ visible: Bool) {
        gameVisible = visible
        foreground = UIApplication.shared.applicationState == .active
        refresh()
    }

    func setForeground(_ active: Bool) {
        foreground = active
        refresh()
    }

    func refresh() {
        let phase = outputView?.window?.windowScene?.activationState
        let enabled = gameVisible && foreground && (phase == .foregroundActive || phase == .foregroundInactive)
        outputView?.isHidden = !enabled
        EKA2L1Bridge.shared.setExternalDisplay(layer: outputView?.renderLayer, enabled: enabled)
        if enabled { onSurfaceChange?() }
    }
}

final class ExternalGameView: UIView {
    override class var layerClass: AnyClass { CAEAGLLayer.self }
    var renderLayer: CAEAGLLayer { layer as! CAEAGLLayer }

    override init(frame: CGRect) {
        super.init(frame: frame)
        isUserInteractionEnabled = false
        backgroundColor = .black
        isOpaque = true
        renderLayer.isOpaque = true
        renderLayer.drawableProperties = [
            kEAGLDrawablePropertyRetainedBacking: false,
            kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8
        ]
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

    override func layoutSubviews() {
        super.layoutSubviews()
        contentScaleFactor = window?.windowScene?.screen.scale ?? 1
        ExternalDisplay.shared.refresh()
    }
}

final class ExternalDisplaySceneDelegate: UIResponder, UIWindowSceneDelegate {
    var window: UIWindow?
    private var gameView: ExternalGameView?

    func scene(_ scene: UIScene, willConnectTo session: UISceneSession,
               options connectionOptions: UIScene.ConnectionOptions) {
        guard let scene = scene as? UIWindowScene,
              session.role == .windowExternalDisplayNonInteractive else { return }
        let controller = UIViewController()
        controller.view.backgroundColor = .black
        let gameView = ExternalGameView(frame: controller.view.bounds)
        gameView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        controller.view.addSubview(gameView)
        let window = UIWindow(windowScene: scene)
        window.frame = scene.coordinateSpace.bounds
        window.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        window.rootViewController = controller
        window.isHidden = false
        self.window = window
        self.gameView = gameView
        ExternalDisplay.shared.connect(gameView)
    }

    func windowScene(_ windowScene: UIWindowScene, didUpdate previousCoordinateSpace: UICoordinateSpace,
                     interfaceOrientation previousInterfaceOrientation: UIInterfaceOrientation,
                     traitCollection previousTraitCollection: UITraitCollection) {
        window?.frame = windowScene.coordinateSpace.bounds
        window?.layoutIfNeeded()
    }

    func sceneDidBecomeActive(_ scene: UIScene) {
        if let gameView { ExternalDisplay.shared.connect(gameView) }
    }

    func sceneWillEnterForeground(_ scene: UIScene) {
        if let gameView { ExternalDisplay.shared.connect(gameView) }
    }

    func sceneDidEnterBackground(_ scene: UIScene) {
        if let gameView { ExternalDisplay.shared.disconnect(gameView) }
    }

    func sceneDidDisconnect(_ scene: UIScene) {
        if let gameView { ExternalDisplay.shared.disconnect(gameView) }
        window?.isHidden = true
        window = nil
        gameView = nil
    }
}

import QuartzCore
import UIKit

extension Notification.Name {
    static let externalGameDisplayChanged = Notification.Name("EKA2L1ExternalGameDisplayChanged")
}

@MainActor
final class ExternalDisplay {
    static let shared = ExternalDisplay()

    private weak var outputView: ExternalGameView?
    private var gameVisible = false
    private var foreground = false
    private(set) var isDisplayingGame = false
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

    func setPointer(position: CGPoint?, pictureSize: CGSize, touching: Bool) {
        outputView?.setPointer(position: position, pictureSize: pictureSize, touching: touching)
    }

    func refresh() {
        let phase = outputView?.window?.windowScene?.activationState
        let enabled = gameVisible && foreground && (phase == .foregroundActive || phase == .foregroundInactive)
        outputView?.isHidden = !enabled
        EKA2L1Bridge.shared.setExternalDisplay(layer: outputView?.renderLayer, enabled: enabled)
        if isDisplayingGame != enabled {
            isDisplayingGame = enabled
            NotificationCenter.default.post(name: .externalGameDisplayChanged, object: nil)
        }
        if enabled { onSurfaceChange?() }
    }
}

final class ExternalGameView: UIView {
    private let cursor = CAShapeLayer()
    private var pointerPosition: CGPoint?
    private var pictureSize = CGSize.zero

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
        let path = UIBezierPath()
        path.move(to: .zero)
        path.addLine(to: CGPoint(x: 0, y: 29))
        path.addLine(to: CGPoint(x: 8, y: 22))
        path.addLine(to: CGPoint(x: 14, y: 34))
        path.addLine(to: CGPoint(x: 20, y: 31))
        path.addLine(to: CGPoint(x: 14, y: 19))
        path.addLine(to: CGPoint(x: 25, y: 19))
        path.close()
        cursor.path = path.cgPath
        cursor.bounds = CGRect(x: 0, y: 0, width: 26, height: 35)
        cursor.anchorPoint = .zero
        cursor.strokeColor = UIColor.black.cgColor
        cursor.lineWidth = 2
        cursor.lineJoin = .round
        cursor.shadowColor = UIColor.black.cgColor
        cursor.shadowOpacity = 0.6
        cursor.shadowRadius = 2
        cursor.shadowOffset = CGSize(width: 0, height: 1)
        cursor.isHidden = true
        layer.addSublayer(cursor)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

    override func layoutSubviews() {
        super.layoutSubviews()
        contentScaleFactor = window?.windowScene?.screen.scale ?? 1
        cursor.contentsScale = contentScaleFactor
        ExternalDisplay.shared.refresh()
        layoutPointer()
    }

    func setPointer(position: CGPoint?, pictureSize: CGSize, touching: Bool) {
        pointerPosition = position
        self.pictureSize = pictureSize
        CATransaction.begin()
        CATransaction.setDisableActions(true)
        cursor.fillColor = touching ? UIColor.systemCyan.cgColor : UIColor.white.cgColor
        layoutPointer()
        CATransaction.commit()
    }

    private func layoutPointer() {
        CATransaction.begin()
        CATransaction.setDisableActions(true)
        cursor.isHidden = pointerPosition == nil || pictureSize.width <= 0 || pictureSize.height <= 0
        if let point = pointerPosition {
            let rect = ControllerPointerState.fittedRect(size: pictureSize, in: bounds)
            cursor.position = CGPoint(x: rect.minX + point.x * max(rect.width - 1, 0),
                                      y: rect.minY + point.y * max(rect.height - 1, 0))
        }
        CATransaction.commit()
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

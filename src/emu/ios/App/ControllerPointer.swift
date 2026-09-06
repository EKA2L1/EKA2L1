import Foundation

enum ControllerPointerAction: String, CaseIterable, Codable, Identifiable {
    case toggle, touch, up, down, left, right

    var id: String { rawValue }

    var name: String {
        switch self {
        case .toggle: return String(localized: "controllerPointer.toggle")
        case .touch: return String(localized: "controllerPointer.touch")
        case .up: return String(localized: "controllerPointer.up")
        case .down: return String(localized: "controllerPointer.down")
        case .left: return String(localized: "controllerPointer.left")
        case .right: return String(localized: "controllerPointer.right")
        }
    }

    var symbol: String {
        switch self {
        case .toggle: return "cursorarrow"
        case .touch: return "hand.tap"
        case .up: return "arrow.up"
        case .down: return "arrow.down"
        case .left: return "arrow.left"
        case .right: return "arrow.right"
        }
    }
}

struct ControllerFeatures: Codable, Equatable {
    var pointerEnabled = true
    var motionEnabled = true
    var vibrationEnabled = true
    var pointerBindings: [String: ControllerPointerAction] = [
        HostButton.rightThumbstickButton.rawValue: .toggle,
        HostButton.buttonA.rawValue: .touch,
        HostButton.dpadUp.rawValue: .up,
        HostButton.dpadDown.rawValue: .down,
        HostButton.dpadLeft.rawValue: .left,
        HostButton.dpadRight.rawValue: .right,
        HostButton.leftStickUp.rawValue: .up,
        HostButton.leftStickDown.rawValue: .down,
        HostButton.leftStickLeft.rawValue: .left,
        HostButton.leftStickRight.rawValue: .right,
    ]

    private static let storageKey = "ios.controllerFeatures"

    static func load(deviceKey: String) -> Self {
        guard let data = UserDefaults.standard.dictionary(forKey: storageKey)?[deviceKey] as? Data,
              var features = try? JSONDecoder().decode(Self.self, from: data) else {
            return Self()
        }
        features.pointerBindings = features.pointerBindings.filter { HostButton(rawValue: $0.key) != nil }
        return features
    }

    func save(deviceKey: String) {
        guard let data = try? JSONEncoder().encode(self) else { return }
        var all = UserDefaults.standard.dictionary(forKey: Self.storageKey) ?? [:]
        all[deviceKey] = data
        UserDefaults.standard.set(all, forKey: Self.storageKey)
    }

    mutating func bind(token: String, to action: ControllerPointerAction) {
        unbind(action)
        pointerBindings[token] = action
    }

    mutating func unbind(_ action: ControllerPointerAction) {
        pointerBindings = pointerBindings.filter { $0.value != action }
    }

    func actions(for tokens: Set<String>) -> Set<ControllerPointerAction> {
        Set(tokens.compactMap { pointerBindings[$0] })
    }
}

struct ControllerPointerState {
    enum TouchPhase: Equatable { case began, moved, ended, cancelled }

    private(set) var position = CGPoint(x: 0.5, y: 0.5)
    private(set) var isVisible = false
    private(set) var isTouching = false
    private var actions: Set<ControllerPointerAction> = []
    private var touchNeedsRelease = false

    mutating func prime(actions: Set<ControllerPointerAction>) {
        self.actions = actions
        touchNeedsRelease = actions.contains(.touch)
    }

    mutating func update(actions next: Set<ControllerPointerAction>) -> [TouchPhase] {
        let newlyPressed = next.subtracting(actions)
        actions = next
        if !next.contains(.touch) { touchNeedsRelease = false }

        if newlyPressed.contains(.toggle) {
            let events = cancelTouch()
            isVisible.toggle()
            touchNeedsRelease = next.contains(.touch)
            return events
        }
        guard isVisible else { return [] }
        if isTouching, !next.contains(.touch) {
            isTouching = false
            return [.ended]
        }
        if newlyPressed.contains(.touch), !touchNeedsRelease {
            isTouching = true
            return [.began]
        }
        return []
    }

    mutating func move(elapsed: TimeInterval, size: CGSize) -> [TouchPhase] {
        guard isVisible, size.width > 1, size.height > 1 else { return [] }
        var x: CGFloat = (actions.contains(.right) ? 1 : 0) - (actions.contains(.left) ? 1 : 0)
        var y: CGFloat = (actions.contains(.down) ? 1 : 0) - (actions.contains(.up) ? 1 : 0)
        let length = hypot(x, y)
        guard length > 0 else { return [] }
        let distance = min(size.width, size.height) * 0.7 * min(max(elapsed, 0), 0.05)
        x = x / length * distance / (size.width - 1)
        y = y / length * distance / (size.height - 1)
        let next = CGPoint(x: min(max(position.x + x, 0), 1),
                           y: min(max(position.y + y, 0), 1))
        guard next != position else { return [] }
        position = next
        return isTouching ? [.moved] : []
    }

    mutating func cancelTouch() -> [TouchPhase] {
        touchNeedsRelease = actions.contains(.touch)
        guard isTouching else { return [] }
        isTouching = false
        return [.cancelled]
    }

    mutating func reset() -> [TouchPhase] {
        let events = cancelTouch()
        isVisible = false
        actions.removeAll()
        return events
    }

    func location(in rect: CGRect) -> CGPoint {
        CGPoint(x: rect.minX + position.x * max(rect.width - 1, 0),
                y: rect.minY + position.y * max(rect.height - 1, 0))
    }

    static func fittedRect(size: CGSize, in bounds: CGRect) -> CGRect {
        guard size.width > 0, size.height > 0 else { return .zero }
        let scale = min(bounds.width / size.width, bounds.height / size.height)
        let fitted = CGSize(width: size.width * scale, height: size.height * scale)
        return CGRect(x: bounds.midX - fitted.width / 2, y: bounds.midY - fitted.height / 2,
                      width: fitted.width, height: fitted.height)
    }
}

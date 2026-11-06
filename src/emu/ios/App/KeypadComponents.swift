import SwiftUI
import UIKit

// Shared building blocks for the on-screen keypad layouts in VirtualKeypad.swift:
// scan codes, the press/release key primitive, key-cap styling, the d-pad and
// the numeric pads.

// Symbian standard scan codes (see services/window/keys.h).
enum Scan {
    static let leftSoft: UInt32 = 0xA4   // std_key_device_0
    static let rightSoft: UInt32 = 0xA5  // std_key_device_1
    static let select: UInt32 = 0xA7     // std_key_device_3
    static let clear: UInt32 = 0x01      // std_key_backspace (guest "C" key)
    static let up: UInt32 = 0x10
    static let down: UInt32 = 0x11
    static let left: UInt32 = 0x0E
    static let right: UInt32 = 0x0F
    static let hash: UInt32 = 0x7F
    static let star: UInt32 = 0x2A
    static let call: UInt32 = 0xB4       // std_key_application_0 (green call)
    static let end: UInt32 = 0xB5        // std_key_application_1 (red end)
    static let edit: UInt32 = 0x12     // std_key_left_shift
}

// Digit, the phone-style letters under it, and the raw scan code. Shared by
// every numeric pad so they all stay identical.
let keypadDigits: [(label: String, sub: String, scan: UInt32)] = [
    ("1", "", 0x31), ("2", "ABC", 0x32), ("3", "DEF", 0x33),
    ("4", "GHI", 0x34), ("5", "JKL", 0x35), ("6", "MNO", 0x36),
    ("7", "PQRS", 0x37), ("8", "TUV", 0x38), ("9", "WXYZ", 0x39),
    ("\u{2217}", "", Scan.star), ("0", "+", 0x30), ("#", "", Scan.hash)
]

// MARK: - Key primitive

struct HoldableRawKey<Label: View>: View {
    let scan: UInt32
    // Hit-test region for the key. Defaults to the full bounding rect; round
    // keys pass a precise shape so neighbouring keys don't overlap.
    var hitShape: AnyShape = AnyShape(Rectangle())
    @ViewBuilder let label: (Bool) -> Label

    @State private var pressed = false
    @State private var sentDown = false
    @State private var impacts = 0

    var body: some View {
        label(pressed)
            .contentShape(hitShape)
            .accessibilityElement(children: .combine)
            .accessibilityAddTraits(.isButton)
            .accessibilityAction {
                EKA2L1Bridge.shared.tapRawKey(scan)
            }
            // SwiftUI's zero-distance DragGesture fires late on iOS 27; UIKit
            // touchesBegan reports the press as soon as the finger lands.
            .overlay(
                KeyTouchSurface(hitShape: hitShape, onPress: press, onRelease: release)
                    .accessibilityHidden(true)
            )
            .onDisappear(perform: release)
            .hapticImpact(.light, trigger: impacts)
    }

    private func press() {
        guard !sentDown else { return }
        sentDown = true
        pressed = true
        impacts += 1
        EKA2L1Bridge.shared.submitRawKey(scan, pressed: true)
    }

    private func release() {
        guard sentDown else { return }
        sentDown = false
        pressed = false
        EKA2L1Bridge.shared.submitRawKey(scan, pressed: false)
    }
}

private struct KeyTouchSurface: UIViewRepresentable {
    let hitShape: AnyShape
    let onPress: () -> Void
    let onRelease: () -> Void

    func makeUIView(context: Context) -> KeyTouchView {
        let view = KeyTouchView()
        configure(view)
        return view
    }

    func updateUIView(_ view: KeyTouchView, context: Context) {
        configure(view)
    }

    static func dismantleUIView(_ view: KeyTouchView, coordinator: ()) {
        view.reset()
    }

    private func configure(_ view: KeyTouchView) {
        view.hitShape = hitShape
        view.onPress = onPress
        view.onRelease = onRelease
    }
}

private final class KeyTouchView: UIView {
    var hitShape: AnyShape = AnyShape(Rectangle())
    var onPress: (() -> Void)?
    var onRelease: (() -> Void)?

    // The key stays held until every finger that landed on it has lifted,
    // even if those fingers slide outside the key.
    private var activeTouches: Set<ObjectIdentifier> = []

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .clear
        isMultipleTouchEnabled = true
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        backgroundColor = .clear
        isMultipleTouchEnabled = true
    }

    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        hitShape.path(in: bounds).contains(point)
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        let wasIdle = activeTouches.isEmpty
        for touch in touches {
            activeTouches.insert(ObjectIdentifier(touch))
        }
        if wasIdle && !activeTouches.isEmpty {
            onPress?()
        }
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        remove(touches)
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        remove(touches)
    }

    func reset() {
        guard !activeTouches.isEmpty else { return }
        activeTouches.removeAll()
        onRelease?()
    }

    private func remove(_ touches: Set<UITouch>) {
        guard !activeTouches.isEmpty else { return }
        for touch in touches {
            activeTouches.remove(ObjectIdentifier(touch))
        }
        if activeTouches.isEmpty {
            onRelease?()
        }
    }
}

// MARK: - Key-cap styling

private struct KeyCapModifier: ViewModifier {
    let pressed: Bool

    func body(content: Content) -> some View {
        if #available(iOS 26.0, *) {
            content
                .foregroundStyle(.white)
                .glassEffect(.regular.interactive(),
                             in: RoundedRectangle(cornerRadius: 14, style: .continuous))
        } else {
            legacyBody(content)
        }
    }

    private func legacyBody(_ content: Content) -> some View {
        content
            .foregroundStyle(.white)
            .background(
                LegacyKeySurface(shape: RoundedRectangle(cornerRadius: 12, style: .continuous),
                                 pressed: pressed)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .strokeBorder(.white.opacity(0.12), lineWidth: 1)
            )
            .scaleEffect(pressed ? 0.93 : 1)
            .animation(.easeOut(duration: 0.12), value: pressed)
    }
}

// Pre-26 key surface: a light blur so keys keep some body over busy screens.
// Pressed keys thicken the blur.
private struct LegacyKeySurface<S: Shape>: View {
    let shape: S
    var pressed = false

    var body: some View {
        shape
            .fill(pressed ? .regularMaterial : .thinMaterial)
            .environment(\.colorScheme, .dark)
    }
}

extension View {
    func keyCap(pressed: Bool) -> some View {
        modifier(KeyCapModifier(pressed: pressed))
    }
}

// Fixed-size cap key sending a raw scan code — soft keys, clear key, etc.
struct CapKey: View {
    let scan: UInt32
    var title: String?
    var symbol: String?
    var tint: Color?
    var size: CGSize = CGSize(width: 58, height: 38)

    var body: some View {
        HoldableRawKey(scan: scan) { pressed in
            Group {
                if let title {
                    Text(title)
                        .font(.system(size: min(18, max(13, size.height * 0.4)),
                                      weight: .semibold,
                                      design: .rounded))
                } else if let symbol {
                    Image(systemName: symbol)
                        .font(.system(size: 16, weight: .semibold))
                        .foregroundStyle(tint ?? .white)
                }
            }
            .frame(width: size.width, height: size.height)
            .keyCap(pressed: pressed)
        }
    }
}

// Left/right soft key. The visible labels stay deliberately plain while the
// accessibility labels remain "LSK"/"RSK" for the regression harness.
struct SoftKey: View {
    enum Side {
        case left, right
    }

    let side: Side
    var size: CGSize = CGSize(width: 58, height: 38)

    var body: some View {
        CapKey(scan: side == .left ? Scan.leftSoft : Scan.rightSoft,
               title: side == .left ? "L" : "R",
               size: size)
            .accessibilityLabel(Text(verbatim: side == .left ? "LSK" : "RSK"))
    }
}

// Green and red phone keys: EStdKeyApplication0 and EStdKeyApplication1.
struct PhoneKey: View {
    enum Side {
        case call, end
    }

    let side: Side
    var size: CGSize = CGSize(width: 58, height: 38)

    var body: some View {
        CapKey(scan: side == .call ? Scan.call : Scan.end,
               symbol: side == .call ? "phone.fill" : "phone.down.fill",
               tint: side == .call ? .green : .red,
               size: size)
            .accessibilityLabel(Text(side == .call ? LocalizedStringKey("keypad.accessibility.call")
                                                   : LocalizedStringKey("keypad.accessibility.end")))
    }
}

struct EditKey: View {
    var size: CGSize = CGSize(width: 58, height: 38)

    var body: some View {
        CapKey(scan: Scan.edit, symbol: "pencil", size: size)
            .accessibilityLabel(Text("key.edit"))
    }
}

// The clear ("C") key — same guest key as hardware backspace.
struct ClearKey: View {
    var size: CGSize = CGSize(width: 58, height: 38)

    var body: some View {
        CapKey(scan: Scan.clear, symbol: "delete.left.fill", size: size)
            .accessibilityLabel(Text("keypad.accessibility.clear"))
    }
}

// MARK: - D-pad

enum DirectionPadMode: String {
    case cross, stick

    var title: LocalizedStringKey {
        switch self {
        case .cross: "keypad.editor.dpadMode.buttons"
        case .stick: "keypad.editor.dpadMode.joystick"
        }
    }
}

struct DirectionPad: View {
    var mode: DirectionPadMode = .cross
    let diameter: CGFloat
    var isInteractive = true

    @State private var state = DPadState()
    @State private var activeScans: Set<UInt32> = []
    @State private var impacts = 0

    var body: some View {
        ZStack {
            switch mode {
            case .cross:
                CrossPadFace(state: state, diameter: diameter)
                // SwiftUI's DragGesture tracks one touch and starts late; the
                // UIKit surface sees every finger as it lands.
                if isInteractive {
                    DPadTouchSurface<CrossTouchView>(onChange: update, onTap: tapOK)
                        .accessibilityHidden(true)
                }
            case .stick:
                StickPadFace(state: state, diameter: diameter)
                if isInteractive {
                    DPadTouchSurface<StickTouchView>(onChange: update, onTap: tapOK)
                        .accessibilityHidden(true)
                }
            }
        }
        .frame(width: diameter, height: diameter)
        .onDisappear {
            update(DPadState())
        }
        .hapticImpact(.light, trigger: impacts)
    }

    private func update(_ new: DPadState) {
        // Keys and highlights follow the finger without easing; only the
        // stick knob springs back when released.
        let recentering = state.knobOffset != .zero && new.knobOffset == .zero
        // The stick ticks on every one of its eight sectors, including a
        // diagonal settling onto a cardinal, which presses no new key.
        let enteredSector = new.sector != nil && new.sector != state.sector
        withTransaction(Transaction(animation: recentering
                                    ? .spring(response: 0.28, dampingFraction: 0.62) : nil)) {
            state = new
        }

        var scans = Set(new.directions.map(\.scan))
        if new.okPressed {
            scans.insert(Scan.select)
        }
        guard scans != activeScans else { return }
        for scan in activeScans.subtracting(scans).sorted() {
            EKA2L1Bridge.shared.submitRawKey(scan, pressed: false)
        }
        let pressedScans = scans.subtracting(activeScans)
        for scan in pressedScans.sorted() {
            EKA2L1Bridge.shared.submitRawKey(scan, pressed: true)
        }
        if !pressedScans.isEmpty || enteredSector {
            impacts += 1
        }
        activeScans = scans
    }

    private func tapOK() {
        EKA2L1Bridge.shared.tapRawKey(Scan.select)
        impacts += 1
    }
}

// Proportions shared by the faces and their touch views. Ratios are of the
// pad's radius unless noted.
private enum DPadLayout {
    // Half-width of each arm and the OK square.
    static let crossArmHalfWidth: CGFloat = 0.34
    // How far past the centre square a finger that started on OK may wander
    // before it steers.
    static let crossOKSlack: CGFloat = 0.08
    // Centre dead zone for fingers that started on a direction.
    static let crossDeadZone: CGFloat = 0.18
    static let crossDeadZoneRelease: CGFloat = 0.13
    // Extra degrees a finger must pass an arm boundary before switching.
    static let crossSectorHysteresis: Double = 8

    // Stick
    static let stickRingWidth: CGFloat = 1.5  // points
    static let stickKnobRadius: CGFloat = 0.56
    // Knob travel; the knob's edge meets the ring at full tilt.
    static let stickTravel: CGFloat = 0.44
    // Dead zone as a fraction of travel, with hysteresis.
    static let stickEngage: CGFloat = 0.4
    static let stickRelease: CGFloat = 0.28
    static let stickSectorHysteresis: Double = 7
    // Points a finger on the knob may move and still count as a tap.
    static let stickTapSlop: CGFloat = 10
    static let stickHoldDelay: UInt64 = 300_000_000
}

private enum PadDirection: CaseIterable {
    case right, down, left, up

    var scan: UInt32 {
        switch self {
        case .right: Scan.right
        case .down: Scan.down
        case .left: Scan.left
        case .up: Scan.up
        }
    }

    var symbol: String {
        switch self {
        case .right: "arrowtriangle.right.fill"
        case .down: "arrowtriangle.down.fill"
        case .left: "arrowtriangle.left.fill"
        case .up: "arrowtriangle.up.fill"
        }
    }

    // Sector 0 is +x and sectors advance clockwise in screen space.
    var sector: Int {
        switch self {
        case .right: 0
        case .down: 2
        case .left: 4
        case .up: 6
        }
    }

    var unit: CGPoint {
        let angle = Double(sector) * .pi / 4
        return CGPoint(x: cos(angle), y: sin(angle))
    }

    // Diagonal sectors hold both neighbouring directions.
    static func directions(for sector: Int?) -> Set<PadDirection> {
        guard let sector else { return [] }
        return Set(allCases.filter { abs(sectorDistance(sector, $0.sector)) <= 1 })
    }

    static func sectorDistance(_ a: Int, _ b: Int) -> Int {
        (a - b + 12) % 8 - 4
    }
}

private struct DPadState: Equatable {
    var directions: Set<PadDirection> = []
    var okPressed = false
    // Cross only: the key under each finger, in the order they landed.
    var fingers: [CrossFinger] = []
    // Stick only.
    var sector: Int?
    // Centre of the ring highlight in degrees. Unwrapped so it sweeps the
    // short way round; a new session starts each time a direction engages.
    var arcDegrees: Double = 0
    var arcSession = 0
    var knobOffset: CGSize = .zero
    var knobTouched = false
}

private extension View {
    func okKeyAccessibility() -> some View {
        accessibilityElement(children: .ignore)
            .accessibilityLabel(Text(verbatim: "OK"))
            .accessibilityAddTraits(.isButton)
            .accessibilityAction {
                EKA2L1Bridge.shared.tapRawKey(Scan.select)
            }
    }
}

// MARK: Cross face

// A key on the cross: one of the arms or the centre OK square.
private enum CrossKey: Hashable {
    case arm(PadDirection)
    case ok
}

private struct CrossFinger: Equatable {
    let id: Int
    let key: CrossKey
}

private struct CrossPadFace: View {
    let state: DPadState
    let diameter: CGFloat

    var body: some View {
        let radius = diameter / 2
        let content = ZStack {
            platters

            ForEach(PadDirection.allCases, id: \.self) { direction in
                glyph(direction)
                    .offset(x: direction.unit.x * radius * 0.7,
                            y: direction.unit.y * radius * 0.7)
            }

            okLabel
                .frame(width: diameter * DPadLayout.crossArmHalfWidth,
                       height: diameter * DPadLayout.crossArmHalfWidth)
                .okKeyAccessibility()
        }
        .frame(width: diameter, height: diameter)

        if #available(iOS 26.0, *) {
            content
                .glassEffect(.regular, in: CrossShape())
        } else {
            content
                .background(
                    LegacyKeySurface(shape: CrossShape())
                        .overlay(CrossShape().stroke(.white.opacity(0.15), lineWidth: 1))
                )
        }
    }

    // Like a menu's selection: every finger has its own platter, which
    // glides along when that finger slides to another key. A new finger's
    // platter appears in place; a lifted finger's fades where it was.
    private var platters: some View {
        let half = diameter / 2 * DPadLayout.crossArmHalfWidth
        let inset = half * 0.12
        return ZStack {
            ForEach(state.fingers, id: \.id) { finger in
                let frame = platterFrame(finger.key, inset: inset)
                RoundedRectangle(cornerRadius: half * 0.45 - inset, style: .continuous)
                    .fill(.white.opacity(0.22))
                    .frame(width: frame.width, height: frame.height)
                    .offset(x: frame.midX, y: frame.midY)
                    .transition(.opacity.combined(with: .scale(scale: 0.9)))
            }
        }
        .animation(.spring(response: 0.3, dampingFraction: 0.82), value: state.fingers)
    }

    // The key's rect relative to the pad's centre, shrunk by inset.
    private func platterFrame(_ key: CrossKey, inset: CGFloat) -> CGRect {
        let radius = diameter / 2
        let half = radius * DPadLayout.crossArmHalfWidth
        let length = radius - half
        let rect: CGRect
        switch key {
        case .ok:
            rect = CGRect(x: -half, y: -half, width: half * 2, height: half * 2)
        case .arm(let direction):
            let vertical = direction == .up || direction == .down
            let size = CGSize(width: vertical ? half * 2 : length,
                              height: vertical ? length : half * 2)
            let centre = CGPoint(x: direction.unit.x * (half + length / 2),
                                 y: direction.unit.y * (half + length / 2))
            rect = CGRect(x: centre.x - size.width / 2, y: centre.y - size.height / 2,
                          width: size.width, height: size.height)
        }
        return rect.insetBy(dx: inset, dy: inset)
    }

    private func glyph(_ direction: PadDirection) -> some View {
        let lit = state.directions.contains(direction)
        return Image(systemName: direction.symbol)
            .font(.system(size: diameter * 0.075))
            .foregroundStyle(.white.opacity(lit ? 1 : 0.7))
    }

    private var okLabel: some View {
        Image(systemName: "circle")
            .font(.system(size: diameter * 0.09, weight: .semibold))
            .foregroundStyle(.white.opacity(state.okPressed ? 1 : 0.7))
    }
}

// Plus sign with rounded arm ends and softened inner corners.
private struct CrossShape: Shape {
    func path(in rect: CGRect) -> Path {
        let r = min(rect.width, rect.height) / 2
        let h = r * DPadLayout.crossArmHalfWidth
        let outer = h * 0.45
        let inner = h * 0.2
        let corners: [(x: CGFloat, y: CGFloat, radius: CGFloat)] = [
            (-h, -r, outer), (h, -r, outer), (h, -h, inner),
            (r, -h, outer), (r, h, outer), (h, h, inner),
            (h, r, outer), (-h, r, outer), (-h, h, inner),
            (-r, h, outer), (-r, -h, outer), (-h, -h, inner),
        ]
        let points = corners.map { CGPoint(x: rect.midX + $0.x, y: rect.midY + $0.y) }

        var path = Path()
        path.move(to: CGPoint(x: (points[points.count - 1].x + points[0].x) / 2,
                              y: (points[points.count - 1].y + points[0].y) / 2))
        for index in points.indices {
            path.addArc(tangent1End: points[index],
                        tangent2End: points[(index + 1) % points.count],
                        radius: corners[index].radius)
        }
        path.closeSubpath()
        return path
    }
}

// MARK: Stick face

private struct StickPadFace: View {
    let state: DPadState
    let diameter: CGFloat

    private var usesGlass: Bool {
        if #available(iOS 26.0, *) { true } else { false }
    }

    var body: some View {
        let ringWidth = DPadLayout.stickRingWidth
        ZStack {
            Circle()
                .strokeBorder(.white.opacity(0.35), lineWidth: ringWidth)

            // The active sector thickens the ring under it and glides round
            // when the direction changes; it appears in place on engage.
            if state.sector != nil {
                SectorArc(centre: state.arcDegrees)
                    .stroke(.white, style: StrokeStyle(lineWidth: ringWidth * 3, lineCap: .round))
                    .animation(.spring(response: 0.24, dampingFraction: 0.78), value: state.arcDegrees)
                    .padding(ringWidth / 2)
                    .shadow(color: .white.opacity(usesGlass ? 0.5 : 0), radius: 4)
                    .id(state.arcSession)
            }

            knob
        }
    }

    @ViewBuilder private var knob: some View {
        let knobSize = diameter * DPadLayout.stickKnobRadius
        let touched = state.knobTouched
        if #available(iOS 26.0, *) {
            knobLabel(size: knobSize)
                .background(Circle().fill(.white.opacity(touched ? 0.2 : 0)))
                .glassEffect(.regular, in: Circle())
                .scaleEffect(touched ? 0.92 : 1)
                .animation(.spring(response: 0.18, dampingFraction: 0.62), value: touched)
                .offset(state.knobOffset)
                .okKeyAccessibility()
        } else {
            knobLabel(size: knobSize)
                .background(LegacyKeySurface(shape: Circle(), pressed: touched))
                .overlay(Circle().strokeBorder(.white.opacity(0.25), lineWidth: 1))
                .shadow(color: .black.opacity(0.3), radius: 6, y: 2)
                .scaleEffect(touched ? 0.9 : 1)
                .animation(.spring(response: 0.18, dampingFraction: 0.62), value: touched)
                .offset(state.knobOffset)
                .okKeyAccessibility()
        }
    }

    private func knobLabel(size: CGFloat) -> some View {
        Text(verbatim: "OK")
            .font(.system(size: size * 0.22, weight: .bold, design: .rounded))
            .foregroundStyle(.white)
            .frame(width: size, height: size)
    }
}

// Rim highlight spanning one 45° sector, centred on `centre` degrees.
private struct SectorArc: Shape {
    var centre: Double

    var animatableData: Double {
        get { centre }
        set { centre = newValue }
    }

    func path(in rect: CGRect) -> Path {
        var path = Path()
        path.addArc(center: CGPoint(x: rect.midX, y: rect.midY),
                    radius: min(rect.width, rect.height) / 2,
                    startAngle: .degrees(centre - 18), endAngle: .degrees(centre + 18),
                    clockwise: false)
        return path
    }
}

// MARK: Touch handling

private struct DPadTouchSurface<TouchView: DPadTouchView>: UIViewRepresentable {
    let onChange: (DPadState) -> Void
    let onTap: () -> Void

    func makeUIView(context: Context) -> TouchView {
        let view = TouchView(frame: .zero)
        configure(view)
        return view
    }

    func updateUIView(_ view: TouchView, context: Context) {
        configure(view)
    }

    static func dismantleUIView(_ view: TouchView, coordinator: ()) {
        view.reset()
    }

    private func configure(_ view: TouchView) {
        view.onChange = onChange
        view.onTap = onTap
    }
}

private class DPadTouchView: UIView {
    var onChange: ((DPadState) -> Void)?
    var onTap: (() -> Void)?

    private var published = DPadState()

    required override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .clear
        isMultipleTouchEnabled = true
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        backgroundColor = .clear
        isMultipleTouchEnabled = true
    }

    var radius: CGFloat {
        min(bounds.width, bounds.height) / 2
    }

    func vector(_ point: CGPoint) -> CGVector {
        CGVector(dx: point.x - bounds.midX, dy: point.y - bounds.midY)
    }

    func distance(_ point: CGPoint) -> CGFloat {
        hypot(point.x - bounds.midX, point.y - bounds.midY)
    }

    func degrees(_ point: CGPoint) -> Double {
        let v = vector(point)
        return atan2(v.dy, v.dx) * 180 / .pi
    }

    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        distance(point) <= radius
    }

    func reset() {}

    func publish(_ state: DPadState) {
        guard state != published else { return }
        published = state
        onChange?(state)
    }
}

private final class CrossTouchView: DPadTouchView {
    private enum Zone: Equatable {
        case ok
        case dead
        case sector(Int)
    }

    // Each finger keeps its own zone so hysteresis is per finger. Fingers
    // keep steering by angle after sliding off the pad.
    private var zones: [ObjectIdentifier: Zone] = [:]
    // A fresh serial per finger names its highlight platter.
    private var serials: [ObjectIdentifier: Int] = [:]
    private var nextSerial = 0

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        for touch in touches {
            let id = ObjectIdentifier(touch)
            let point = touch.location(in: self)
            zones[id] = inCentre(point, slack: 0)
                ? .ok
                : .sector(sector(at: point, current: nil))
            serials[id] = nextSerial
            nextSerial += 1
        }
        publishZones()
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        for touch in touches {
            let id = ObjectIdentifier(touch)
            guard let zone = zones[id] else { continue }
            zones[id] = nextZone(from: zone, at: touch.location(in: self))
        }
        publishZones()
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        end(touches)
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        end(touches)
    }

    override func reset() {
        zones.removeAll()
        serials.removeAll()
        publishZones()
    }

    private func end(_ touches: Set<UITouch>) {
        for touch in touches {
            zones.removeValue(forKey: ObjectIdentifier(touch))
            serials.removeValue(forKey: ObjectIdentifier(touch))
        }
        publishZones()
    }

    // Inside the centre square, grown by slack (a fraction of the radius).
    private func inCentre(_ point: CGPoint, slack: CGFloat) -> Bool {
        let v = vector(point)
        return max(abs(v.dx), abs(v.dy)) <= radius * (DPadLayout.crossArmHalfWidth + slack)
    }

    private func nextZone(from zone: Zone, at point: CGPoint) -> Zone {
        let d = distance(point) / radius
        switch zone {
        case .ok:
            if inCentre(point, slack: DPadLayout.crossOKSlack) {
                return .ok
            }
            return .sector(sector(at: point, current: nil))
        case .dead:
            return d < DPadLayout.crossDeadZone ? .dead : .sector(sector(at: point, current: nil))
        case .sector(let current):
            return d < DPadLayout.crossDeadZoneRelease
                ? .dead : .sector(sector(at: point, current: current))
        }
    }

    // Four-way: always a cardinal sector (0, 2, 4, 6), never a diagonal.
    private func sector(at point: CGPoint, current: Int?) -> Int {
        let angle = degrees(point)
        let nearest = (Int((angle / 90).rounded()) % 4 + 4) % 4 * 2
        guard let current else { return nearest }
        var delta = abs(angle - Double(current) * 45).truncatingRemainder(dividingBy: 360)
        delta = min(delta, 360 - delta)
        return delta > 45 + DPadLayout.crossSectorHysteresis ? nearest : current
    }

    private func publishZones() {
        var state = DPadState()
        for (id, zone) in zones.sorted(by: { serials[$0.key, default: 0] < serials[$1.key, default: 0] }) {
            let serial = serials[id, default: 0]
            switch zone {
            case .ok:
                state.okPressed = true
                state.fingers.append(CrossFinger(id: serial, key: .ok))
            case .dead:
                break
            case .sector(let sector):
                let directions = PadDirection.directions(for: sector)
                state.directions.formUnion(directions)
                // Four-way, so a sector is always exactly one arm.
                if let direction = directions.first {
                    state.fingers.append(CrossFinger(id: serial, key: .arm(direction)))
                }
            }
        }
        publish(state)
    }
}

private final class StickTouchView: DPadTouchView {
    private enum Phase {
        case idle
        // On the knob, not yet a drag: lifting taps OK.
        case pending(start: CGPoint)
        // Rested on the knob long enough to hold OK.
        case holding(start: CGPoint)
        case steering(anchor: CGPoint)
    }

    // The stick follows one finger; others are ignored until it lifts.
    private var touch: ObjectIdentifier?
    private var phase = Phase.idle
    private var sector: Int? {
        didSet {
            guard let sector, sector != oldValue else { return }
            let target = Double(sector) * 45
            if oldValue == nil {
                arcSession += 1
                arcDegrees = target
            } else {
                arcDegrees += remainder(target - arcDegrees, 360)
            }
        }
    }
    private var arcDegrees: Double = 0
    private var arcSession = 0
    private var knob: CGSize = .zero
    private var holdTask: Task<Void, Never>?

    private var travel: CGFloat {
        radius * DPadLayout.stickTravel
    }

    // Only the knob takes new touches; the ring is just a guide.
    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        distance(point) <= radius * DPadLayout.stickKnobRadius
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard touch == nil, let first = touches.first else { return }
        touch = ObjectIdentifier(first)
        phase = .pending(start: first.location(in: self))
        holdTask = Task { [weak self] in
            try? await Task.sleep(nanoseconds: DPadLayout.stickHoldDelay)
            guard !Task.isCancelled else { return }
            self?.beginHold()
        }
        publishPhase()
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let first = touches.first(where: { ObjectIdentifier($0) == touch }) else { return }
        let point = first.location(in: self)
        switch phase {
        case .idle:
            return
        case .pending(let start), .holding(let start):
            if hypot(point.x - start.x, point.y - start.y) > DPadLayout.stickTapSlop {
                holdTask?.cancel()
                phase = .steering(anchor: start)
                steer(point, from: start)
            } else {
                knob = CGSize(width: point.x - start.x, height: point.y - start.y)
            }
        case .steering(let anchor):
            steer(point, from: anchor)
        }
        publishPhase()
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard touches.contains(where: { ObjectIdentifier($0) == touch }) else { return }
        if case .pending = phase {
            onTap?()
        }
        finish()
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard touches.contains(where: { ObjectIdentifier($0) == touch }) else { return }
        finish()
    }

    override func reset() {
        finish()
    }

    private func beginHold() {
        guard case .pending(let start) = phase else { return }
        phase = .holding(start: start)
        publishPhase()
    }

    private func finish() {
        holdTask?.cancel()
        holdTask = nil
        touch = nil
        phase = .idle
        sector = nil
        knob = .zero
        publishPhase()
    }

    private func steer(_ point: CGPoint, from anchor: CGPoint) {
        let dx = point.x - anchor.x
        let dy = point.y - anchor.y
        let length = hypot(dx, dy)
        let scale = length > travel ? travel / length : 1
        knob = CGSize(width: dx * scale, height: dy * scale)

        let threshold = travel * (sector == nil ? DPadLayout.stickEngage : DPadLayout.stickRelease)
        guard length >= threshold else {
            sector = nil
            return
        }

        let degrees = atan2(dy, dx) * 180 / .pi
        let nearest = (Int((degrees / 45).rounded()) % 8 + 8) % 8
        if let current = sector {
            var delta = abs(degrees - Double(current) * 45).truncatingRemainder(dividingBy: 360)
            delta = min(delta, 360 - delta)
            if delta > 22.5 + DPadLayout.stickSectorHysteresis {
                sector = nearest
            }
        } else {
            sector = nearest
        }
    }

    private func publishPhase() {
        var state = DPadState(sector: sector, arcDegrees: arcDegrees, arcSession: arcSession,
                              knobOffset: knob)
        state.directions = PadDirection.directions(for: sector)
        switch phase {
        case .pending:
            state.knobTouched = true
        case .holding:
            state.knobTouched = true
            state.okPressed = true
        case .idle, .steering:
            break
        }
        publish(state)
    }
}

// MARK: - Numeric pads

// Phone-style 3x4 numeric pad. Glass caps need a wider gap than the flat
// pre-26 caps to read as separate keys.
struct CapsNumericPad: View {
    var size = CGSize(width: 150, height: 208)

    private var spacing: CGFloat {
        if #available(iOS 26.0, *) { 4 } else { 2 }
    }

    var body: some View {
        if #available(iOS 26.0, *) {
            GlassEffectContainer(spacing: 0) {
                grid
            }
        } else {
            grid
        }
    }

    private var grid: some View {
        let keyHeight = (size.height - spacing * 3) / 4
        let columns = Array(repeating: GridItem(.flexible(), spacing: spacing), count: 3)
        return LazyVGrid(columns: columns, spacing: spacing) {
            ForEach(keypadDigits, id: \.label) { digit in
                HoldableRawKey(scan: digit.scan) { pressed in
                    VStack(spacing: 1) {
                        Text(digit.label)
                            .font(.system(size: size.height / 10.4,
                                          weight: .semibold,
                                          design: .rounded))
                        if !digit.sub.isEmpty {
                            Text(digit.sub)
                                .font(.system(size: max(7, size.height / 29.7),
                                              weight: .semibold))
                                .tracking(0.5)
                                .foregroundStyle(.white.opacity(0.55))
                        }
                    }
                    .frame(maxWidth: .infinity)
                    .frame(height: keyHeight)
                    .keyCap(pressed: pressed)
                }
            }
        }
        .frame(width: size.width, height: size.height)
    }
}

// iOS system-keyboard-style numeric pad: three flexible columns of rounded-rect
// keys that each fill their whole grid cell, so the tap target is large and the
// keys spread across the full available width.
struct FilledNumericPad: View {
    var keyHeight: CGFloat = 56
    var rowSpacing: CGFloat = 8

    private let columns = Array(repeating: GridItem(.flexible(), spacing: 8), count: 3)

    var body: some View {
        LazyVGrid(columns: columns, spacing: rowSpacing) {
            ForEach(keypadDigits, id: \.label) { digit in
                // Default (rectangular) hit shape so the whole cell is tappable.
                HoldableRawKey(scan: digit.scan) { pressed in
                    VStack(spacing: 0) {
                        Text(digit.label)
                            .font(.system(size: keyHeight * 0.46, weight: .regular, design: .rounded))
                        if !digit.sub.isEmpty {
                            Text(digit.sub)
                                .font(.system(size: max(8, keyHeight * 0.16), weight: .semibold))
                                .tracking(1)
                                .foregroundStyle(.white.opacity(0.55))
                        }
                    }
                    .foregroundStyle(.white)
                    .frame(maxWidth: .infinity)
                    .frame(height: keyHeight)
                    .background(
                        RoundedRectangle(cornerRadius: 10, style: .continuous)
                            .fill(.white.opacity(pressed ? 0.30 : 0.14))
                    )
                    .overlay(
                        RoundedRectangle(cornerRadius: 10, style: .continuous)
                            .strokeBorder(.white.opacity(0.12), lineWidth: 1)
                    )
                    .animation(.easeOut(duration: 0.1), value: pressed)
                }
            }
        }
    }
}

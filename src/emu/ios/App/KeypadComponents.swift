import SwiftUI
import UIKit

// Shared building blocks for the on-screen keypad layouts in VirtualKeypad.swift:
// scan codes, the press/release key primitive, key-cap styling, the sliding
// d-pad and the numeric pads.

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
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { _ in
                        press()
                    }
                    .onEnded { _ in
                        release()
                    }
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

// MARK: - Key-cap styling

enum KeyKind {
    case digit, soft
}

private struct KeyCapModifier: ViewModifier {
    let kind: KeyKind
    let pressed: Bool

    func body(content: Content) -> some View {
        content
            .foregroundStyle(.white)
            .background(
                RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .fill(.white.opacity(background))
            )
            .overlay(
                RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .strokeBorder(.white.opacity(0.12), lineWidth: 1)
            )
            .scaleEffect(pressed ? 0.93 : 1)
            .animation(.easeOut(duration: 0.12), value: pressed)
    }

    private var background: Double {
        switch kind {
        case .digit:
            return pressed ? 0.24 : 0.10
        case .soft:
            return pressed ? 0.26 : 0.13
        }
    }
}

private struct KeypadSurface: ViewModifier {
    func body(content: Content) -> some View {
        content
            .padding(14)
            .background(
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .fill(.ultraThinMaterial)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .strokeBorder(.white.opacity(0.12), lineWidth: 1)
            )
    }
}

extension View {
    func keyCap(kind: KeyKind, pressed: Bool) -> some View {
        modifier(KeyCapModifier(kind: kind, pressed: pressed))
    }

    func keypadSurface() -> some View {
        modifier(KeypadSurface())
    }
}

// Fixed-size cap key sending a raw scan code — soft keys, clear key, etc.
struct CapKey: View {
    let scan: UInt32
    var title: String?
    var symbol: String?
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
                }
            }
            .frame(width: size.width, height: size.height)
            .keyCap(kind: .soft, pressed: pressed)
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

// The clear ("C") key — same guest key as hardware backspace.
struct ClearKey: View {
    var size: CGSize = CGSize(width: 58, height: 38)

    var body: some View {
        CapKey(scan: Scan.clear, symbol: "delete.left.fill", size: size)
            .accessibilityLabel(Text("keypad.accessibility.clear"))
    }
}

// MARK: - Sliding d-pad

// Circular four-way pad with a centred OK (select), used by every layout.
// The four direction zones are the ring quadrants split on the diagonals.
// Each touch on the ring holds its own direction, so multiple direction keys
// can remain pressed at once. Sliding a finger into another quadrant changes
// only that touch's key. The centre OK is its own key — slides across it keep
// the current direction (games treat OK as fire, so a transient press while
// crossing the middle would misfire).
struct SlidingDPad: View {
    var diameter: CGFloat = 130

    private let innerRatio: CGFloat = 0.34

    @State private var activeScans: Set<UInt32> = []
    @State private var impacts = 0

    private struct Direction {
        let scan: UInt32
        let symbol: String
        let sectorStart: Double // degrees, SwiftUI convention (0° = +x, cw)
        let labelOffset: CGVector
    }

    private var directions: [Direction] {
        let r = diameter * 0.36
        return [
            Direction(scan: Scan.up, symbol: "chevron.up", sectorStart: 225,
                      labelOffset: CGVector(dx: 0, dy: -r)),
            Direction(scan: Scan.right, symbol: "chevron.right", sectorStart: 315,
                      labelOffset: CGVector(dx: r, dy: 0)),
            Direction(scan: Scan.down, symbol: "chevron.down", sectorStart: 45,
                      labelOffset: CGVector(dx: 0, dy: r)),
            Direction(scan: Scan.left, symbol: "chevron.left", sectorStart: 135,
                      labelOffset: CGVector(dx: -r, dy: 0)),
        ]
    }

    var body: some View {
        let okSize = diameter * innerRatio
        ZStack {
            Circle()
                .fill(
                    LinearGradient(
                        colors: [.white.opacity(0.16), .white.opacity(0.05)],
                        startPoint: .top, endPoint: .bottom
                    )
                )
                .overlay(Circle().strokeBorder(.white.opacity(0.15), lineWidth: 1))
                .overlay(PadDividers(innerRatio: innerRatio).stroke(.white.opacity(0.12), lineWidth: 1))

            ForEach(directions, id: \.scan) { dir in
                let pressed = activeScans.contains(dir.scan)
                Sector(startAngle: .degrees(dir.sectorStart),
                       endAngle: .degrees(dir.sectorStart + 90),
                       innerRatio: innerRatio)
                    .fill(.white.opacity(pressed ? 0.26 : 0.0001))
                Image(systemName: dir.symbol)
                    .font(.system(size: diameter * 0.1, weight: .semibold))
                    .foregroundStyle(.white.opacity(0.9))
                    .scaleEffect(pressed ? 0.86 : 1)
                    .offset(x: dir.labelOffset.dx, y: dir.labelOffset.dy)
                    .animation(.easeOut(duration: 0.1), value: pressed)
            }

            // SwiftUI's DragGesture tracks only one touch. Use a UIKit surface
            // so every held finger can contribute a direction simultaneously.
            DPadTouchSurface(innerRatio: innerRatio) { point in
                directionScan(at: point)
            } onActiveDirectionsChanged: { scans in
                updateActive(scans)
            }

            HoldableRawKey(scan: Scan.select, hitShape: AnyShape(Circle())) { pressed in
                Text(verbatim: "OK")
                    .font(.system(size: okSize * 0.28, weight: .bold, design: .rounded))
                    .foregroundStyle(.white)
                    .frame(width: okSize, height: okSize)
                    .background(
                        Circle().fill(
                            LinearGradient(
                                colors: [.white.opacity(0.28), .white.opacity(0.12)],
                                startPoint: .top, endPoint: .bottom
                            )
                        )
                    )
                    .overlay(Circle().strokeBorder(.white.opacity(0.25), lineWidth: 1))
                    .scaleEffect(pressed ? 0.88 : 1)
                    .opacity(pressed ? 0.7 : 1)
                    .animation(.easeOut(duration: 0.12), value: pressed)
            }
        }
        .frame(width: diameter, height: diameter)
        .onDisappear {
            updateActive([])
        }
        .hapticImpact(.light, trigger: impacts)
    }

    // Direction under the finger, or nil to keep that touch's current one
    // (finger over the OK circle mid-slide, or outside the ring — holding past
    // the rim is common in action games and should not drop the direction).
    private func directionScan(at point: CGPoint) -> UInt32? {
        let radius = diameter / 2
        let dx = point.x - radius
        let dy = point.y - radius
        let dist = (dx * dx + dy * dy).squareRoot()
        if dist <= radius * innerRatio || dist > radius {
            return nil
        }
        var degrees = atan2(dy, dx) * 180 / .pi // -180..180, 0° = +x, cw
        if degrees < 0 {
            degrees += 360
        }
        switch degrees {
        case 45..<135: return Scan.down
        case 135..<225: return Scan.left
        case 225..<315: return Scan.up
        default: return Scan.right
        }
    }

    private func updateActive(_ scans: Set<UInt32>) {
        guard scans != activeScans else { return }

        for scan in activeScans.subtracting(scans).sorted() {
            EKA2L1Bridge.shared.submitRawKey(scan, pressed: false)
        }
        let pressedScans = scans.subtracting(activeScans)
        for scan in pressedScans.sorted() {
            EKA2L1Bridge.shared.submitRawKey(scan, pressed: true)
        }
        impacts += pressedScans.count
        activeScans = scans
    }
}

private struct DPadTouchSurface: UIViewRepresentable {
    let innerRatio: CGFloat
    let directionAtPoint: (CGPoint) -> UInt32?
    let onActiveDirectionsChanged: (Set<UInt32>) -> Void

    func makeUIView(context: Context) -> DPadTouchView {
        let view = DPadTouchView()
        configure(view)
        return view
    }

    func updateUIView(_ view: DPadTouchView, context: Context) {
        configure(view)
    }

    static func dismantleUIView(_ view: DPadTouchView, coordinator: ()) {
        view.reset()
    }

    private func configure(_ view: DPadTouchView) {
        view.innerRatio = innerRatio
        view.directionAtPoint = directionAtPoint
        view.onActiveDirectionsChanged = onActiveDirectionsChanged
    }
}

private final class DPadTouchView: UIView {
    var innerRatio: CGFloat = 0.34
    var directionAtPoint: ((CGPoint) -> UInt32?)?
    var onActiveDirectionsChanged: ((Set<UInt32>) -> Void)?

    private var heldDirections: [ObjectIdentifier: UInt32] = [:]
    private var publishedScans: Set<UInt32> = []

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

    // Own only the annular direction region. This lets the SwiftUI OK button
    // above the surface receive touches that begin in the centre.
    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        let radius = min(bounds.width, bounds.height) / 2
        let centre = CGPoint(x: bounds.midX, y: bounds.midY)
        let distance = hypot(point.x - centre.x, point.y - centre.y)
        return distance > radius * innerRatio && distance <= radius
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        for touch in touches {
            guard let scan = directionAtPoint?(touch.location(in: self)) else { continue }
            heldDirections[ObjectIdentifier(touch)] = scan
        }
        publishActiveDirections()
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        for touch in touches {
            let identifier = ObjectIdentifier(touch)
            guard heldDirections[identifier] != nil,
                  let scan = directionAtPoint?(touch.location(in: self)) else { continue }
            heldDirections[identifier] = scan
        }
        publishActiveDirections()
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        remove(touches)
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        remove(touches)
    }

    func reset() {
        heldDirections.removeAll()
        publish([])
    }

    private func remove(_ touches: Set<UITouch>) {
        for touch in touches {
            heldDirections.removeValue(forKey: ObjectIdentifier(touch))
        }
        publishActiveDirections()
    }

    private func publishActiveDirections() {
        publish(Set(heldDirections.values))
    }

    private func publish(_ scans: Set<UInt32>) {
        guard scans != publishedScans else { return }
        publishedScans = scans
        onActiveDirectionsChanged?(scans)
    }
}

// Annular wedge between innerRatio*R and R, spanning [startAngle, endAngle].
private struct Sector: Shape {
    let startAngle: Angle
    let endAngle: Angle
    var innerRatio: CGFloat = 0.34

    func path(in rect: CGRect) -> Path {
        let center = CGPoint(x: rect.midX, y: rect.midY)
        let outer = min(rect.width, rect.height) / 2
        let inner = outer * innerRatio
        var path = Path()
        path.addArc(center: center, radius: outer, startAngle: startAngle,
                    endAngle: endAngle, clockwise: false)
        path.addArc(center: center, radius: inner, startAngle: endAngle,
                    endAngle: startAngle, clockwise: true)
        path.closeSubpath()
        return path
    }
}

// The four diagonal separators (at 45/135/225/315°) from the OK circle out to
// the rim, so the equal quadrant split is visible.
private struct PadDividers: Shape {
    var innerRatio: CGFloat = 0.34

    func path(in rect: CGRect) -> Path {
        let center = CGPoint(x: rect.midX, y: rect.midY)
        let outer = min(rect.width, rect.height) / 2
        let inner = outer * innerRatio
        var path = Path()
        for degrees in stride(from: 45.0, to: 360.0, by: 90.0) {
            let radians = degrees * .pi / 180.0
            let dir = CGPoint(x: cos(radians), y: sin(radians))
            path.move(to: CGPoint(x: center.x + dir.x * inner, y: center.y + dir.y * inner))
            path.addLine(to: CGPoint(x: center.x + dir.x * outer, y: center.y + dir.y * outer))
        }
        return path
    }
}

// MARK: - Numeric pads

// Phone-style 3x4 numeric pad with a restrained 2pt separation between caps.
struct CapsNumericPad: View {
    var size = CGSize(width: 150, height: 208)

    private let spacing: CGFloat = 2
    private let columns = Array(repeating: GridItem(.flexible(), spacing: 2), count: 3)

    var body: some View {
        let keyHeight = (size.height - spacing * 3) / 4
        LazyVGrid(columns: columns, spacing: spacing) {
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
                    .keyCap(kind: .digit, pressed: pressed)
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

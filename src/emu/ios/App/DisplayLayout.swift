import SwiftUI

// One layout for portrait and one for landscape, shared by every game.
enum DisplayGravity: String, Codable, CaseIterable, Identifiable {
    case left
    case top
    case center
    case right
    case bottom

    var id: String { rawValue }

    // The order IosEmulator.h documents, which is also the Android frontend's.
    var rawIndex: Int {
        switch self {
        case .left: return 0
        case .top: return 1
        case .center: return 2
        case .right: return 3
        case .bottom: return 4
        }
    }

    var symbol: String {
        switch self {
        case .left: return "arrow.left.to.line"
        case .top: return "arrow.up.to.line"
        case .center: return "dot.squareshape"
        case .right: return "arrow.right.to.line"
        case .bottom: return "arrow.down.to.line"
        }
    }

    var title: LocalizedStringKey {
        switch self {
        case .left: return "display.gravity.left"
        case .top: return "display.gravity.top"
        case .center: return "display.gravity.center"
        case .right: return "display.gravity.right"
        case .bottom: return "display.gravity.bottom"
        }
    }
}

struct DisplayLayoutConfiguration: Codable, Equatable {
    // Multiplies the size the picture is fitted at, so 1 fills the safe area.
    var scale: Double
    var gravity: DisplayGravity
    // Shift from the gravity anchor, as a fraction of the canvas, so a stored
    // layout survives a surface resize.
    var offsetX: Double
    var offsetY: Double

    static let scaleRange: ClosedRange<Double> = 0.1...1.0

    // What fullscreen presents: the whole surface, filled and centred. With no
    // keypad to work around there is nothing for a stored layout to solve.
    static let fullscreen = DisplayLayoutConfiguration(
        scale: 1, gravity: .center, offsetX: 0, offsetY: 0)

    // Portrait hangs the picture below the notch, where a bottom keypad then
    // has the rest of the screen; landscape has no such split, so it centres.
    static func standard(landscape: Bool) -> DisplayLayoutConfiguration {
        DisplayLayoutConfiguration(
            scale: 1,
            gravity: landscape ? .center : .top,
            offsetX: 0,
            offsetY: 0
        )
    }

    func encoded() -> String {
        guard let data = try? JSONEncoder().encode(self) else { return "" }
        return String(decoding: data, as: UTF8.self)
    }

    static func decoded(_ rawValue: String, landscape: Bool) -> DisplayLayoutConfiguration {
        guard let data = rawValue.data(using: .utf8),
              var configuration = try? JSONDecoder().decode(DisplayLayoutConfiguration.self, from: data)
        else {
            return standard(landscape: landscape)
        }
        configuration.scale = min(max(configuration.scale, scaleRange.lowerBound), scaleRange.upperBound)
        return configuration
    }
}

enum DisplayLayoutDefaults {
    static let portraitKey = "ios.displayLayout.portrait"
    static let landscapeKey = "ios.displayLayout.landscape"
}

// MARK: - Editor

// The emulator keeps drawing the picture; the editor only outlines where it lands.
struct DisplayLayoutEditor: View {
    let size: CGSize
    let safeAreaInsets: EdgeInsets
    @Binding var configuration: DisplayLayoutConfiguration
    // The presented picture in canvas coordinates, re-read while the editor is
    // up because the emulator, not this view, decides where it ends up.
    let pictureFrame: () -> CGRect
    let onReset: () -> Void
    let onDone: () -> Void

    @State private var dragStart: CGPoint?
    @State private var outline: CGRect = .zero

    private let refreshTimer = Timer.publish(every: 1.0 / 30.0, on: .main, in: .common).autoconnect()

    var body: some View {
        ZStack {
            Color.black.opacity(0.28)
                .contentShape(Rectangle())
                .gesture(dragGesture)

            if !outline.isEmpty {
                RoundedRectangle(cornerRadius: 6, style: .continuous)
                    .stroke(.white.opacity(0.9), style: StrokeStyle(lineWidth: 2, dash: [7, 5]))
                    .frame(width: outline.width, height: outline.height)
                    .position(x: outline.midX, y: outline.midY)
                    .allowsHitTesting(false)
            }

            VStack(spacing: 8) {
                editorHeader
                editorSettings
            }
            .frame(maxHeight: .infinity, alignment: .top)
            .padding(.leading, max(14, safeAreaInsets.leading + 10))
            .padding(.trailing, max(14, safeAreaInsets.trailing + 10))
            .padding(.top, max(12, safeAreaInsets.top + 8))
        }
        .frame(width: size.width, height: size.height)
        .ignoresSafeArea()
        .onReceive(refreshTimer) { _ in
            let frame = pictureFrame()
            if frame != outline {
                outline = frame
            }
        }
    }

    private var dragGesture: some Gesture {
        DragGesture()
            .onChanged { value in
                guard size.width > 0, size.height > 0 else { return }
                let start = dragStart
                    ?? CGPoint(x: configuration.offsetX, y: configuration.offsetY)
                dragStart = start
                configuration.offsetX = start.x + value.translation.width / size.width
                configuration.offsetY = start.y + value.translation.height / size.height
            }
            .onEnded { _ in
                dragStart = nil
            }
    }

    private var editorHeader: some View {
        HStack(spacing: 12) {
            editorButton(symbol: "arrow.counterclockwise", label: "display.editor.reset") {
                onReset()
            }

            Text("display.editor.hint")
                .font(.callout.weight(.semibold))
                .foregroundStyle(.white)
                .multilineTextAlignment(.center)
                .frame(maxWidth: .infinity)
                .padding(.horizontal, 14)
                .frame(height: 44)
                .background(.black.opacity(0.68), in: Capsule())
                .overlay(Capsule().strokeBorder(.white.opacity(0.16), lineWidth: 1))

            editorButton(symbol: "checkmark", label: "common.done") {
                onDone()
            }
        }
    }

    private var editorSettings: some View {
        ViewThatFits(in: .horizontal) {
            HStack(spacing: 8) {
                scaleBar
                    .frame(minWidth: 260, maxWidth: 300)
                gravityPicker
            }

            VStack(spacing: 8) {
                scaleBar
                gravityPicker
            }
        }
    }

    private var scaleBar: some View {
        HStack(spacing: 12) {
            Image(systemName: "arrow.up.left.and.arrow.down.right")
                .foregroundStyle(.white.opacity(0.8))
            Slider(value: $configuration.scale, in: DisplayLayoutConfiguration.scaleRange)
                .tint(.white)
            Text(configuration.scale.formatted(.percent.precision(.fractionLength(0))))
                .font(.caption.monospacedDigit().weight(.semibold))
                .foregroundStyle(.white)
                .frame(width: 42, alignment: .trailing)
        }
        .padding(.horizontal, 16)
        .frame(height: 50)
        .background(.black.opacity(0.72), in: Capsule())
        .overlay(Capsule().strokeBorder(.white.opacity(0.16), lineWidth: 1))
        .accessibilityLabel("display.editor.scale")
    }

    // A segmented picker in the editor's own capsule styling: the system
    // segmented style can't be tinted to read on the dimmed game picture.
    private var gravityPicker: some View {
        HStack(spacing: 4) {
            ForEach(DisplayGravity.allCases) { gravity in
                let selected = configuration.gravity == gravity
                Button {
                    // A drag is a nudge away from the current anchor, so it
                    // means nothing once the anchor moves.
                    configuration.gravity = gravity
                    configuration.offsetX = 0
                    configuration.offsetY = 0
                } label: {
                    Image(systemName: gravity.symbol)
                        .font(.system(size: 15, weight: .semibold))
                        .foregroundStyle(.white)
                        .frame(width: 42, height: 38)
                        .background(.white.opacity(selected ? 0.26 : 0), in: Capsule())
                }
                .buttonStyle(.plain)
                .accessibilityLabel(gravity.title)
                .accessibilityAddTraits(selected ? [.isSelected] : [])
            }
        }
        .padding(.horizontal, 6)
        .frame(height: 50)
        .fixedSize(horizontal: true, vertical: false)
        .background(.black.opacity(0.72), in: Capsule())
        .overlay(Capsule().strokeBorder(.white.opacity(0.16), lineWidth: 1))
        // A label on the row itself would replace each button's own, leaving
        // five identically named targets.
        .accessibilityElement(children: .contain)
    }

    private func editorButton(
        symbol: String,
        label: LocalizedStringKey,
        action: @escaping () -> Void
    ) -> some View {
        Button(action: action) {
            Image(systemName: symbol)
                .font(.system(size: 17, weight: .bold))
                .foregroundStyle(.white)
                .frame(width: 44, height: 44)
                .background(.black.opacity(0.72), in: Circle())
                .overlay(Circle().strokeBorder(.white.opacity(0.18), lineWidth: 1))
        }
        .buttonStyle(.plain)
        .accessibilityLabel(label)
    }
}

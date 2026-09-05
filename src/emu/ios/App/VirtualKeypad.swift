import SwiftUI
import UIKit

// The keypad now has one user-positionable layout instead of a set of fixed
// portrait/landscape presets. Positions are normalized so they survive changes
// in screen size; portrait and landscape use separate stored configurations.
enum KeypadElement: String, CaseIterable, Hashable {
    case dpad
    case leftSoft
    case rightSoft
    case numeric
    case menu
    case clear

    var title: String {
        switch self {
        case .dpad: return "D-pad"
        case .leftSoft: return "LSK"
        case .rightSoft: return "RSK"
        case .numeric: return String(localized: "keypad.editor.numberPad")
        case .menu: return String(localized: "emulator.menu")
        case .clear: return String(localized: "keypad.accessibility.clear")
        }
    }

    func size(in canvasSize: CGSize) -> CGSize {
        // Base both orientations on the display's physical short edge so a
        // rotation never changes the apparent control size. Cap growth on
        // large displays so the keypad does not become visually dominant.
        let shortEdge = min(canvasSize.width, canvasSize.height)
        let majorWidth = min(180, max(150, (shortEdge - 36) / 2))
        let scale = majorWidth / 150

        switch self {
        case .dpad:
            return CGSize(width: majorWidth - 4, height: majorWidth - 4)
        case .leftSoft, .rightSoft, .menu, .clear:
            return CGSize(width: 56 * scale, height: 36 * scale)
        case .numeric:
            return CGSize(width: majorWidth, height: majorWidth * 190 / 150)
        }
    }
}

struct NormalizedKeypadPoint: Codable, Equatable {
    var x: Double
    var y: Double

    func point(in size: CGSize) -> CGPoint {
        CGPoint(x: x * size.width, y: y * size.height)
    }

    static func make(_ point: CGPoint, in size: CGSize) -> NormalizedKeypadPoint {
        guard size.width > 0, size.height > 0 else {
            return NormalizedKeypadPoint(x: 0.5, y: 0.5)
        }
        return NormalizedKeypadPoint(
            x: point.x / size.width,
            y: point.y / size.height
        )
    }
}

struct KeypadLayoutConfiguration: Codable, Equatable {
    var dpad: NormalizedKeypadPoint
    var leftSoft: NormalizedKeypadPoint
    var rightSoft: NormalizedKeypadPoint
    var numeric: NormalizedKeypadPoint
    var menu: NormalizedKeypadPoint
    var clear: NormalizedKeypadPoint

    func point(for element: KeypadElement, in size: CGSize) -> CGPoint {
        normalizedPoint(for: element).point(in: size)
    }

    mutating func setPoint(_ point: CGPoint, for element: KeypadElement, in size: CGSize) {
        let normalized = NormalizedKeypadPoint.make(point, in: size)
        switch element {
        case .dpad: dpad = normalized
        case .leftSoft: leftSoft = normalized
        case .rightSoft: rightSoft = normalized
        case .numeric: numeric = normalized
        case .menu: menu = normalized
        case .clear: clear = normalized
        }
    }

    mutating func constrainElementsToWindow(
        in size: CGSize,
        safeAreaInsets: EdgeInsets = EdgeInsets()
    ) {
        guard size.width > 0, size.height > 0 else { return }

        let windowSize = CGSize(
            width: size.width + safeAreaInsets.leading + safeAreaInsets.trailing,
            height: size.height + safeAreaInsets.top + safeAreaInsets.bottom
        )

        for element in KeypadElement.allCases {
            let elementSize = element.size(in: windowSize)
            let point = point(for: element, in: size)
            let constrainedPoint = CGPoint(
                x: constrainedCoordinate(
                    point.x,
                    elementLength: elementSize.width,
                    windowLength: windowSize.width
                ),
                y: constrainedCoordinate(
                    point.y,
                    elementLength: elementSize.height,
                    windowLength: windowSize.height
                )
            )
            setPoint(constrainedPoint, for: element, in: size)
        }
    }

    func encoded() -> String {
        guard let data = try? JSONEncoder().encode(self) else { return "" }
        return String(decoding: data, as: UTF8.self)
    }

    static func decoded(
        _ rawValue: String,
        defaultFor size: CGSize,
        safeAreaInsets: EdgeInsets = EdgeInsets()
    ) -> KeypadLayoutConfiguration {
        if let data = rawValue.data(using: .utf8),
           let configuration = try? JSONDecoder().decode(KeypadLayoutConfiguration.self, from: data) {
            return configuration
        }
        return classicDefault(in: size, safeAreaInsets: safeAreaInsets)
    }

    // The reset layout preserves the familiar bottom-centred arrangement.
    // Portrait aligns the tops of the d-pad and number pad, with L/R above and
    // Menu/Clear filling the shorter d-pad column down to the number-pad bottom.
    static func classicDefault(
        in size: CGSize,
        safeAreaInsets: EdgeInsets = EdgeInsets()
    ) -> KeypadLayoutConfiguration {
        let controlSize = CGSize(
            width: size.width + safeAreaInsets.leading + safeAreaInsets.trailing,
            height: size.height + safeAreaInsets.top + safeAreaInsets.bottom
        )
        let dpadSize = KeypadElement.dpad.size(in: controlSize)
        let numericSize = KeypadElement.numeric.size(in: controlSize)
        let softKeySize = KeypadElement.leftSoft.size(in: controlSize)
        let menuSize = KeypadElement.menu.size(in: controlSize)
        let clearSize = KeypadElement.clear.size(in: controlSize)

        if size.width > size.height {
            // N-Gage-style landscape default: navigation in the left hand,
            // number pad in the right, with the soft keys above each cluster.
            let fullWidth = size.width + safeAreaInsets.leading + safeAreaInsets.trailing
            let safeRightEdge = fullWidth - safeAreaInsets.trailing
            let centerY = safeAreaInsets.top + size.height / 2
            let dpadCenter = CGPoint(
                x: safeAreaInsets.leading + 12 + dpadSize.width / 2,
                y: centerY
            )
            let numericCenter = CGPoint(
                x: safeRightEdge - 12 - numericSize.width / 2,
                y: centerY
            )
            let leftSoft = CGPoint(
                x: dpadCenter.x,
                y: dpadCenter.y - dpadSize.height / 2 - 12 - softKeySize.height / 2
            )
            let rightSoft = CGPoint(
                x: numericCenter.x,
                y: numericCenter.y - numericSize.height / 2 - 12 - softKeySize.height / 2
            )
            let menu = CGPoint(
                x: dpadCenter.x,
                y: dpadCenter.y + dpadSize.height / 2 + 12 + menuSize.height / 2
            )
            let clear = CGPoint(
                x: numericCenter.x,
                y: numericCenter.y + numericSize.height / 2 + 12 + clearSize.height / 2
            )

            return KeypadLayoutConfiguration(
                dpad: .make(dpadCenter, in: size),
                leftSoft: .make(leftSoft, in: size),
                rightSoft: .make(rightSoft, in: size),
                numeric: .make(numericCenter, in: size),
                menu: .make(menu, in: size),
                clear: .make(clear, in: size)
            )
        }

        // The d-pad is 4pt narrower than its original slot; put that space
        // between the two major controls so the outer 12pt margins stay fixed.
        let gap: CGFloat = 16
        let totalWidth = dpadSize.width + gap + numericSize.width
        let leading = max(12, (size.width - totalWidth) / 2)
        let dpadCenterX = leading + dpadSize.width / 2
        let numericCenterX = dpadCenterX + dpadSize.width / 2
            + gap + numericSize.width / 2
        // GeometryReader reports the safe-area-reduced height here, while the
        // overlay ignores safe areas. Adding the top inset maps the top of the
        // bottom safe area into overlay coordinates.
        let bottomEdge = size.height + safeAreaInsets.top
        let topEdge = bottomEdge - numericSize.height
        let numericCenterY = topEdge + numericSize.height / 2
        let dpadCenterY = topEdge + dpadSize.height / 2
        let softCenterY = topEdge - 12 - softKeySize.height / 2
        let leftSoft = CGPoint(
            x: dpadCenterX - dpadSize.width / 2 + softKeySize.width / 2,
            y: softCenterY
        )
        let rightSoft = CGPoint(
            x: numericCenterX + numericSize.width / 2 - softKeySize.width / 2,
            y: softCenterY
        )
        let menu = CGPoint(
            x: dpadCenterX - dpadSize.width / 2 + menuSize.width / 2,
            y: bottomEdge - menuSize.height / 2
        )
        let clear = CGPoint(
            x: dpadCenterX + dpadSize.width / 2 - clearSize.width / 2,
            y: bottomEdge - clearSize.height / 2
        )

        return KeypadLayoutConfiguration(
            dpad: .make(CGPoint(x: dpadCenterX, y: dpadCenterY), in: size),
            leftSoft: .make(leftSoft, in: size),
            rightSoft: .make(rightSoft, in: size),
            numeric: .make(CGPoint(x: numericCenterX, y: numericCenterY), in: size),
            menu: .make(menu, in: size),
            clear: .make(clear, in: size)
        )
    }

    private func normalizedPoint(for element: KeypadElement) -> NormalizedKeypadPoint {
        switch element {
        case .dpad: return dpad
        case .leftSoft: return leftSoft
        case .rightSoft: return rightSoft
        case .numeric: return numeric
        case .menu: return menu
        case .clear: return clear
        }
    }

    private func constrainedCoordinate(
        _ coordinate: CGFloat,
        elementLength: CGFloat,
        windowLength: CGFloat
    ) -> CGFloat {
        let minimum = elementLength / 2
        let maximum = windowLength - elementLength / 2
        guard minimum <= maximum else { return windowLength / 2 }
        return min(max(coordinate, minimum), maximum)
    }
}

private struct KeypadElementBackdrop: View {
    let element: KeypadElement

    @ViewBuilder var body: some View {
        switch element {
        case .dpad:
            Circle().fill(.black)
        case .leftSoft, .rightSoft, .numeric, .menu, .clear:
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .fill(.black)
        }
    }
}

enum KeypadDefaults {
    static let opacityKey = "ios.keypadOpacity"
    static let opacity = 0.85
    static let opacityRange: ClosedRange<Double> = 0.1...1.0
    static let portraitLayoutKey = "ios.keypadPositions.portrait"
    static let landscapeLayoutKey = "ios.keypadPositions.landscape"
    static let fullscreenKey = "ios.fullscreenDisplay"
    static let touchFullscreenKey = "ios.fullscreenDisplay.touch"
    static let orientationLockKey = "ios.lockGameOrientation"

    // Keep the historical launch argument useful for automated touch tests.
    // Every former non-fullscreen preset now selects the customizable keypad.
    static func launchArgumentFullscreen() -> Bool? {
        guard let value = UserDefaults.standard.string(forKey: "LaunchKeypadLayout") else {
            return nil
        }
        return value == "fullscreen"
    }
}

// What the system-menu key needs from the hosting emulator screen.
struct KeypadMenuActions {
    var fullScreen: Binding<Bool>
    var locksOrientation: Binding<Bool>
    var editKeypadLayout: () -> Void
    var guestScreenModes: [Int]
    var guestScreenMode: Binding<Int>
    var frameLimit: Binding<Int>
    var saveScreenshot: () -> Void
    var exitGame: () -> Void
}

// MARK: - System menu

struct SystemMenuKey: View {
    let actions: KeypadMenuActions
    var size: CGSize = CGSize(width: 58, height: 38)

    var body: some View {
        Menu {
            menuContent
        } label: {
            Image(systemName: "line.3.horizontal")
                .font(.system(size: 16, weight: .semibold))
                .frame(width: size.width, height: size.height)
                .keyCap(kind: .soft, pressed: false)
        }
        .accessibilityLabel("emulator.menu")
    }

    @ViewBuilder
    private var menuContent: some View {
        Button {
            actions.editKeypadLayout()
        } label: {
            Label("keypad.editor.editLayout", systemImage: "move.3d")
        }

        Toggle(isOn: actions.locksOrientation) {
            Label("emulator.menu.lockOrientation", systemImage: "lock.rotation")
        }

        Menu {
            fpsLimitPicker

            if !actions.guestScreenModes.isEmpty {
                Divider()
                guestScreenModePicker
            }
        } label: {
            Label("emulator.menu.gameSettings", systemImage: "slider.horizontal.3")
        }

        Button {
            actions.saveScreenshot()
        } label: {
            Label("emulator.saveScreenshot", systemImage: "camera")
        }

        Button(role: .destructive) {
            actions.exitGame()
        } label: {
            Label("emulator.exit", systemImage: "xmark.circle")
        }
    }

    @ViewBuilder
    private var fpsLimitPicker: some View {
        let picker = Picker("emulator.menu.fpsLimit", selection: actions.frameLimit) {
            Text(verbatim: "15").tag(15)
            Text(verbatim: "25").tag(25)
            Text(verbatim: "30").tag(30)
            Text(verbatim: "60").tag(60)
            Text("emulator.fpsLimit.unlimited").tag(0)
        }
        if #available(iOS 17, *) {
            picker.pickerStyle(.palette)
        } else {
            picker
        }
    }

    @ViewBuilder
    private var guestScreenModePicker: some View {
        let picker = Picker("emulator.menu.guestScreenMode", selection: actions.guestScreenMode) {
            ForEach(actions.guestScreenModes, id: \.self) { mode in
                Text(verbatim: "\(mode)").tag(mode)
            }
        }
        if #available(iOS 17, *) {
            picker.pickerStyle(.palette)
        } else {
            picker
        }
    }
}

// MARK: - Runtime keypad

private struct KeypadElementFramesKey: PreferenceKey {
    static let defaultValue: [KeypadElement: CGRect] = [:]

    static func reduce(value: inout [KeypadElement: CGRect],
                       nextValue: () -> [KeypadElement: CGRect]) {
        value.merge(nextValue(), uniquingKeysWith: { _, new in new })
    }
}

struct VirtualKeypad: View {
    let size: CGSize
    let controlSize: CGSize
    let configuration: KeypadLayoutConfiguration
    let fullScreen: Bool
    let actions: KeypadMenuActions
    let onFramesChange: ([CGRect]) -> Void

    var body: some View {
        ZStack {
            if !fullScreen {
                runtimeElement(.dpad) {
                    SlidingDPad(diameter: KeypadElement.dpad.size(in: controlSize).width)
                }
                runtimeElement(.leftSoft) {
                    SoftKey(side: .left, size: KeypadElement.leftSoft.size(in: controlSize))
                }
                runtimeElement(.rightSoft) {
                    SoftKey(side: .right, size: KeypadElement.rightSoft.size(in: controlSize))
                }
                runtimeElement(.numeric) {
                    CapsNumericPad(size: KeypadElement.numeric.size(in: controlSize))
                }
                runtimeElement(.clear) {
                    ClearKey(size: KeypadElement.clear.size(in: controlSize))
                }
            }

            runtimeElement(.menu) {
                SystemMenuKey(actions: actions, size: KeypadElement.menu.size(in: controlSize))
            }
        }
        .frame(width: size.width, height: size.height)
        .ignoresSafeArea()
        .onPreferenceChange(KeypadElementFramesKey.self) { frames in
            onFramesChange(Array(frames.values))
        }
    }

    private func position(for element: KeypadElement) -> CGPoint {
        return configuration.point(for: element, in: size)
    }

    private func runtimeElement<Content: View>(
        _ element: KeypadElement,
        @ViewBuilder content: () -> Content
    ) -> some View {
        let elementSize = element.size(in: controlSize)
        return content()
            .frame(width: elementSize.width, height: elementSize.height)
            .background(KeypadElementBackdrop(element: element))
            .background(
                GeometryReader { proxy in
                    Color.clear.preference(
                        key: KeypadElementFramesKey.self,
                        value: [element: proxy.frame(in: .global)]
                    )
                }
            )
            .position(position(for: element))
    }
}

// MARK: - Layout editor

struct KeypadLayoutEditor: View {
    let size: CGSize
    let controlSize: CGSize
    let safeAreaInsets: EdgeInsets
    @Binding var configuration: KeypadLayoutConfiguration
    @Binding var opacity: Double
    @Binding var fullScreen: Bool
    let actions: KeypadMenuActions
    let onReset: () -> Void
    let onDone: () -> Void

    @State private var dragStarts: [KeypadElement: CGPoint] = [:]

    var body: some View {
        ZStack {
            Color.black.opacity(0.28)
                .contentShape(Rectangle())

            ForEach(editorElements, id: \.self) { element in
                draggableElement(element)
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
    }

    private var editorElements: [KeypadElement] {
        fullScreen ? [.menu] : KeypadElement.allCases
    }

    private var editorSettings: some View {
        ViewThatFits(in: .horizontal) {
            HStack(spacing: 8) {
                opacityBar
                    .frame(minWidth: 260, maxWidth: 300)
                fullScreenToggle
            }

            VStack(spacing: 8) {
                opacityBar
                fullScreenToggle
            }
        }
    }

    private var editorHeader: some View {
        HStack(spacing: 12) {
            editorButton(symbol: "arrow.counterclockwise", label: "keypad.editor.reset") {
                onReset()
            }

            Text("keypad.editor.hint")
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

    private var opacityBar: some View {
        HStack(spacing: 12) {
            Image(systemName: "circle.lefthalf.filled")
                .foregroundStyle(.white.opacity(0.8))
            Slider(value: $opacity, in: KeypadDefaults.opacityRange)
                .tint(.white)
            Text(opacity.formatted(.percent.precision(.fractionLength(0))))
                .font(.caption.monospacedDigit().weight(.semibold))
                .foregroundStyle(.white)
                .frame(width: 42, alignment: .trailing)
        }
        .padding(.horizontal, 16)
        .frame(height: 50)
        .background(.black.opacity(0.72), in: Capsule())
        .overlay(Capsule().strokeBorder(.white.opacity(0.16), lineWidth: 1))
        .accessibilityLabel("settings.keypadOpacity")
    }

    private var fullScreenToggle: some View {
        Toggle("emulator.menu.fullScreen", isOn: $fullScreen)
            .font(.callout.weight(.semibold))
            .foregroundStyle(.white)
            .tint(.white)
            .padding(.horizontal, 16)
            .frame(height: 50)
            .fixedSize(horizontal: true, vertical: false)
            .background(.black.opacity(0.72), in: Capsule())
            .overlay(Capsule().strokeBorder(.white.opacity(0.16), lineWidth: 1))
    }

    private func draggableElement(_ element: KeypadElement) -> some View {
        let elementSize = element.size(in: controlSize)
        return ZStack {
            KeypadElementBackdrop(element: element)

            elementContent(element)
                .allowsHitTesting(false)
                .accessibilityHidden(true)

            RoundedRectangle(cornerRadius: element == .dpad ? 28 : 14, style: .continuous)
                .stroke(
                    .white.opacity(0.9),
                    style: StrokeStyle(lineWidth: 2, dash: [7, 5])
                )
        }
        .frame(width: elementSize.width, height: elementSize.height)
        .opacity(opacity)
        .contentShape(Rectangle())
        .position(configuration.point(for: element, in: size))
        .gesture(
            DragGesture()
                .onChanged { value in
                    let start = dragStarts[element]
                        ?? configuration.point(for: element, in: size)
                    dragStarts[element] = start
                    let proposed = CGPoint(
                        x: start.x + value.translation.width,
                        y: start.y + value.translation.height
                    )
                    configuration.setPoint(
                        proposed,
                        for: element,
                        in: size
                    )
                }
                .onEnded { _ in
                    dragStarts[element] = nil
                }
        )
        .accessibilityLabel(Text(verbatim: element.title))
        .accessibilityHint("keypad.editor.dragHint")
        .accessibilityElement(children: .ignore)
    }

    @ViewBuilder
    private func elementContent(_ element: KeypadElement) -> some View {
        let elementSize = element.size(in: controlSize)
        switch element {
        case .dpad:
            SlidingDPad(diameter: elementSize.width)
        case .leftSoft:
            SoftKey(side: .left, size: elementSize)
        case .rightSoft:
            SoftKey(side: .right, size: elementSize)
        case .numeric:
            CapsNumericPad(size: elementSize)
        case .menu:
            SystemMenuKey(actions: actions, size: elementSize)
        case .clear:
            ClearKey(size: elementSize)
        }
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

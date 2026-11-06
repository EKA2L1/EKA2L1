import SwiftUI
import UIKit

// Impact feedback for the on-screen controls.
//
// Allocating a UIImpactFeedbackGenerator per event and dropping it right after
// impactOccurred() makes UIKit spin its CoreHaptics engine up and back down
// around every single tap. The keypad and the sliding d-pad fire several taps a
// second, so that churn keeps CoreHaptics creating and tearing down pattern
// players, and each of those reports a metric through Apple's AudioAnalytics
// XPC client. When an engine error report races a player-init report on that
// client it over-releases a CF object mid-serialisation and traps
// (__CFTypeCollectionRelease) with no frame of ours on the stack.
//
// The feedback therefore has to come from a generator that outlives the tap.
// On iOS 17 that is SwiftUI's own .sensoryFeedback, which owns a generator for
// as long as the modifier is installed; on iOS 16 `Haptics` below keeps one per
// style by hand. Both are driven the same way, by a value that changes when the
// control wants a tap.

extension View {
    /// Plays an impact whenever `trigger` changes. Use a counter rather than
    /// the control's own state so feedback fires only on the edge you mean —
    /// key-down but not key-up, say.
    @ViewBuilder
    func hapticImpact<T: Equatable>(_ style: UIImpactFeedbackGenerator.FeedbackStyle,
                                    intensity: Double = 1,
                                    trigger: T) -> some View {
        if #available(iOS 17.0, *) {
            sensoryFeedback(style.sensoryFeedback(intensity: intensity), trigger: trigger)
        } else {
            onChange(of: trigger) { _ in Haptics.impact(style, intensity: intensity) }
        }
    }
}

@available(iOS 17.0, *)
private extension UIImpactFeedbackGenerator.FeedbackStyle {
    func sensoryFeedback(intensity: Double) -> SensoryFeedback {
        switch self {
        case .light: .impact(weight: .light, intensity: intensity)
        case .medium: .impact(weight: .medium, intensity: intensity)
        case .heavy: .impact(weight: .heavy, intensity: intensity)
        case .soft: .impact(flexibility: .soft, intensity: intensity)
        case .rigid: .impact(flexibility: .rigid, intensity: intensity)
        @unknown default: .impact(intensity: intensity)
        }
    }
}

/// iOS 16 fallback for `hapticImpact`. Unused on iOS 17+, where SwiftUI owns
/// the generator.
@MainActor
enum Haptics {
    private static var generators: [UIImpactFeedbackGenerator.FeedbackStyle: UIImpactFeedbackGenerator] = [:]

    static func impact(_ style: UIImpactFeedbackGenerator.FeedbackStyle, intensity: Double = 1) {
        // Playing feedback while the app is not frontmost fails inside
        // CoreHaptics, and that failure is what raises the racing error report.
        guard UIApplication.shared.applicationState == .active else { return }

        let generator: UIImpactFeedbackGenerator
        if let existing = generators[style] {
            generator = existing
        } else {
            generator = UIImpactFeedbackGenerator(style: style)
            generators[style] = generator
        }
        generator.impactOccurred(intensity: intensity)
        // Keeps the engine alive for the next tap instead of letting it idle
        // out and restart.
        generator.prepare()
    }

    /// Drops the cached generators so a backgrounded app is not holding the
    /// haptic engine; the next foreground tap rebuilds them.
    static func release() {
        generators.removeAll()
    }
}

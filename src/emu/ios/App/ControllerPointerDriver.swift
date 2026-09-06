import QuartzCore
import UIKit

@MainActor
final class ControllerPointerDriver: NSObject {
    var configuration = ControllerFeatures()
    private var state = ControllerPointerState()
    private var enabled = false
    private var pressedTokens: Set<String> = []
    private var displayLink: CADisplayLink?
    private var lastTimestamp: CFTimeInterval = 0
    private var displayRect = CGRect.zero
    // UITouch objects are aligned pointers; this identity cannot collide with one.
    private let pointerID: UInt = 1

    var consumedTokens: Set<String> {
        guard enabled else { return [] }
        return Set(configuration.pointerBindings.compactMap { token, action in
            action == .toggle || state.isVisible ? token : nil
        })
    }

    func setEnabled(_ enabled: Bool) {
        guard enabled != self.enabled else { return }
        self.enabled = enabled
        emit(state.reset())
        state.prime(actions: configuration.actions(for: pressedTokens))
        updateDisplayLink()
        updateCursor()
    }

    func handle(pressed: Set<String>) {
        pressedTokens = pressed
        guard enabled else { return }
        refreshGeometry()
        guard !displayRect.isEmpty else {
            state.prime(actions: configuration.actions(for: pressed))
            return
        }
        emit(state.update(actions: configuration.actions(for: pressed)))
        updateDisplayLink()
        updateCursor()
    }

    private func refreshGeometry() {
        let rect = EKA2L1Bridge.shared.guestDisplayRect
        if rect != displayRect {
            emit(state.cancelTouch())
            displayRect = rect
        }
    }

    private func updateDisplayLink() {
        if enabled && state.isVisible {
            guard displayLink == nil else { return }
            let link = CADisplayLink(target: self, selector: #selector(tick(_:)))
            link.preferredFrameRateRange = CAFrameRateRange(minimum: 30, maximum: 60, preferred: 60)
            link.add(to: .main, forMode: .common)
            displayLink = link
        } else {
            displayLink?.invalidate()
            displayLink = nil
            lastTimestamp = 0
        }
    }

    @objc private func tick(_ link: CADisplayLink) {
        refreshGeometry()
        let elapsed = lastTimestamp == 0 ? link.duration : link.timestamp - lastTimestamp
        lastTimestamp = link.timestamp
        emit(state.move(elapsed: elapsed, size: displayRect.size))
        updateCursor()
    }

    private func emit(_ events: [ControllerPointerState.TouchPhase]) {
        guard !displayRect.isEmpty else { return }
        let point = state.location(in: displayRect)
        for event in events {
            let phase: EKA2L1PointerPhase
            switch event {
            case .began: phase = .began
            case .moved: phase = .moved
            case .ended: phase = .ended
            case .cancelled: phase = .cancelled
            }
            EKA2L1Bridge.shared.submitPointer(x: point.x, y: point.y, phase: phase, pointerId: pointerID)
        }
    }

    private func updateCursor() {
        ExternalDisplay.shared.setPointer(
            position: enabled && state.isVisible && !displayRect.isEmpty ? state.position : nil,
            pictureSize: displayRect.size, touching: state.isTouching)
    }
}

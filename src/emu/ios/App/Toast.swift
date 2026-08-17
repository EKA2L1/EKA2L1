import SwiftUI

// Transient status messages (install/boot/uninstall results) shown as a toast
// that slides up from the bottom of the surface it is attached to and fades out
// on its own. It replaces the old bottom status-bar text, and is deliberately a
// plain overlay on the home content: a running app's screen lives in a pushed
// navigation destination, so nothing ever draws on top of the guest frame.

private let toastDefaultDuration: Double = 3.0

private struct ToastLabel: View {
    let message: String

    var body: some View {
        Text(message)
            .font(.subheadline)
            .foregroundStyle(.primary)
            .multilineTextAlignment(.center)
            .lineLimit(3)
            .padding(.horizontal, 18)
            .padding(.vertical, 12)
            .modifier(ToastBackground())
            // Purely informational: never swallow taps meant for the app grid.
            .allowsHitTesting(false)
            .accessibilityAddTraits(.isStaticText)
    }
}

// iOS 26 renders the toast on Liquid Glass; earlier releases fall back to the
// blurred material that matches the rest of the pre-26 chrome.
private struct ToastBackground: ViewModifier {
    private var shape: RoundedRectangle {
        RoundedRectangle(cornerRadius: 22, style: .continuous)
    }

    func body(content: Content) -> some View {
        if #available(iOS 26.0, *) {
            content.glassEffect(.regular, in: shape)
        } else {
            content
                .background(.ultraThinMaterial, in: shape)
                .overlay(shape.strokeBorder(Color.primary.opacity(0.08)))
                .shadow(color: .black.opacity(0.18), radius: 12, y: 4)
        }
    }
}

private struct ToastModifier: ViewModifier {
    @Binding var message: String?
    let duration: Double

    func body(content: Content) -> some View {
        content
            .overlay(alignment: .bottom) {
                if let message {
                    ToastLabel(message: message)
                        .padding(.horizontal, 24)
                        .padding(.bottom, 28)
                        .transition(.move(edge: .bottom).combined(with: .opacity))
                        // Keyed on the text, so a message arriving while another
                        // one is up restarts the dismissal countdown instead of
                        // inheriting the older toast's remaining time.
                        .task(id: message) {
                            try? await Task.sleep(nanoseconds: UInt64(duration * 1_000_000_000))
                            guard !Task.isCancelled else { return }
                            self.message = nil
                        }
                }
            }
            .animation(.spring(response: 0.35, dampingFraction: 0.85), value: message)
    }
}

extension View {
    // Presents `message` as an auto-dismissing bottom toast, clearing the
    // binding once it has been on screen for `duration` seconds.
    func toast(message: Binding<String?>, duration: Double = toastDefaultDuration) -> some View {
        modifier(ToastModifier(message: message, duration: duration))
    }
}

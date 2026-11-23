import SwiftUI

struct OnboardingView: View {
    @Environment(\.dismiss) private var dismiss

    private static let dumberURL = URL(string: "https://github.com/EKA2L1/Dumber")!
    private static let dumpsURL = URL(string: "https://eka2l1.miraheze.org/wiki/Important_Links")!

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 0) {
                    OnboardingStep(number: 1, title: "onboarding.dump.title") {
                        Text("onboarding.dump.body")
                        HStack(spacing: 16) {
                            Link(destination: Self.dumberURL) {
                                Label("onboarding.dump.dumber", systemImage: "arrow.up.right")
                            }
                            Link(destination: Self.dumpsURL) {
                                Label("onboarding.dump.links", systemImage: "arrow.up.right")
                            }
                        }
                        .font(.subheadline.weight(.medium))
                        .foregroundStyle(.tint)
                    }

                    OnboardingStep(number: 2, title: "onboarding.rom.title") {
                        Text("onboarding.rom.body")
                    }

                    OnboardingStep(number: 3, title: "onboarding.game.title", isLast: true) {
                        Text("onboarding.game.body \(Image(systemName: "plus"))")
                    }

                    Text("onboarding.legal")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                .padding(20)
                .frame(maxWidth: 560, alignment: .leading)
                .frame(maxWidth: .infinity)
            }
            .navigationTitle("onboarding.title")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("onboarding.done") { dismiss() }
                }
            }
        }
        .modifier(PhoneHalfSheet())
    }
}

// Detents turn the iPad form sheet into a bottom sheet, so only iPhone gets them.
private struct PhoneHalfSheet: ViewModifier {
    func body(content: Content) -> some View {
        if UIDevice.current.userInterfaceIdiom == .phone {
            content.presentationDetents([.medium, .large])
        } else {
            content
        }
    }
}

private struct OnboardingStep<Content: View>: View {
    let number: Int
    let title: LocalizedStringKey
    var isLast = false
    @ViewBuilder let content: Content

    var body: some View {
        HStack(alignment: .top, spacing: 14) {
            VStack(spacing: 6) {
                Text(verbatim: "\(number)")
                    .font(.subheadline.weight(.bold))
                    .foregroundStyle(.white)
                    .frame(width: 28, height: 28)
                    .background(Color.accentColor, in: Circle())
                if !isLast {
                    Rectangle()
                        .fill(Color(.separator))
                        .frame(width: 2)
                        .frame(maxHeight: .infinity)
                }
            }

            VStack(alignment: .leading, spacing: 8) {
                Text(title)
                    .font(.headline)
                    .padding(.top, 3)
                content
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .padding(.bottom, 28)
        }
    }
}

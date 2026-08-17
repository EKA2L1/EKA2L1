import SwiftUI

struct OnboardingView: View {
    var onFinish: () -> Void

    @Environment(\.dismiss) private var dismiss
    @State private var page = 0

    private let pages: [(symbol: String, title: LocalizedStringResource, body: LocalizedStringResource)] = [
        ("iphone.and.arrow.forward", "onboarding.device.title", "onboarding.device.body"),
        ("folder.badge.plus", "onboarding.install.title", "onboarding.install.body"),
        ("hand.raised", "onboarding.legal.title", "onboarding.legal.body")
    ]

    var body: some View {
        NavigationStack {
            VStack(spacing: 24) {
                TabView(selection: $page) {
                    ForEach(pages.indices, id: \.self) { index in
                        VStack(spacing: 18) {
                            Image(systemName: pages[index].symbol)
                                .font(.system(size: 54, weight: .semibold))
                                .foregroundStyle(.tint)
                                .frame(width: 86, height: 86)
                            Text(pages[index].title)
                                .font(.title2.weight(.semibold))
                                .multilineTextAlignment(.center)
                            Text(pages[index].body)
                                .font(.body)
                                .foregroundStyle(.secondary)
                                .multilineTextAlignment(.center)
                                .fixedSize(horizontal: false, vertical: true)
                        }
                        .padding(.horizontal, 28)
                        .tag(index)
                    }
                }
                .tabViewStyle(.page(indexDisplayMode: .always))

                Button(page == pages.count - 1 ? "onboarding.done" : "onboarding.next") {
                    if page == pages.count - 1 {
                        onFinish()
                        dismiss()
                    } else {
                        withAnimation(.easeInOut(duration: 0.2)) {
                            page += 1
                        }
                    }
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.large)
                .padding(.horizontal, 28)
                .padding(.bottom, 20)
            }
            .navigationTitle("onboarding.title")
            .navigationBarTitleDisplayMode(.inline)
        }
        .presentationDetents([.large])
    }
}

#Preview {
    OnboardingView {}
}

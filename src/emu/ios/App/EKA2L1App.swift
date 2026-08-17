// SwiftUI shell for the EKA2L1 iOS port.
//
// scenePhase is the iOS contract for "app is foreground / background". The
// emulator must stop touching the EAGL context the moment we leave .active
// or the system will tear down our drawable and crash the next GL call.

import SwiftUI
import UIKit

// SwiftUI Menu is backed by a UICollectionView on current iOS releases. UIKit's
// type-selection setup crashes while building that context-menu collection, so
// provide the no-op hook before any menus are presented.
extension UICollectionView {
    @objc func _configureTypeSelectInteractionIfNeeded() {}
}

// Supplies the emulator screen's orientation policy. It allows every direction
// by default and narrows to the current direction when the in-game lock is on.
final class AppOrientationDelegate: NSObject, UIApplicationDelegate {
    func application(_ application: UIApplication,
                     supportedInterfaceOrientationsFor window: UIWindow?) -> UIInterfaceOrientationMask {
        lockedInterfaceOrientationMask
    }
}

@main
struct EKA2L1App: App {
    @UIApplicationDelegateAdaptor(AppOrientationDelegate.self) private var appDelegate
    @Environment(\.scenePhase) private var scenePhase

    var body: some Scene {
        WindowGroup {
            ContentView()
        }
        .onChange(of: scenePhase) { newPhase in
            switch newPhase {
            case .active:
                EKA2L1Bridge.shared.resume()
            case .inactive, .background:
                EKA2L1Bridge.shared.pause()
                // pause() deactivates the audio session, which CoreHaptics
                // shares; don't keep the iOS 16 fallback's generators alive
                // across that. SwiftUI handles this itself on iOS 17+.
                Haptics.release()
            @unknown default:
                break
            }
        }
    }
}

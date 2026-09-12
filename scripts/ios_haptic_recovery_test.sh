#!/usr/bin/env bash
set -euo pipefail

# Requires an Apple Silicon Mac, initialized submodules, a configured iOS build,
# and xcodebuildmcp. EKA2L1_IOS_BUILD_DIR overrides the generated header directory.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${EKA2L1_IOS_BUILD_DIR:-$ROOT_DIR/build/ios-simulator}"
SIMULATOR_ID="${1:?Usage: ios_haptic_recovery_test.sh SIMULATOR_UDID}"
TEST_DIR="$(mktemp -d /tmp/eka2l1-haptic-recovery.XXXXXX)"
APP="$TEST_DIR/HapticRecoveryTests.app"
BUNDLE_ID="com.eka2l1.haptic-recovery-tests"
trap 'rm -rf "$TEST_DIR"' EXIT
mkdir -p "$APP"

SDK="$(xcrun --sdk iphonesimulator --show-sdk-path)"
clang++ -target arm64-apple-ios16.0-simulator -isysroot "$SDK" \
    -std=c++17 -fobjc-arc -DDISABLE_LOGGING -DSPDLOG_FMT_EXTERNAL \
    -I "$ROOT_DIR/src/emu/drivers/include" \
    -I "$ROOT_DIR/src/emu/common/include" \
    -I "$BUILD_DIR/src/emu/common/include" \
    -I "$ROOT_DIR/src/external/spdlog/include" \
    -I "$ROOT_DIR/src/external/fmt/include" \
    "$ROOT_DIR/src/emu/ios/Tests/HapticRecoveryTests.mm" \
    "$ROOT_DIR/src/emu/drivers/src/hwrm/backend/vibration_ios.mm" \
    -framework Foundation -framework GameController \
    -framework CoreHaptics -framework CoreFoundation -framework UIKit \
    -o "$APP/HapticRecoveryTests"

python3 - "$APP" "$BUNDLE_ID" <<'PY'
from pathlib import Path
import plistlib, sys
app = Path(sys.argv[1])
with (app / 'Info.plist').open('wb') as out:
    plistlib.dump(dict(CFBundleIdentifier=sys.argv[2], CFBundleExecutable='HapticRecoveryTests',
                      CFBundleName='HapticRecoveryTests', CFBundlePackageType='APPL',
                      CFBundleVersion='1', CFBundleShortVersionString='1.0',
                      MinimumOSVersion='16.0', LSRequiresIPhoneOS=True,
                      UIDeviceFamily=[1, 2]), out)
PY
codesign --force --sign - "$APP"
xcodebuildmcp simulator install --simulator-id "$SIMULATOR_ID" --app-path "$APP"
DATA_DIR="$(xcrun simctl get_app_container "$SIMULATOR_ID" "$BUNDLE_ID" data)"
rm -f "$DATA_DIR/Documents/result.txt"
xcodebuildmcp simulator launch-app --simulator-id "$SIMULATOR_ID" --bundle-id "$BUNDLE_ID"
for _ in {1..20}; do
    if [ -f "$DATA_DIR/Documents/result.txt" ]; then
        cat "$DATA_DIR/Documents/result.txt"
        xcodebuildmcp simulator stop --simulator-id "$SIMULATOR_ID" --bundle-id "$BUNDLE_ID"
        exit 0
    fi
    sleep 1
done
echo "FAIL: haptic recovery harness did not complete; inspect the simulator crash report" >&2
exit 1

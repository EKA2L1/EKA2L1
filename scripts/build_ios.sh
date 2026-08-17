#!/usr/bin/env bash
#
# Build the iOS app.
#
# Generates Xcode projects under build/ios-device and build/ios-simulator using
# cmake/ios.toolchain.cmake, then runs `xcodebuild` for each.
#
# Usage:
#   scripts/build_ios.sh                 # build both device + simulator
#   scripts/build_ios.sh device          # device only (PLATFORM=OS64), unsigned
#   scripts/build_ios.sh device-signed   # device, code-signed (needs team)
#   scripts/build_ios.sh install         # build signed device + install to phone
#   scripts/build_ios.sh simulator       # simulator only (SIMULATORARM64)
#   scripts/build_ios.sh smoke           # build sim, install + launch on
#                                        # the booted iPhone simulator, grep
#                                        # log for EKA2L1_SMOKE: PASS / FAIL
#   scripts/build_ios.sh archive         # device .xcarchive signed for App
#                                        # Store / TestFlight distribution
#   scripts/build_ios.sh clean           # remove build/ios-* directories
#
# Environment variables:
#   EKA2L1_IOS_DEPLOYMENT_TARGET   default 16.0
#   EKA2L1_IOS_CONFIGURATION       default Debug
#   EKA2L1_IOS_SCHEME              default EKA2L1
#   EKA2L1_IOS_DYNARMIC            ON/OFF: compile the dynarmic JIT in.
#                                  Defaults: ON for simulator builds and the
#                                  unsigned `device` build (sideload IPA),
#                                  OFF for signed device builds; `archive` is
#                                  always OFF so App Store / TestFlight
#                                  binaries never carry JIT code. Runtime
#                                  still requires the sideloaded process to
#                                  have JIT permission.
#   EKA2L1_SANITIZER               address | thread: build with ASan or TSan.
#                                  Forces the dynarmic JIT off (sanitizers
#                                  cannot see into JIT-emitted code), so the
#                                  run goes through the dyncom interpreter.
#   EKA2L1_IOS_DEVELOPMENT_TEAM    Apple Development team id (device signing)
#   EKA2L1_IOS_DEVICE              target device name/udid for `install`
#   EKA2L1_IOS_ARCHIVE_PATH        .xcarchive output for `archive`
#                                  (default build/ios-device/EKA2L1.xcarchive)
#   EKA2L1_ASC_KEY_PATH            App Store Connect API key (.p8) for
#   EKA2L1_ASC_KEY_ID              -allowProvisioningUpdates on CI hosts;
#   EKA2L1_ASC_KEY_ISSUER_ID       all three must be set together
#
# This script intentionally does not require Qt or any signing identity.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

DEPLOYMENT_TARGET="${EKA2L1_IOS_DEPLOYMENT_TARGET:-16.0}"
# Propagate the resolved target to the FFmpeg sub-build so its prebuilt static
# libs carry the same min-version (otherwise ld warns about a 18.0/16.0 mismatch).
export EKA2L1_IOS_DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET}"
CONFIGURATION="${EKA2L1_IOS_CONFIGURATION:-Debug}"
SCHEME="${EKA2L1_IOS_SCHEME:-EKA2L1}"
# Set EKA2L1_IOS_DEVELOPMENT_TEAM=<team id> to build a code-signed device
# bundle that can be installed on a real iPhone (see `device-signed` /
# `install` commands below). Empty => unsigned build.
DEVELOPMENT_TEAM="${EKA2L1_IOS_DEVELOPMENT_TEAM:-}"

configure_one() {
    local label="$1"
    local platform="$2"
    local sdk="$3"
    local team="$4"
    local jit="${5:-OFF}"
    local build_dir="build/ios-${label}"

    # ffmpeg is not shipped prebuilt for iOS; its own script produces the slice.
    if [ ! -f "src/external/ffmpeg/ios/${label}/lib/libavcodec.a" ]; then
        (cd src/external/ffmpeg && EKA2L1_IOS_DEPLOYMENT_TARGET="${EKA2L1_IOS_DEPLOYMENT_TARGET:-16.0}" sh ios-build.sh "${label}")
    fi


    echo "==> Configuring ${label} (PLATFORM=${platform}, sdk=${sdk})"
    # CMake 4.x dropped compatibility with cmake_minimum_required < 3.5, and
    # several bundled submodules (glm, ext-boost, ...) still declare older
    # minimums. Pin a policy floor for the whole graph until those submodules
    # are bumped upstream.
    cmake -S . -B "${build_dir}" \
        -G Xcode \
        -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake \
        -DPLATFORM="${platform}" \
        -DDEPLOYMENT_TARGET="${DEPLOYMENT_TARGET}" \
        -DEKA2L1_IOS_DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET}" \
        -DEKA2L1_IOS_DEVELOPMENT_TEAM="${team}" \
        -DEKA2L1_IOS_ENABLE_FFMPEG=ON \
        -DEKA2L1_IOS_DYNARMIC="${jit}" \
        -DEKA2L1_SANITIZER="${EKA2L1_SANITIZER:-}" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_XCODE_ATTRIBUTE_SKIP_INSTALL=YES
    # SKIP_INSTALL=YES above keeps the dozens of static-lib targets out of the
    # archive's install phase; without it `xcodebuild archive` produces a
    # "Generic Xcode Archive" that -exportArchive refuses to distribute (the
    # app target overrides it back to NO in src/emu/ios/CMakeLists.txt).
}

build_one() {
    local label="$1"
    local platform="$2"
    local sdk="$3"
    local build_dir="build/ios-${label}"

    # Only the device bundle signs, and only when a team is provided.
    local team=""
    if [ "${label}" = "device" ]; then
        team="${DEVELOPMENT_TEAM}"
    fi

    # Dynarmic default: ON for simulator builds and the unsigned device build
    # (the sideload IPA), OFF for signed device builds. EKA2L1_IOS_DYNARMIC
    # overrides both ways.
    local jit_default=OFF
    if [ "${label}" = "simulator" ]; then
        jit_default=ON
    elif [ "${label}" = "device" ] && [ -z "${team}" ]; then
        jit_default=ON
    fi
    # Sanitizers cannot instrument JIT-emitted code; force the interpreter.
    if [ -n "${EKA2L1_SANITIZER:-}" ]; then
        jit_default=OFF
    fi
    local jit="${EKA2L1_IOS_DYNARMIC:-${jit_default}}"

    configure_one "${label}" "${platform}" "${sdk}" "${team}" "${jit}"

    echo "==> Building ${label}"
    if [ -n "${team}" ]; then
        # Signed device build: let Xcode auto-provision (registers the app id
        # and the connected device's UDID against the team on demand).
        xcodebuild \
            -project "${build_dir}/EKA2L1.xcodeproj" \
            -scheme "${SCHEME}" \
            -configuration "${CONFIGURATION}" \
            -sdk "${sdk}" \
            -allowProvisioningUpdates \
            DEVELOPMENT_TEAM="${team}" \
            CODE_SIGN_STYLE=Automatic \
            build
    else
        xcodebuild \
            -project "${build_dir}/EKA2L1.xcodeproj" \
            -scheme "${SCHEME}" \
            -configuration "${CONFIGURATION}" \
            -sdk "${sdk}" \
            CODE_SIGNING_ALLOWED=NO \
            build
    fi
}

# Produce a device .xcarchive for TestFlight / App Store distribution.
# The archive itself signs automatically for development (overriding the
# identity to "Apple Distribution" here conflicts with the target's automatic
# signing style and fails the archive); the distribution re-sign happens at
# xcodebuild -exportArchive time. On a CI host, pass an App Store Connect API
# key via EKA2L1_ASC_KEY_* so -allowProvisioningUpdates can create the
# development cert + profile on the fly.
archive_device() {
    if [ -z "${DEVELOPMENT_TEAM}" ]; then
        echo "archive: set EKA2L1_IOS_DEVELOPMENT_TEAM=<team id> first." >&2
        exit 6
    fi

    # The archive feeds App Store / TestFlight: never compile the JIT in.
    configure_one device OS64 iphoneos "${DEVELOPMENT_TEAM}" OFF

    local archive_path="${EKA2L1_IOS_ARCHIVE_PATH:-build/ios-device/EKA2L1.xcarchive}"

    local auth_args=()
    if [ -n "${EKA2L1_ASC_KEY_PATH:-}" ]; then
        auth_args+=(
            -authenticationKeyPath "${EKA2L1_ASC_KEY_PATH}"
            -authenticationKeyID "${EKA2L1_ASC_KEY_ID:?EKA2L1_ASC_KEY_ID required with EKA2L1_ASC_KEY_PATH}"
            -authenticationKeyIssuerID "${EKA2L1_ASC_KEY_ISSUER_ID:?EKA2L1_ASC_KEY_ISSUER_ID required with EKA2L1_ASC_KEY_PATH}"
        )
    fi

    echo "==> Archiving device (${CONFIGURATION}) -> ${archive_path}"
    xcodebuild \
        -project build/ios-device/EKA2L1.xcodeproj \
        -scheme "${SCHEME}" \
        -configuration "${CONFIGURATION}" \
        -destination 'generic/platform=iOS' \
        -archivePath "${archive_path}" \
        -allowProvisioningUpdates \
        ${auth_args[@]+"${auth_args[@]}"} \
        DEVELOPMENT_TEAM="${DEVELOPMENT_TEAM}" \
        CODE_SIGN_STYLE=Automatic \
        GCC_GENERATE_DEBUGGING_SYMBOLS=YES \
        DEBUG_INFORMATION_FORMAT=dwarf-with-dsym \
        archive

    # CMake redirects CONFIGURATION_BUILD_DIR into the source build tree, so
    # dsymutil writes the dSYM next to the .app instead of the archive's
    # products dir and Xcode's archive collector misses it. Stage it into the
    # .xcarchive manually so -exportArchive uploads symbols to App Store
    # Connect and CI can attach it as an artifact.
    local dsym="build/ios-device/src/emu/ios/${CONFIGURATION}-iphoneos/EKA2L1.app.dSYM"
    if [ -d "${dsym}" ]; then
        mkdir -p "${archive_path}/dSYMs"
        rm -rf "${archive_path}/dSYMs/EKA2L1.app.dSYM"
        cp -R "${dsym}" "${archive_path}/dSYMs/"
    else
        echo "archive: warning: no dSYM found at ${dsym}" >&2
    fi
}

# Build a signed device bundle and install it onto a connected iPhone.
install_device() {
    if [ -z "${DEVELOPMENT_TEAM}" ]; then
        echo "install: set EKA2L1_IOS_DEVELOPMENT_TEAM=<team id> first." >&2
        echo "  Find it with: security find-identity -v -p codesigning" >&2
        exit 6
    fi

    build_one device OS64 iphoneos

    local device_app="build/ios-device/src/emu/ios/${CONFIGURATION}-iphoneos/EKA2L1.app"
    if [ ! -d "${device_app}" ]; then
        echo "install: built bundle not found at ${device_app}" >&2
        exit 7
    fi

    # Pick the target device. EKA2L1_IOS_DEVICE may be a name, UDID or the
    # devicectl identifier; default to the first connected device.
    local device="${EKA2L1_IOS_DEVICE:-}"
    if [ -z "${device}" ]; then
        local tmp_json
        tmp_json="$(mktemp -t eka2l1-devices)"
        xcrun devicectl list devices --json-output "${tmp_json}" >/dev/null 2>&1 || true
        device="$(python3 -c '
import json, sys
try:
    d = json.load(open(sys.argv[1]))
except Exception:
    sys.exit(0)
for dev in d.get("result", {}).get("devices", []):
    if dev.get("connectionProperties", {}).get("tunnelState") == "connected":
        print(dev.get("hardwareProperties", {}).get("udid", ""))
        break
' "${tmp_json}")"
        rm -f "${tmp_json}"
        if [ -z "${device}" ]; then
            echo "install: no connected device found. Plug in + trust the iPhone," >&2
            echo "  or set EKA2L1_IOS_DEVICE=<name|udid>. Connected devices:" >&2
            xcrun devicectl list devices >&2 || true
            exit 8
        fi
    fi
    echo "==> Installing to device '${device}'"
    xcrun devicectl device install app --device "${device}" "${device_app}"
}

smoke_test() {
    local sim_app="build/ios-simulator/src/emu/ios/${CONFIGURATION}-iphonesimulator/EKA2L1.app"
    local bundle_id="${EKA2L1_IOS_BUNDLE_ID:-com.eka2l1.emulator}"
    local timeout_s="${EKA2L1_IOS_SMOKE_TIMEOUT:-30}"

    if [ ! -d "${sim_app}" ]; then
        echo "==> Smoke: simulator .app missing, building first"
        build_one simulator SIMULATORARM64 iphonesimulator
    fi

    local booted
    booted="$(xcrun simctl list devices booted 2>/dev/null \
        | awk -F '[()]' '/Booted/ { print $2; exit }')"
    if [ -z "${booted}" ]; then
        echo "Smoke: no booted iPhone simulator. Boot one in Simulator.app first." >&2
        exit 3
    fi
    echo "==> Smoke: target simulator ${booted}"

    xcrun simctl terminate "${booted}" "${bundle_id}" >/dev/null 2>&1 || true
    xcrun simctl install "${booted}" "${sim_app}"
    xcrun simctl launch --terminate-running-process "${booted}" "${bundle_id}" >/dev/null

    local started_at
    started_at="$(date +%s)"
    local deadline=$((started_at + timeout_s))
    local marker=""
    while [ "$(date +%s)" -lt "${deadline}" ]; do
        marker="$(xcrun simctl spawn "${booted}" log show --last 5s \
                    --predicate "process == \"EKA2L1\"" 2>/dev/null \
                  | grep -E 'EKA2L1_SMOKE: (PASS|FAIL)' \
                  | tail -1 || true)"
        if [ -n "${marker}" ]; then
            break
        fi
        sleep 1
    done

    xcrun simctl terminate "${booted}" "${bundle_id}" >/dev/null 2>&1 || true

    if [ -z "${marker}" ]; then
        echo "Smoke: timeout after ${timeout_s}s without EKA2L1_SMOKE marker" >&2
        exit 4
    fi

    echo "${marker}"
    if echo "${marker}" | grep -q 'EKA2L1_SMOKE: PASS'; then
        exit 0
    fi
    exit 5
}

case "${1:-all}" in
    clean)
        rm -rf build/ios-device build/ios-simulator
        echo "Removed build/ios-device and build/ios-simulator"
        ;;
    device)
        build_one device OS64 iphoneos
        ;;
    device-signed)
        if [ -z "${DEVELOPMENT_TEAM}" ]; then
            echo "device-signed: set EKA2L1_IOS_DEVELOPMENT_TEAM=<team id> first." >&2
            exit 6
        fi
        build_one device OS64 iphoneos
        ;;
    install)
        install_device
        ;;
    archive)
        archive_device
        ;;
    simulator)
        build_one simulator SIMULATORARM64 iphonesimulator
        ;;
    smoke)
        build_one simulator SIMULATORARM64 iphonesimulator
        smoke_test
        ;;
    all|"")
        build_one device OS64 iphoneos
        build_one simulator SIMULATORARM64 iphonesimulator
        ;;
    *)
        echo "Unknown command: ${1}" >&2
        echo "Usage: $0 [device|device-signed|install|archive|simulator|smoke|all|clean]" >&2
        exit 2
        ;;
esac

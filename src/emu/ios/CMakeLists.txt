# EKA2L1 iOS frontend
#
# Builds the SwiftUI shell plus the Obj-C++ bridge (IosEmulator)
# that drives the emu core on iOS: device install/boot, applist scan + launch,
# EAGL rendering, input, audio and vibration. Mirrors the Android JNI bridge
# under src/emu/android/app/src/main/cpp.
#
# The expected invocation is:
#   cmake -S . -B build/ios -G Xcode \
#       -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake \
#       -DPLATFORM=OS64
#
# Or with PLATFORM=SIMULATORARM64 for the simulator.

# Swift is not a language in the top-level project() declaration, so enable
# it here before declaring the bundle target. The Xcode generator + leetal
# ios.toolchain understand Swift natively.
enable_language(Swift)

set(EKA2L1_IOS_APP EKA2L1)
set(EKA2L1_IOS_BUNDLE_ID "com.eka2l1.emulator" CACHE STRING "iOS bundle identifier")

# Apple Development team for real-device installs. Leave empty for the
# unsigned build (simulator / CI). Set it to a
# team id (e.g. via -DEKA2L1_IOS_DEVELOPMENT_TEAM=XXXXXXXXXX or the
# EKA2L1_IOS_DEVELOPMENT_TEAM env passed through build_ios.sh) to flip the
# target into automatic code signing so the .app can be sideloaded onto a
# physical iPhone. dyncom-only means no JIT entitlement is needed, so a plain
# Apple Development signature is enough.
set(EKA2L1_IOS_DEVELOPMENT_TEAM "" CACHE STRING "Apple Development team id for device signing")

set(EKA2L1_IOS_RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Resources")
set(EKA2L1_IOS_APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/App")
set(EKA2L1_IOS_BRIDGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Bridge")

set(EKA2L1_IOS_SOURCES
    "${EKA2L1_IOS_APP_DIR}/EKA2L1App.swift"
    "${EKA2L1_IOS_APP_DIR}/ContentView.swift"
    "${EKA2L1_IOS_APP_DIR}/OnboardingView.swift"
    "${EKA2L1_IOS_APP_DIR}/EKA2L1Bridge.swift"
    "${EKA2L1_IOS_APP_DIR}/EmulatorView.swift"
    "${EKA2L1_IOS_APP_DIR}/VirtualKeypad.swift"
    "${EKA2L1_IOS_APP_DIR}/KeypadComponents.swift"
    "${EKA2L1_IOS_APP_DIR}/Haptics.swift"
    "${EKA2L1_IOS_APP_DIR}/EmulatorViewController.swift"
    "${EKA2L1_IOS_APP_DIR}/ControllerMapping.swift"
    "${EKA2L1_IOS_APP_DIR}/SettingsView.swift"
    "${EKA2L1_IOS_APP_DIR}/Toast.swift"
    "${EKA2L1_IOS_BRIDGE_DIR}/IosEmulator.h"
    "${EKA2L1_IOS_BRIDGE_DIR}/IosEmulator.mm"
    "${EKA2L1_IOS_BRIDGE_DIR}/EKA2L1-Bridging-Header.h"
    "${EKA2L1_IOS_RESOURCES_DIR}/Info.plist"
    "${EKA2L1_IOS_RESOURCES_DIR}/EKA2L1.entitlements"
    "${EKA2L1_IOS_RESOURCES_DIR}/Localizable.xcstrings"
    "${EKA2L1_IOS_RESOURCES_DIR}/PrivacyInfo.xcprivacy"
)

add_executable(${EKA2L1_IOS_APP} MACOSX_BUNDLE ${EKA2L1_IOS_SOURCES})

set(EKA2L1_IOS_BUNDLE_RESOURCES
    "${EKA2L1_IOS_RESOURCES_DIR}/Localizable.xcstrings"
    "${EKA2L1_IOS_RESOURCES_DIR}/PrivacyInfo.xcprivacy")
target_sources(${EKA2L1_IOS_APP} PRIVATE ${EKA2L1_IOS_BUNDLE_RESOURCES})
# "Resources" maps to Xcode's Copy Bundle Resources phase, which is what
# compiles the string catalog into <lang>.lproj/Localizable.strings (a plain
# copy-files phase like "." ships the raw .xcstrings and runtime lookups fall
# back to the bare string keys). Xcode flattens the phase output to the bundle
# root on iOS.
set_source_files_properties(${EKA2L1_IOS_BUNDLE_RESOURCES} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources")
set_source_files_properties("${EKA2L1_IOS_RESOURCES_DIR}/Localizable.xcstrings" PROPERTIES
    XCODE_LAST_KNOWN_FILE_TYPE "text.json.xcstrings")

# The emulator reads its shipped resources through paths relative to the
# working directory ("resources/..", "patch/.."), which on iOS is the sandbox's
# data tree. Rather than staging copies there, IosEmulator.mm calls
# set_runtime_resource_root() with the bundle folder laid out below, so the
# read-only .app is the only source at runtime — a missing copy here means a
# null shader FILE* and an unpatched guest, not a silent fallback.
#
# NB: the root must NOT be named "resources" — the build host's filesystem is
# case-insensitive, so a top-level "resources" collides with the reserved
# "Resources" bundle directory and codesign rejects the .app with "bundle
# format unrecognized, invalid, or unsuitable". One level down inside "emures"
# the names the emulator expects are free to be used verbatim.
#
# emures/resources: the GLES shader sources loaded by ogl_graphics_driver::
# do_init, plus the default SF2 bank player_tsf opens as
# "resources/defaultbank.sf2".
file(GLOB EKA2L1_IOS_SHADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../drivers/resources/gles/*")
list(APPEND EKA2L1_IOS_SHADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../drivers/resources/defaultbank.sf2")
target_sources(${EKA2L1_IOS_APP} PRIVATE ${EKA2L1_IOS_SHADER_FILES})
set_source_files_properties(${EKA2L1_IOS_SHADER_FILES} PROPERTIES
    MACOSX_PACKAGE_LOCATION "emures/resources")

# emures/resources/upscale: the upscale filters, which the driver looks up as
# "resources//upscale//<name>.frag".
file(GLOB EKA2L1_IOS_UPSCALE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../drivers/resources/upscale/*.frag")
target_sources(${EKA2L1_IOS_APP} PRIVATE ${EKA2L1_IOS_UPSCALE_FILES})
set_source_files_properties(${EKA2L1_IOS_UPSCALE_FILES} PROPERTIES
    MACOSX_PACKAGE_LOCATION "emures/resources/upscale")

# emures/patch: the prebuilt HLE patch DLLs + .map files that
# lib_manager::load_patch_libraries (epoc.cpp) scans at boot to apply ROM /
# library patches (audio via mediaclientaudiostream, the scdv screen driver,
# etc.). Without them the guest boots unpatched — no sound, and apps that hit an
# unpatched path (e.g. Snakes falling back to d_display.ldd) panic the active
# scheduler and black-screen. Names are flat and unique, so a single package
# location stages them alongside each other, matching the layout
# load_patch_libraries expects.
file(GLOB EKA2L1_IOS_PATCH_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../patch/*/group/*.map"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../patch/*/group/*.dll")
target_sources(${EKA2L1_IOS_APP} PRIVATE ${EKA2L1_IOS_PATCH_FILES})
set_source_files_properties(${EKA2L1_IOS_PATCH_FILES} PROPERTIES
    MACOSX_PACKAGE_LOCATION "emures/patch")

target_include_directories(${EKA2L1_IOS_APP} PRIVATE "${EKA2L1_IOS_BRIDGE_DIR}")
target_compile_definitions(${EKA2L1_IOS_APP} PRIVATE GLES_SILENCE_DEPRECATION)

# Link the emu static libraries the bridge depends on. We mirror the Android
# JNI bridge link line (see src/emu/android/app/src/main/cpp/CMakeLists.txt)
# because that target is known-good for a non-Qt frontend.
target_link_libraries(${EKA2L1_IOS_APP} PRIVATE
    common
    cpu
    drivers
    epoc
    epockern
    epocloader
    epocpkg
    epocservs
    lunasvg
    sqlite3
    yaml-cpp
    "-framework GameController")


# ---------------------------------------------------------------------------
# App icon (Apple Icon Composer "Liquid Glass" .icon, Xcode 26).
#
# Keep Resources/AppIcon.icon as the only checked-in source of truth. The iOS CI
# workflow runs on macOS 26 / Xcode 26, so actool can compile the Icon Composer
# bundle directly into Assets.car plus flat PNG fallbacks. The matching
# CFBundleIcons keys live in Info.plist.
set(EKA2L1_APPICON "${CMAKE_CURRENT_SOURCE_DIR}/Resources/AppIcon.icon")
add_custom_command(TARGET ${EKA2L1_IOS_APP} POST_BUILD
    COMMAND /bin/sh -c
        "xcrun actool --compile \"$TARGET_BUILD_DIR/$WRAPPER_NAME\" --app-icon AppIcon --output-partial-info-plist \"${CMAKE_CURRENT_BINARY_DIR}/appicon_partial.plist\" --platform \"$PLATFORM_NAME\" --minimum-deployment-target \"${EKA2L1_IOS_DEPLOYMENT_TARGET}\" --target-device iphone --target-device ipad \"${EKA2L1_APPICON}\""
    VERBATIM)

set_target_properties(${EKA2L1_IOS_APP} PROPERTIES
    BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${EKA2L1_IOS_RESOURCES_DIR}/Info.plist"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${EKA2L1_IOS_BUNDLE_ID}"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
    XCODE_ATTRIBUTE_SWIFT_VERSION "6.0"
    XCODE_ATTRIBUTE_LOCALIZATION_PREFERS_STRING_CATALOGS "YES"
    XCODE_ATTRIBUTE_SWIFT_EMIT_LOC_STRINGS "YES"
    XCODE_ATTRIBUTE_STRING_CATALOG_GENERATE_SYMBOLS "YES"
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${EKA2L1_IOS_BRIDGE_DIR}/EKA2L1-Bridging-Header.h"
    # Xcode 26's swift driver rejects the bare -stdlib=libc++ that CMake's
    # Xcode generator otherwise pushes into OTHER_SWIFT_FLAGS for mixed
    # Swift/C++ targets. Explicitly reset OTHER_SWIFT_FLAGS to the inherited
    # defaults so the unsupported flag is not appended. Swift only talks to
    # the Obj-C++ bridge through plain Obj-C, so C++ interop on the Swift
    # side stays disabled.
    XCODE_ATTRIBUTE_SWIFT_OBJC_INTEROP_MODE "objc"
    XCODE_ATTRIBUTE_OTHER_SWIFT_FLAGS "$(inherited) -Xcc -DGLES_SILENCE_DEPRECATION"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${EKA2L1_IOS_DEPLOYMENT_TARGET}"
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${EKA2L1_IOS_RESOURCES_DIR}/EKA2L1.entitlements"
    XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
    # Archive/TestFlight: the app is the only product that installs into the
    # archive (build_ios.sh defaults SKIP_INSTALL=YES for every other target,
    # otherwise the .xcarchive degrades to a non-distributable "Generic Xcode
    # Archive"). INSTALL_PATH matches Xcode's iOS app template so the archive
    # places the bundle under Products/Applications.
    XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
    XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)")

if (EKA2L1_IOS_DEVELOPMENT_TEAM STREQUAL "")
    # Unsigned by default. Developers can flip these by passing
    # -DEKA2L1_IOS_DEVELOPMENT_TEAM=<team> (see above) or by opening the
    # generated Xcode project and selecting a team.
    set_target_properties(${EKA2L1_IOS_APP} PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    message(STATUS "EKA2L1 iOS: unsigned build (no development team set)")
else()
    # Real-device install path: automatic signing against the given team.
    # Xcode registers the bundle id / device UDID and provisions on demand
    # (xcodebuild needs -allowProvisioningUpdates; see scripts/build_ios.sh).
    set_target_properties(${EKA2L1_IOS_APP} PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${EKA2L1_IOS_DEVELOPMENT_TEAM}"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "YES"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "YES")
    message(STATUS "EKA2L1 iOS: signed build for team ${EKA2L1_IOS_DEVELOPMENT_TEAM}")
endif()

message(STATUS "EKA2L1 iOS frontend skeleton: bundle ${EKA2L1_IOS_BUNDLE_ID}, deployment ${EKA2L1_IOS_DEPLOYMENT_TARGET}")

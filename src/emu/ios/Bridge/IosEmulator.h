// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Obj-C facade for the iOS-side emulator state. Mirrors the role
// `eka2l1::android::emulator` plays for the Android frontend: device install /
// boot, applist scan + launch, render-surface attach, input, audio (cubeb),
// vibration (Core Haptics) and SIS install. Sensors / camera are not wired
// yet. Implemented on top of the C++ `eka2l1::ios::emulator` in IosEmulator.mm.

#import <Foundation/Foundation.h>
#import <QuartzCore/CAEAGLLayer.h>

NS_ASSUME_NONNULL_BEGIN

@interface EKA2L1AppEntry : NSObject
@property(nonatomic, assign) uint32_t uid;
@property(nonatomic, copy) NSString *name;
// YES for apps that ship in the device ROM (built-in system apps), NO for apps
// the user installed from a SIS/SISX package. Lets the frontend default to
// showing only user-installed apps.
@property(nonatomic, assign) BOOL system;
@end

// An installed Symbian device (firmware). `index` matches device_manager's
// ordering so it can be passed straight back to bootDeviceAtIndex:.
@interface EKA2L1DeviceEntry : NSObject
@property(nonatomic, assign) NSUInteger index;
@property(nonatomic, copy) NSString *firmwareCode;
@property(nonatomic, copy) NSString *manufacturer;
@property(nonatomic, copy) NSString *model;
@end

@interface EKA2L1NGageInstallReport : NSObject
@property(nonatomic, assign) NSInteger result;
@property(nonatomic, copy) NSString *gameName;
@end

// A guest system language the current device's ROM ships. `code` is the
// Symbian TLanguage value stored in config.yml.
@interface EKA2L1LanguageEntry : NSObject
@property(nonatomic, assign) NSInteger code;
@property(nonatomic, copy) NSString *name;
@end

// Mirrors eka2l1::device_installation_error 1:1, plus one iOS-only case the
// bridge raises itself: `Cancelled`, when the user stopped the install (the
// installers report a cancel as a generic failure / corrupt ROM, which would be
// a lie to show). Frontend maps these to the Android-sourced strings.
typedef NS_ENUM(NSInteger, EKA2L1InstallResult) {
    EKA2L1InstallResultSuccess = 0,
    EKA2L1InstallResultNotExist,
    EKA2L1InstallResultInsufficient,
    EKA2L1InstallResultRpkgCorrupt,
    EKA2L1InstallResultDetermineProductFailure,
    EKA2L1InstallResultAlreadyExist,
    EKA2L1InstallResultGeneralFailure,
    EKA2L1InstallResultRomFailToCopy,
    EKA2L1InstallResultVplInvalid,
    EKA2L1InstallResultRofsCorrupt,
    EKA2L1InstallResultRomCorrupt,
    EKA2L1InstallResultFpsxCorrupt,
    EKA2L1InstallResultNeedRpkg,
    EKA2L1InstallResultCancelled,
    // The .7z file could not be opened or a member failed to decompress.
    EKA2L1InstallResultArchiveCorrupt,
    // The .7z opened fine but holds no ROM and no unpacked device dump.
    EKA2L1InstallResultArchiveNoDevice,
};

@interface EKA2L1Emulator : NSObject

+ (instancetype)shared;

// Lifecycle ----------------------------------------------------------------
// Initialise the underlying eka2l1::system using the Documents directory at
// `documentsPath`. Creates the data/ tree if missing and points the shipped
// resource lookups (shaders, MIDI bank, HLE patches) at the app bundle.
- (BOOL)startWithDocumentsPath:(NSString *)documentsPath;

// Tear the system down. Called from the SwiftUI shutdown path.
- (void)shutdown;

// Devices + applist -------------------------------------------------------
// List installed Symbian devices (from device_manager). Empty until the user
// installs one via installDeviceWithRomPath:rpkgPath:.
- (NSArray<EKA2L1DeviceEntry *> *)installedDevices;

// Index of the currently-booted device, or -1 if none is booted yet.
- (NSInteger)currentDeviceIndex;

// Install a device from a raw ROM dump (and optionally an RPKG file). Mirrors
// the Android launcher::install_device path: install_rpkg when the ROM needs
// it, else install_rom. Writes into the sandbox storage and persists
// devices.yml. Does NOT boot the device — call bootDeviceAtIndex: after.
//
// Unpacking a firmware runs for minutes on a large dump, so both installer
// callbacks are wired: `progress` reports 0…1 completion (throttled, delivered
// on the calling thread) and `cancelCheck` is polled between files — return YES
// from it to stop, and the installer reverts what it wrote. Runs synchronously;
// call it off the main thread.
- (EKA2L1InstallResult)installDeviceWithRomPath:(NSString *)romPath
                                       rpkgPath:(nullable NSString *)rpkgPath
                                       progress:(nullable void (^)(double fraction))progress
                                    cancelCheck:(nullable BOOL (^)(void))cancelCheck
    NS_SWIFT_NAME(installDevice(romPath:rpkgPath:progress:cancelCheck:));

// Install a device from a .7z archive. Two packagings are understood, since
// both are what gets shared: a ROM image with the RPKG that goes with it, or an
// already-unpacked device (a `data/drives/z/<code>/` tree plus the ROM under
// `data/roms/<code>/`) which is copied straight into place. Which one it is
// comes out of the archive's file list, so the caller doesn't choose.
//
// Same contract as installDeviceWithRomPath: above — progress/cancel callbacks,
// synchronous, does not boot the device.
- (EKA2L1InstallResult)installDeviceWithArchivePath:(NSString *)archivePath
                                           progress:(nullable void (^)(double fraction))progress
                                        cancelCheck:(nullable BOOL (^)(void))cancelCheck
    NS_SWIFT_NAME(installDevice(archivePath:progress:cancelCheck:));

// Boot a previously-installed device by index: (re)builds the system, sets
// the device, mounts drives, binds the graphics driver. Returns YES on
// success.
//
// The device selection is not persisted here. A ROM/RPKG dump that is damaged
// in a way the installer doesn't catch only fails while the device comes up,
// and it fails hard — so the boot is first recorded as an attempt on disk, and
// only confirmDeviceBoot promotes that to a saved selection. A run that dies in
// between leaves the marker, and the next launch skips that device instead of
// crashing again forever (see takeFailedBootDeviceCode).
- (BOOL)bootDeviceAtIndex:(NSUInteger)index;

// Sign off on the boot above: retires the crash marker and writes the device
// selection to config.yml. Call once the frontend has the app list for the
// booted device — that is the proof the dump can carry the process. No-op when
// no boot is pending or one is still running.
- (void)confirmDeviceBoot;

// Firmware code of the device the previous run of the app died on while
// booting, or nil when the last run was clean. Cleared by this call, so the
// frontend gets to report it exactly once per launch.
- (nullable NSString *)takeFailedBootDeviceCode;

// Delete an installed device by index: removes its entry from devices.yml and
// deletes the device's ROM filesystem (drive Z) and resident ROM image from
// the sandbox. Does NOT reboot — the caller decides which device to boot next
// (device_manager decrements its current index to keep pointing at the same
// device when a lower-indexed one is removed). Returns YES on success.
- (BOOL)deleteDeviceAtIndex:(NSUInteger)index NS_SWIFT_NAME(deleteDevice(at:));

// Rename an installed device by index: updates the device's model (the name
// shown on the home title / device switcher) and persists devices.yml. Mirrors
// the Android launcher::set_device_name path. Does NOT reboot. Returns YES on
// success (NO if the index is out of range or no system is up).
- (BOOL)renameDeviceAtIndex:(NSUInteger)index
                     toName:(NSString *)name
    NS_SWIFT_NAME(renameDevice(at:to:));

// Mirrors the Android/Qt "Rescan devices" action: rebuild device_manager by
// walking drive Z's storage tree for device dumps (recovers devices dropped
// from devices.yml, e.g. after restoring a backup that lost it). Does NOT
// boot the device — call bootDeviceAtIndex: after if this returns YES.
// Returns YES if the scan found at least one device.
- (BOOL)rescanDevices;

// Trigger applist rescan + return the resulting (uid, name) list.
- (NSArray<EKA2L1AppEntry *> *)rescanApps;

// Launch a previously-listed app. Runs off the main thread: binding the
// graphics driver / setting the screen mode issues synchronous graphics
// commands, and the graphics worker thread bounces the CAEAGLLayer attach back
// onto the main queue (an iOS-26 hard requirement). Driving the launch on the
// main thread would deadlock those two against each other, so the work runs on
// a serial control queue and `completion` (if given) fires on the main queue
// with the launch result. Set appExitHandler before calling.
- (void)launchAppWithUID:(uint32_t)uid completion:(nullable void (^)(BOOL success))completion;

// Invoked on the main queue when the currently-running app's process exits —
// whether it left normally (Exit soft key), was killed, or panicked. The
// frontend uses it to close the emulator screen when the guest app goes away.
// `fatalDetails` is non-nil when the guest died from a panic / non-zero
// terminate and should be shown before closing. Cleared by passing nil. Set
// this before launchAppWithUID:.
@property(nonatomic, copy, nullable) void (^appExitHandler)(NSString *_Nullable fatalDetails);

// Kill the app launched by the last launchAppWithUID:, in lockstep with the
// frontend closing the emulator screen. No-op if nothing is running or the
// process has already exited. Killing fires the process logon, so clear
// appExitHandler first when the caller is already tearing the screen down.
- (void)closeRunningApp;

// Install a SIS / SISX package onto the running device. Picks drive D for
// S80 devices and drive E otherwise, mirroring the Android install path.
- (BOOL)installSisAtPath:(NSString *)sisPath;

// Install a classic N-Gage game card, given either the card folder or an
// archive holding it. Returns the core ngage_game_card_install_error code plus
// the detected game name when available. Heavy; call from a background queue.
- (EKA2L1NGageInstallReport *)installNGageGameAtPath:(NSString *)cardPath;

// Uninstall a user-installed package by its app UID. Deletes the package's
// files and registration; ROM/system apps cannot be uninstalled. Returns NO if
// no matching installed package is found.
- (BOOL)uninstallAppWithUID:(uint32_t)uid;

// Render surface / lifecycle ----------------------------------------------
// Frontend hands the EAGLView's CAEAGLLayer here. Called from
// viewDidLayoutSubviews so re-orientation is handled.
- (void)attachLayer:(CAEAGLLayer *)layer
         pixelSize:(CGSize)pixelSize
              scale:(CGFloat)scale NS_SWIFT_NAME(attach(layer:pixelSize:scale:));
- (void)detachLayer NS_SWIFT_NAME(detachLayer());

- (void)pause;
- (void)resume;

// Input -------------------------------------------------------------------
// Single-touch dispatch from EAGLView.
typedef NS_ENUM(NSInteger, EKA2L1PointerPhase) {
    EKA2L1PointerPhaseBegan = 0,
    EKA2L1PointerPhaseMoved = 1,
    EKA2L1PointerPhaseEnded = 2,
    EKA2L1PointerPhaseCancelled = 3,
};

- (void)submitPointerEventAtX:(CGFloat)x
	                            y:(CGFloat)y
	                        phase:(EKA2L1PointerPhase)phase
	                    pointerId:(uintptr_t)pointerId NS_SWIFT_NAME(submitPointer(x:y:phase:pointerId:));

- (void)submitRawKey:(uint32_t)scanCode pressed:(BOOL)pressed;
- (void)tapRawKey:(uint32_t)scanCode;

// YES when the booted device's Symbian version drives its UI by touch
// (S60v5 / Symbian^3 and later). The frontend uses it to pick the fullscreen
// keypad layout by default for those ROMs.
- (BOOL)currentDeviceIsTouchScreen;

// Vertical anchor for the presented guest picture, in surface pixels.
// Pass a negative value to centre it (default). >= 0 pins the picture's top
// edge at that offset (clamped) — used to top-align the picture when a keypad
// overlays the bottom of the screen.
- (void)setDisplayAnchorTopPixels:(NSInteger)anchorTop;

// Available Window Server screen-mode indices plus the current mode. The
// snapshot keys are `modes` ([NSNumber]) and `current` (NSNumber).
- (NSDictionary<NSString *, id> *)guestScreenModeSnapshot;

// Select the guest's real Window Server screen mode and persist it through the
// shared per-app compatibility settings used by the other frontends.
- (void)setGuestScreenModeForAppUID:(uint32_t)uid
                               mode:(NSInteger)mode
                         completion:(void (^)(NSInteger mode))completion
    NS_SWIFT_NAME(setGuestScreenMode(forAppUID:mode:completion:));

// Per-app guest frame-rate limit (the emulated screen's vsync cap, persisted
// through the shared per-app compatibility settings). `limit` is a target FPS
// of 15 / 30 / 60; pass 0 (or negative) for "unlimited" (uncapped presentation
// for the running session). The value applies live to the focused screen and
// is restored on the app's next launch.
- (void)setGuestFrameLimitForAppUID:(uint32_t)uid limit:(NSInteger)limit
    NS_SWIFT_NAME(setGuestFrameLimit(forAppUID:limit:));

// The current guest frame-rate limit for `uid`: 15 / 30 / 60, or 0 for
// unlimited. Reads the live focused screen when a session is running, else the
// persisted per-app setting.
- (NSInteger)guestFrameLimitForAppUID:(uint32_t)uid
    NS_SWIFT_NAME(guestFrameLimit(forAppUID:));

// Current interface orientation as a CCW rotation from the device's natural
// portrait orientation: portrait 0, landscapeLeft 90, portraitUpsideDown 180,
// landscapeRight 270. Combined with the guest screen mode's rotation to keep
// accelerometer samples aligned with the on-screen guest — CoreMotion reports
// in the physical device frame and knows nothing about UIKit rotation. Push
// it whenever the emulator screen (re)lays out.
- (void)setHostInterfaceRotationDegrees:(NSInteger)degrees;

- (NSDictionary<NSString *, id> *)currentConfigSnapshot;
- (BOOL)applyConfigSnapshot:(NSDictionary<NSString *, id> *)snapshot;

// System language -----------------------------------------------------------
// Languages shipped by the currently-selected device's ROM. Empty until a
// device is installed. Mirrors the Android frontend's language picker source.
- (NSArray<EKA2L1LanguageEntry *> *)availableLanguages;

// The configured guest system language code, or -1 when unset.
- (NSInteger)currentLanguageCode;

// Set the guest system language: persists to config.yml, updates the kernel's
// current language and the live locale property so running guests observe the
// locale change. Apps pick up their translated resources on next launch.
- (void)setSystemLanguageCode:(NSInteger)code;

// JIT (dynarmic) support. `jitCompiledIn` is YES when this build carries the
// dynarmic backend (EKA2L1_IOS_DYNARMIC: simulator, or a sideload device
// build — never App Store / TestFlight). `jitAvailable` additionally
// requires the running process to have JIT permission (sideloaded with a
// debugger / JIT enabler attached); without it the emulator silently falls
// back to the interpreter even if the user opted in.
@property(nonatomic, readonly) BOOL jitCompiledIn;
@property(nonatomic, readonly) BOOL jitAvailable;
- (uint64_t)renderedFrameCount;

// Decode an app's registered icon (MIF / MBM / NVG / SVGB / SVG)
// and return a square RGBA PNG sized `sizePx` per side. Returns nil if
// the registration has no icon or all decode attempts fail. Safe to
// call from a background queue; SwiftUI consumes the NSData via
// `UIImage(data:)`.
- (nullable NSData *)iconPNGDataForUID:(uint32_t)uid sizePx:(NSUInteger)sizePx;

@end

NS_ASSUME_NONNULL_END

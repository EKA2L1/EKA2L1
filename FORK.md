# EKA2L1 FE (fork of EKA2L1)

This is an unofficial fork of [EKA2L1](https://github.com/EKA2L1/EKA2L1), the Symbian OS / N-Gage emulator.
It is **not** affiliated with or endorsed by the EKA2L1 team. All credit for the emulator goes to the
upstream authors and contributors. This fork only changes the Android app, as described below.

## What is different

### Launching games from frontends (ES-DE, IISU) now works with any active device

Upstream, a launch `.json` stores the `DeviceCode` of the device the game needs (for example an S60v3
phone for one game and an S60v5 phone for another). When the launch was made while a different device was
active, the app tried to hot-swap the device before the surface existed. That code path is barely exercised
on Android and the game would hang or crash.

In this fork:

- The device requested by the `.json` is selected **at boot** (`Emulator.setBootDeviceCode`, applied in
  `emulator::stage_one()` before the first `set_device`). There is no hot swap anymore.
- If the process is already running with another device, the app saves the requested device and restarts
  the process to relaunch the game, the same way the Devices menu does.
- If the requested device is not installed, you get a message and the current device is used.
- `launcher::launch_app` no longer dereferences a null registration. A wrong device or unknown app shows an
  error instead of crashing the app.

### Fork identity (so it can be installed next to the official app)

- `applicationId`: `com.github.eka2l1.fe` (the Java namespace is unchanged, the JNI depends on it).
- App name: **EKA2L1 FE**.
- An `activity-alias` named `com.github.eka2l1.fe.emu.EmulatorActivity` so launchers that use the short
  component name `.emu.EmulatorActivity` keep working.
- Data folder is separate: `Android/data/com.github.eka2l1.fe`. Install your devices and games again, or point
  the emulator directory setting to a shared folder.

## Using it with ES-DE / IISU

Point the launch entry to package `com.github.eka2l1.fe` and activity `com.github.eka2l1.emu.EmulatorActivity`
(or the short form `.emu.EmulatorActivity`). Existing launch `.json` files work as they are.

## Building

See [BUILDING.md](BUILDING.md). Requirements for the Android app: JDK 17, Android SDK platform 34,
build-tools 34.0.0, NDK 25.1.8937393, CMake 3.22.1.

To sign a release build, create `src/emu/android/keystore.properties` (it is git-ignored):

```
storeFile=/absolute/path/to/your.keystore
storePassword=...
keyAlias=...
keyPassword=...
```

then run, from `src/emu/android`:

```
./gradlew assembleRelease
```

The APK is written to `app/build/outputs/apk/release/`. Do not pass `-Pandroid.injected.build.abi=...` for
release builds you plan to distribute: Gradle marks those APKs as `testOnly`.

## License

EKA2L1 is licensed under the GNU GPL v3 (see [LICENSE](LICENSE)). This fork keeps the same license.
The full list of changes is available in the git history of this repository.

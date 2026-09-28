#!/usr/bin/env bash
# Crea la rama "fe" con dos commits limpios sobre el commit base de EKA2L1:
#   1) fix: lanzar juegos desde .json con otro dispositivo activo (candidato a PR para upstream)
#   2) fork: identidad (applicationId, nombre, alias), firma de release, README/FORK.md
# Ejecutar dentro de tu carpeta del repo (donde esta la carpeta .git).
set -euo pipefail

BASE_COMMIT="0d1831aa8444fea52958e6f98be4dc4f8463540c"
BRANCH="${1:-fe}"

git rev-parse --show-toplevel >/dev/null 2>&1 || { echo "Ejecuta esto dentro del repo." >&2; exit 1; }
cd "$(git rev-parse --show-toplevel)"

if [ -z "$(git config user.name)" ] || [ -z "$(git config user.email)" ]; then
  echo "Configura tu identidad de git antes:" >&2
  echo '  git config user.name "Tu Nombre"' >&2
  echo '  git config user.email "tu@correo"' >&2
  exit 1
fi

git cat-file -e "$BASE_COMMIT^{commit}" 2>/dev/null || { echo "No existe el commit base $BASE_COMMIT en este repo." >&2; exit 1; }
git show-ref --verify --quiet "refs/heads/$BRANCH" && { echo "La rama '$BRANCH' ya existe. Usa otro nombre: $0 otra-rama" >&2; exit 1; }

if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
  echo "==> Guardando cambios sin commit en un stash (por seguridad)..."
  git stash push -m "antes-de-$BRANCH"
fi

git checkout -q -b "$BRANCH" "$BASE_COMMIT"

P1="$(mktemp)"; P2="$(mktemp)"
cat > "$P1" <<'PATCH1_EOF'
diff --git a/src/emu/android/app/src/main/cpp/include/android/launcher.h b/src/emu/android/app/src/main/cpp/include/android/launcher.h
index 3af6e32..ca4398f 100644
--- a/src/emu/android/app/src/main/cpp/include/android/launcher.h
+++ b/src/emu/android/app/src/main/cpp/include/android/launcher.h
@@ -72,15 +72,15 @@ namespace eka2l1::android {
         std::vector<std::uint8_t> screenshot_buffer_;
 
         void set_language_to_property(const language new_one);
-        void set_language_current(const language lang);
         void retrieve_servers();
 
     public:
         explicit launcher(eka2l1::system *sys);
+        void set_language_current(const language lang);
 
         std::vector<std::string> get_apps();
         jobjectArray get_app_icon(JNIEnv *env, std::uint32_t uid);
-        void launch_app(std::uint32_t uid);
+        bool launch_app(std::uint32_t uid);
         package::installation_result install_app(std::string &path);
         std::vector<std::string> get_devices();
         std::vector<std::string> get_device_firwmare_codes();
diff --git a/src/emu/android/app/src/main/cpp/include/android/state.h b/src/emu/android/app/src/main/cpp/include/android/state.h
index 7438b31..ded8e69 100644
--- a/src/emu/android/app/src/main/cpp/include/android/state.h
+++ b/src/emu/android/app/src/main/cpp/include/android/state.h
@@ -77,6 +77,9 @@ namespace eka2l1::android {
 
         bool first_time;
 
+        // Firmware code of the device that should be used at boot (empty = use the one in config).
+        std::string boot_device_code;
+
         common::semaphore graphics_sema;
         common::semaphore pause_sema;
         common::semaphore pause_graphics_sema;
diff --git a/src/emu/android/app/src/main/cpp/src/launcher.cpp b/src/emu/android/app/src/main/cpp/src/launcher.cpp
index e33c3a7..d70792e 100644
--- a/src/emu/android/app/src/main/cpp/src/launcher.cpp
+++ b/src/emu/android/app/src/main/cpp/src/launcher.cpp
@@ -285,8 +285,17 @@ namespace eka2l1::android {
         return nullptr;
     }
 
-    void launcher::launch_app(std::uint32_t uid) {
+    bool launcher::launch_app(std::uint32_t uid) {
+        if (!alserv) {
+            LOG_ERROR(eka2l1::FRONTEND_UI, "Application list server is not available, can't launch app 0x{:X}", uid);
+            return false;
+        }
+
         apa_app_registry *reg = alserv->get_registration(uid);
+        if (!reg) {
+            LOG_ERROR(eka2l1::FRONTEND_UI, "App 0x{:X} is not registered in the current device (wrong device profile?)", uid);
+            return false;
+        }
 
         epoc::apa::command_line cmdline;
         cmdline.launch_cmd_ = epoc::apa::command_create;
@@ -301,6 +310,7 @@ namespace eka2l1::android {
         });
 
         kern->unlock();
+        return true;
     }
 
     package::installation_result launcher::install_app(std::string &path) {
diff --git a/src/emu/android/app/src/main/cpp/src/native-lib.cpp b/src/emu/android/app/src/main/cpp/src/native-lib.cpp
index 9d32039..d8af234 100644
--- a/src/emu/android/app/src/main/cpp/src/native-lib.cpp
+++ b/src/emu/android/app/src/main/cpp/src/native-lib.cpp
@@ -71,6 +71,19 @@ Java_com_github_eka2l1_emu_Emulator_setDirectory(
     eka2l1::common::set_current_directory(executable_directory);
 }
 
+static std::string s_pending_boot_device_code;
+
+extern "C" JNIEXPORT void JNICALL
+Java_com_github_eka2l1_emu_Emulator_setBootDeviceCode(JNIEnv *env, jclass clazz, jstring code) {
+    if (!code) {
+        s_pending_boot_device_code.clear();
+        return;
+    }
+    const char *cstr = env->GetStringUTFChars(code, nullptr);
+    s_pending_boot_device_code = cstr;
+    env->ReleaseStringUTFChars(code, cstr);
+}
+
 extern "C" JNIEXPORT jboolean JNICALL
 Java_com_github_eka2l1_emu_Emulator_startNative(
     JNIEnv *env,
@@ -80,6 +93,7 @@ Java_com_github_eka2l1_emu_Emulator_startNative(
     eka2l1::drivers::android::register_camera_callbacks(env);
 
     state = std::make_unique<eka2l1::android::emulator>();
+    state->boot_device_code = s_pending_boot_device_code;
     return emulator_entry(*state);
 }
 
@@ -111,10 +125,10 @@ static void redraw_screens_immediately() {
     state->graphics_driver->submit_command_list(retrieved);
 }
 
-extern "C" JNIEXPORT void JNICALL
+extern "C" JNIEXPORT jboolean JNICALL
 Java_com_github_eka2l1_emu_Emulator_launchApp(JNIEnv *env, jclass clazz, jint uid) {
     // Launch the real app...
-    state->launcher->launch_app(uid);
+    return state->launcher->launch_app(uid) ? JNI_TRUE : JNI_FALSE;
 }
 
 extern "C" JNIEXPORT void JNICALL
diff --git a/src/emu/android/app/src/main/cpp/src/state.cpp b/src/emu/android/app/src/main/cpp/src/state.cpp
index 727491e..05148f8 100644
--- a/src/emu/android/app/src/main/cpp/src/state.cpp
+++ b/src/emu/android/app/src/main/cpp/src/state.cpp
@@ -17,6 +17,8 @@
  * along with this program. If not, see <http://www.gnu.org/licenses/>.
  */
 
+#include <algorithm>
+
 #include <android/input_dialog.h>
 #include <android/state.h>
 #include <common/algorithm.h>
@@ -129,6 +131,23 @@ namespace eka2l1::android {
         device_manager *dvcmngr = symsys->get_device_manager();
 
         if (dvcmngr->total() > 0) {
+            // A frontend launch (.json / shortcut) may request a specific device. Select it now,
+            // before the first set_device, so no hot-swap is needed later. Not saved to config.
+            if (!boot_device_code.empty()) {
+                const auto &dvcs = dvcmngr->get_devices();
+                bool found = false;
+                for (std::size_t i = 0; i < dvcs.size(); i++) {
+                    if (common::compare_ignore_case(dvcs[i].firmware_code.c_str(), boot_device_code.c_str()) == 0) {
+                        conf.device = static_cast<int>(i);
+                        found = true;
+                        break;
+                    }
+                }
+                if (!found) {
+                    LOG_WARN(FRONTEND_CMDLINE, "Requested boot device {} is not installed, using device index {}", boot_device_code, conf.device);
+                }
+            }
+
             symsys->startup();
 
             if (!symsys->set_device(conf.device)) {
@@ -154,6 +173,17 @@ namespace eka2l1::android {
         launcher = std::make_unique<eka2l1::android::launcher>(symsys.get());
         eka2l1::drivers::ui::launcher_instance = launcher.get();
 
+        if (!boot_device_code.empty() && dvcmngr->total() > 0) {
+            // Same language fallback that launcher::set_current_device does
+            const auto &dvcs = dvcmngr->get_devices();
+            if (static_cast<std::size_t>(conf.device) < dvcs.size()) {
+                const auto &langs = dvcs[conf.device].languages;
+                if (std::find(langs.begin(), langs.end(), conf.language) == langs.end()) {
+                    launcher->set_language_current(static_cast<language>(dvcs[conf.device].default_language_code));
+                }
+            }
+        }
+
         stage_two_inited = false;
     }
 
diff --git a/src/emu/android/app/src/main/java/com/github/eka2l1/emu/Emulator.java b/src/emu/android/app/src/main/java/com/github/eka2l1/emu/Emulator.java
index 2071655..7f52477 100644
--- a/src/emu/android/app/src/main/java/com/github/eka2l1/emu/Emulator.java
+++ b/src/emu/android/app/src/main/java/com/github/eka2l1/emu/Emulator.java
@@ -738,7 +738,10 @@ public class Emulator {
 
     private static native String[] getApps();
 
-    public static native void launchApp(int uid);
+    public static native boolean launchApp(int uid);
+
+    /** Must be called before the native side starts (startNative). Selects the device by firmware code. */
+    public static native void setBootDeviceCode(String code);
 
     public static native void surfaceChanged(Surface surface, int width, int height);
 
diff --git a/src/emu/android/app/src/main/java/com/github/eka2l1/emu/EmulatorActivity.java b/src/emu/android/app/src/main/java/com/github/eka2l1/emu/EmulatorActivity.java
index d2075f3..961ddbf 100644
--- a/src/emu/android/app/src/main/java/com/github/eka2l1/emu/EmulatorActivity.java
+++ b/src/emu/android/app/src/main/java/com/github/eka2l1/emu/EmulatorActivity.java
@@ -121,43 +121,51 @@ public class EmulatorActivity extends AppCompatActivity {
 
         boolean launchFromFile = intent.getData() != null;
 
-        if (externalIntent) {
-            Emulator.initializeForShortcutLaunch(this);
-        }
-
-        AppDataStore dataStore = AppDataStore.getAndroidStore();
-        setTheme(dataStore.getString(PREF_THEME, "dark"));
-        super.onCreate(savedInstanceState);
-        setContentView(R.layout.activity_emulator);
-        overlayView = findViewById(R.id.overlay);
-
         String name;
         String deviceCode;
 
+        // Read the launch info first: the device must be known before the native side boots.
         if (intent.getData() != null) {
-            InputStream inputStream = null;
-
-            try {
-                inputStream = getContentResolver().openInputStream(intent.getData());
-            } catch (FileNotFoundException e) {
-                throw new RuntimeException(e);
+            AppLaunchInfo launchInfo = null;
+            try (InputStream inputStream = getContentResolver().openInputStream(intent.getData())) {
+                launchInfo = gson.fromJson(new InputStreamReader(inputStream), AppLaunchInfo.class);
+            } catch (Exception e) {
+                e.printStackTrace();
             }
 
-            AppLaunchInfo launchInfo = gson.fromJson(new InputStreamReader(inputStream), AppLaunchInfo.class);
+            if (launchInfo == null || launchInfo.appName == null || launchInfo.appUid == 0) {
+                super.onCreate(savedInstanceState);
+                showLaunchError(getString(R.string.error));
+                return;
+            }
 
             uid = launchInfo.appUid;
             name = launchInfo.appName;
             deviceCode = launchInfo.deviceCode;
-
-            if (name == null || uid == 0) {
-                throw new RuntimeException("Invalid launch info");
-            }
         } else {
             uid = intent.getLongExtra(KEY_APP_UID, -1);
             name = intent.getStringExtra(KEY_APP_NAME);
             deviceCode = intent.getStringExtra(KEY_DEVICE_CODE);
         }
 
+        if (externalIntent) {
+            // Only has effect if the native side has not started yet in this process.
+            Emulator.setBootDeviceCode(deviceCode);
+            Emulator.initializeForShortcutLaunch(this);
+
+            if (!applyLaunchDevice(deviceCode, name)) {
+                // Process is being restarted with the right device
+                super.onCreate(savedInstanceState);
+                return;
+            }
+        }
+
+        AppDataStore dataStore = AppDataStore.getAndroidStore();
+        setTheme(dataStore.getString(PREF_THEME, "dark"));
+        super.onCreate(savedInstanceState);
+        setContentView(R.layout.activity_emulator);
+        overlayView = findViewById(R.id.overlay);
+
         String uidStr = Long.toHexString(uid).toUpperCase();
         File configDir = new File(Emulator.getConfigsDir(), uidStr);
         String defProfile = dataStore.getString(PREF_DEFAULT_PROFILE, null);
@@ -209,16 +217,6 @@ public class EmulatorActivity extends AppCompatActivity {
         EmulatorCamera.setActivity(this);
         EmulatorLocation.setActivity(this);
 
-        if (deviceCode != null) {
-            String []availableDevices = Emulator.getDeviceFirmwareCodes();
-            for (int id = 0; id < availableDevices.length; id++) {
-                if (availableDevices[id].compareToIgnoreCase(deviceCode) == 0) {
-                    Emulator.setCurrentDevice(id, true);
-                    break;
-                }
-            }
-        }
-
         setActionBar(name);
         hideSystemUI();
 
@@ -253,6 +251,55 @@ public class EmulatorActivity extends AppCompatActivity {
                 params.screenBackgroundImageKeepAspectRatio);
     }
 
+    private void showLaunchError(String message) {
+        Toast.makeText(this, message, Toast.LENGTH_LONG).show();
+        finish();
+    }
+
+    /**
+     * Makes sure the device requested by the launch info is the one running.
+     * @return true if it is fine to continue; false if the process is being restarted.
+     */
+    private boolean applyLaunchDevice(String deviceCode, String name) {
+        if (deviceCode == null || deviceCode.isEmpty()) {
+            return true;
+        }
+
+        String[] codes = Emulator.getDeviceFirmwareCodes();
+        int wanted = -1;
+        for (int i = 0; i < codes.length; i++) {
+            if (codes[i].compareToIgnoreCase(deviceCode) == 0) {
+                wanted = i;
+                break;
+            }
+        }
+
+        if (wanted < 0) {
+            Toast.makeText(this, "Device " + deviceCode + " is not installed. Using the current device.",
+                    Toast.LENGTH_LONG).show();
+            return true;
+        }
+
+        if (Emulator.getCurrentDevice() == wanted) {
+            return true;
+        }
+
+        // The process was already alive with another device: save the new one and cold restart,
+        // same as the Devices menu does.
+        Emulator.setCurrentDevice(wanted, false);
+
+        Intent restart = new Intent(this, EmulatorActivity.class);
+        restart.putExtra(KEY_APP_IS_SHORTCUT, true);
+        restart.putExtra(KEY_APP_UID, uid);
+        restart.putExtra(KEY_APP_NAME, name);
+        restart.putExtra(KEY_DEVICE_CODE, deviceCode);
+        restart.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
+        startActivity(restart);
+        finish();
+        Runtime.getRuntime().exit(0);
+        return false;
+    }
+
     @Override
     public void onWindowFocusChanged(boolean hasFocus) {
         super.onWindowFocusChanged(hasFocus);
@@ -543,8 +590,10 @@ public class EmulatorActivity extends AppCompatActivity {
 
             Emulator.surfaceChanged(holder.getSurface(), width, height);
             if (!launched) {
-                Emulator.launchApp((int) uid);
                 launched = true;
+                if (!Emulator.launchApp((int) uid)) {
+                    showLaunchError(getString(R.string.error));
+                }
             }
         }
 
PATCH1_EOF
cat > "$P2" <<'PATCH2_EOF'
diff --git a/FORK.md b/FORK.md
new file mode 100644
index 0000000..cd201d9
--- /dev/null
+++ b/FORK.md
@@ -0,0 +1,66 @@
+# EKA2L1 FE (fork of EKA2L1)
+
+This is an unofficial fork of [EKA2L1](https://github.com/EKA2L1/EKA2L1), the Symbian OS / N-Gage emulator.
+It is **not** affiliated with or endorsed by the EKA2L1 team. All credit for the emulator goes to the
+upstream authors and contributors. This fork only changes the Android app, as described below.
+
+## What is different
+
+### Launching games from frontends (ES-DE, IISU) now works with any active device
+
+Upstream, a launch `.json` stores the `DeviceCode` of the device the game needs (for example an S60v3
+phone for one game and an S60v5 phone for another). When the launch was made while a different device was
+active, the app tried to hot-swap the device before the surface existed. That code path is barely exercised
+on Android and the game would hang or crash.
+
+In this fork:
+
+- The device requested by the `.json` is selected **at boot** (`Emulator.setBootDeviceCode`, applied in
+  `emulator::stage_one()` before the first `set_device`). There is no hot swap anymore.
+- If the process is already running with another device, the app saves the requested device and restarts
+  the process to relaunch the game, the same way the Devices menu does.
+- If the requested device is not installed, you get a message and the current device is used.
+- `launcher::launch_app` no longer dereferences a null registration. A wrong device or unknown app shows an
+  error instead of crashing the app.
+
+### Fork identity (so it can be installed next to the official app)
+
+- `applicationId`: `com.github.eka2l1.fe` (the Java namespace is unchanged, the JNI depends on it).
+- App name: **EKA2L1 FE**.
+- An `activity-alias` named `com.github.eka2l1.fe.emu.EmulatorActivity` so launchers that use the short
+  component name `.emu.EmulatorActivity` keep working.
+- Data folder is separate: `Android/data/com.github.eka2l1.fe`. Install your devices and games again, or point
+  the emulator directory setting to a shared folder.
+
+## Using it with ES-DE / IISU
+
+Point the launch entry to package `com.github.eka2l1.fe` and activity `com.github.eka2l1.emu.EmulatorActivity`
+(or the short form `.emu.EmulatorActivity`). Existing launch `.json` files work as they are.
+
+## Building
+
+See [BUILDING.md](BUILDING.md). Requirements for the Android app: JDK 17, Android SDK platform 34,
+build-tools 34.0.0, NDK 25.1.8937393, CMake 3.22.1.
+
+To sign a release build, create `src/emu/android/keystore.properties` (it is git-ignored):
+
+```
+storeFile=/absolute/path/to/your.keystore
+storePassword=...
+keyAlias=...
+keyPassword=...
+```
+
+then run, from `src/emu/android`:
+
+```
+./gradlew assembleRelease
+```
+
+The APK is written to `app/build/outputs/apk/release/`. Do not pass `-Pandroid.injected.build.abi=...` for
+release builds you plan to distribute: Gradle marks those APKs as `testOnly`.
+
+## License
+
+EKA2L1 is licensed under the GNU GPL v3 (see [LICENSE](LICENSE)). This fork keeps the same license.
+The full list of changes is available in the git history of this repository.
diff --git a/README.md b/README.md
index 98aeb54..52d746e 100644
--- a/README.md
+++ b/README.md
@@ -1,3 +1,9 @@
+> **This is EKA2L1 FE, an unofficial fork of [EKA2L1](https://github.com/EKA2L1/EKA2L1).**
+> It fixes launching games from frontends such as ES-DE and IISU when a different device profile is active.
+> See [FORK.md](FORK.md) for the changes. The rest of this README is the upstream one.
+
+---
+
 <div class="header">
   <p align="center">
      <img src="https://i.imgur.com/FasrbKV.png" width="256">
diff --git a/src/emu/android/.gitignore b/src/emu/android/.gitignore
index 41a8929..ce1b1ff 100644
--- a/src/emu/android/.gitignore
+++ b/src/emu/android/.gitignore
@@ -10,3 +10,8 @@
 .cxx
 *.dll
 *.map
+
+# Release signing (never commit these)
+/keystore.properties
+*.keystore
+*.jks
diff --git a/src/emu/android/app/build.gradle b/src/emu/android/app/build.gradle
index c8b2322..af70878 100644
--- a/src/emu/android/app/build.gradle
+++ b/src/emu/android/app/build.gradle
@@ -9,6 +9,12 @@ def getGitHash = { ->
     return stdout.toString().trim()
 }
 
+def keystoreProps = new Properties()
+def keystorePropsFile = rootProject.file("keystore.properties")
+if (keystorePropsFile.exists()) {
+    keystorePropsFile.withInputStream { keystoreProps.load(it) }
+}
+
 def ciArg = '-DCI=OFF'
 if (project.hasProperty('ciArg')) {
     ciArg = project.property('ciArg').toString()
@@ -19,11 +25,11 @@ android {
     namespace 'com.github.eka2l1'
 
     defaultConfig {
-        applicationId "com.github.eka2l1"
+        applicationId "com.github.eka2l1.fe"
         minSdkVersion 21
         targetSdkVersion 34
-        versionCode 12
-        versionName "0.1.0"
+        versionCode 13
+        versionName "0.1.0-fe.1"
         ndkVersion = '25.1.8937393'
         buildConfigField "String", "GIT_HASH", "\"${getGitHash()}\""
         ndk {
@@ -40,8 +46,22 @@ android {
         }
     }
 
+    signingConfigs {
+        release {
+            if (keystorePropsFile.exists()) {
+                storeFile file(keystoreProps['storeFile'])
+                storePassword keystoreProps['storePassword']
+                keyAlias keystoreProps['keyAlias']
+                keyPassword keystoreProps['keyPassword']
+            }
+        }
+    }
+
     buildTypes {
         release {
+            if (keystorePropsFile.exists()) {
+                signingConfig signingConfigs.release
+            }
             minifyEnabled true
             proguardFiles getDefaultProguardFile('proguard-android-optimize.txt'), 'proguard-rules.pro'
             externalNativeBuild {
diff --git a/src/emu/android/app/src/main/AndroidManifest.xml b/src/emu/android/app/src/main/AndroidManifest.xml
index f2b88d2..4fdabe8 100644
--- a/src/emu/android/app/src/main/AndroidManifest.xml
+++ b/src/emu/android/app/src/main/AndroidManifest.xml
@@ -61,6 +61,13 @@
                 <data android:pathPattern=".*.json" />
             </intent-filter>
         </activity>
+        <!-- The applicationId differs from the Java namespace in this fork. Launchers such as
+             ES-DE / IISU use the short name ".emu.EmulatorActivity", which Android expands
+             using the applicationId. This alias makes that name resolve. -->
+        <activity-alias
+            android:name="com.github.eka2l1.fe.emu.EmulatorActivity"
+            android:exported="true"
+            android:targetActivity="com.github.eka2l1.emu.EmulatorActivity" />
         <provider
             android:name="androidx.core.content.FileProvider"
             android:authorities="${applicationId}.provider"
diff --git a/src/emu/android/app/src/main/res/values/strings.xml b/src/emu/android/app/src/main/res/values/strings.xml
index 144c395..40d36fc 100644
--- a/src/emu/android/app/src/main/res/values/strings.xml
+++ b/src/emu/android/app/src/main/res/values/strings.xml
@@ -1,5 +1,5 @@
 <resources>
-    <string name="app_name" translatable="false">EKA2L1</string>
+    <string name="app_name" translatable="false">EKA2L1 FE</string>
     <string name="error">Error</string>
     <string name="no_data">No data</string>
     <string name="search">Search</string>
PATCH2_EOF

git apply --check "$P1"
git apply "$P1"
git add -A
git commit -q -m "Android: boot with the device from the launch .json, fix null deref in launch_app

Launching a game from a frontend (ES-DE, IISU) through a .json while a different
device profile was active hot-swapped the device before the surface existed, which
hung or crashed. The requested device is now selected at boot (stage_one), the
process is restarted when it is already running with another device, unknown
devices produce a message, and launch_app validates the registration (no SIGSEGV)."

git apply --check "$P2"
git apply "$P2"
git add -A
git commit -q -m "Fork identity: EKA2L1 FE (applicationId com.github.eka2l1.fe), release signing, docs

- applicationId com.github.eka2l1.fe, app name EKA2L1 FE, version 0.1.0-fe.1
- activity-alias so launchers using .emu.EmulatorActivity keep working
- optional release signing through keystore.properties (git-ignored)
- FORK.md and README notice"

rm -f "$P1" "$P2"
echo
git log --oneline -3
echo "Listo: rama '$BRANCH'."

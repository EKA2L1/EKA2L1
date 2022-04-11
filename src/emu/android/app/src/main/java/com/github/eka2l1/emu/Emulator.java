/*
 * Copyright (c) 2020 EKA2L1 Team
 * Copyright (c) 2012- PPSSPP Project
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

package com.github.eka2l1.emu;

import static com.github.eka2l1.emu.Constants.PREF_VIBRATION;

import android.annotation.SuppressLint;
import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.os.Vibrator;
import android.provider.DocumentsContract;
import android.view.Surface;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.LinearLayout;

import androidx.annotation.RequiresApi;
import androidx.appcompat.app.AlertDialog;
import androidx.documentfile.provider.DocumentFile;

import com.github.eka2l1.BuildConfig;
import com.github.eka2l1.R;
import com.github.eka2l1.applist.AppItem;
import com.github.eka2l1.settings.AppDataStore;
import com.github.eka2l1.util.FileUtils;
import com.github.eka2l1.util.BitmapUtils;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;

import io.reactivex.Completable;
import io.reactivex.Single;

public class Emulator {
    public static final String APP_CONFIG_FILE = "/config.json";
    public static final String APP_KEY_LAYOUT_FILE = "/VirtualKeyboardLayout";

    public static final int INSTALL_DEVICE_ERROR_NONE = 0;
    public static final int INSTALL_DEVICE_ERROR_NOT_EXIST = 1;
    public static final int INSTALL_DEVICE_ERROR_INSUFFICENT = 2;
    public static final int INSTALL_DEVICE_ERROR_RPKG_CORRUPT = 3;
    public static final int INSTALL_DEVICE_ERROR_DETERMINE_PRODUCT_FAIL = 4;
    public static final int INSTALL_DEVICE_ERROR_ALREADY_EXIST = 5;
    public static final int INSTALL_DEVICE_ERROR_GENERAL_FAILURE = 6;
    public static final int INSTALL_DEVICE_ERROR_ROM_FAIL_TO_COPY = 7;
    public static final int INSTALL_DEVICE_ERROR_VPL_FILE_INVALID = 8;
    public static final int INSTALL_DEVICE_ERROR_ROFS_CORRUPTED = 9;
    public static final int INSTALL_DEVICE_ERROR_ROM_CORRUPTED = 10;
    public static final int INSTALL_DEVICE_ERROR_FPSX_CORRUPTED = 11;

    private static final int STORAGE_ERROR_SUCCESS = 0;
    private static final int STORAGE_ERROR_UNKNOWN = -1;
    private static final int STORAGE_ERROR_NOT_FOUND = -2;
    private static final int STORAGE_ERROR_DISK_FULL = -3;
    private static final int STORAGE_ERROR_ALREADY_EXISTS = -4;

    private static final String[] columns = new String[] {
        DocumentsContract.Document.COLUMN_DISPLAY_NAME,
        DocumentsContract.Document.COLUMN_SIZE,
        DocumentsContract.Document.COLUMN_FLAGS,
        DocumentsContract.Document.COLUMN_MIME_TYPE,  // check for MIME_TYPE_DIR
        DocumentsContract.Document.COLUMN_LAST_MODIFIED
    };

    @SuppressLint("StaticFieldLeak")
    private static Context context;
    private static boolean vibrationEnabled;
    private static Vibrator vibrator;
    private static AlertDialog inputDialog;

    private static String emulatorDir;
    private static String compatDir;
    private static String configsDir;
    private static String profilesDir;
    private static String scriptsDir;
    private static boolean init;
    private static boolean load;

    static {
        System.loadLibrary("native-lib");
    }

    public static void initializePath(Context context) {
        Emulator.context = context;
        vibrationEnabled = AppDataStore.getAndroidStore().getBoolean(PREF_VIBRATION, true);

        if (FileUtils.isExternalStorageLegacy()) {
            emulatorDir = Environment.getExternalStorageDirectory() + "/EKA2L1/";
        } else {
            emulatorDir = context.getExternalFilesDir(null).getPath() + "/";
        }
        compatDir = emulatorDir + "compat/";
        configsDir = emulatorDir + "android/configs/";
        profilesDir = emulatorDir + "android/profiles/";
        scriptsDir = emulatorDir + "scripts/";
    }

    public static void initializeFolders() {
        File folder = new File(emulatorDir);
        if (!folder.exists()) {
            folder.mkdirs();
        }
        File nomedia = new File(emulatorDir, ".nomedia");
        if (!nomedia.exists()) {
            try {
                nomedia.createNewFile();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        File configsFolder = new File(configsDir);
        if (!configsFolder.exists()) {
            configsFolder.mkdirs();
        }
        File profilesFolder = new File(profilesDir);
        if (!profilesFolder.exists()) {
            profilesFolder.mkdirs();
        }
        File scriptsFolder = new File(scriptsDir);
        if (!scriptsFolder.exists()) {
            scriptsFolder.mkdirs();
        }

        boolean shouldUpdate = checkUpdate();
        updateFolder("resources", shouldUpdate);
        updateFolder("patch", shouldUpdate);
        copyFolder("compat", shouldUpdate);
        copyFolder("scripts", shouldUpdate);

        setDirectory(emulatorDir);
    }

    private static boolean checkUpdate() {
        boolean result = false;
        File versionFile = new File(emulatorDir, "version");
        String version = FileUtils.readFromFile(versionFile);
        if (!version.equals(BuildConfig.GIT_HASH)) {
            result = true;
            FileUtils.writeToFile(BuildConfig.GIT_HASH, versionFile);
        }
        return result;
    }

    private static void updateFolder(String folderName, boolean shouldUpdate) {
        File patchFolder = new File(emulatorDir, folderName);
        if (shouldUpdate || !patchFolder.exists()) {
            if (patchFolder.exists()) {
                FileUtils.deleteDirectory(patchFolder);
            }
            FileUtils.copyAssetFolder(context, folderName, emulatorDir + folderName);
        }
    }

    private static void copyFolder(String folderName, boolean shouldUpdate) {
        File patchFolder = new File(emulatorDir, folderName);
        if (shouldUpdate || !patchFolder.exists()) {
            FileUtils.copyAssetFolder(context, folderName, emulatorDir + folderName);
        }
    }

    public static String getEmulatorDir() {
        return emulatorDir;
    }

    public static String getCompatDir() {
        return compatDir;
    }

    public static String getProfilesDir() {
        return profilesDir;
    }

    public static String getConfigsDir() {
        return configsDir;
    }

    public static Single<ArrayList<AppItem>> getAppsList() {
        return Single.create(emitter -> {
            checkInit();
            String[] apps = getApps();
            ArrayList<AppItem> items = new ArrayList<>();
            for (int i = 0; i < apps.length; i += 2) {
                long appUid = Long.parseLong(apps[i]);
                Bitmap[] iconAndMask = getAppIcon(appUid);

                Bitmap finalIcon = null;
                if (iconAndMask != null) {
                    if ((iconAndMask.length == 1) || (iconAndMask[1] == null)) {
                        finalIcon = iconAndMask[0];
                    } else {
                        finalIcon = BitmapUtils.SourceWithMergedSymbianMask(iconAndMask[0], iconAndMask[1]);

                        iconAndMask[0].recycle();
                        iconAndMask[1].recycle();
                    }
                }

                AppItem item = new AppItem(appUid, 0, apps[i + 1], finalIcon);
                items.add(item);
            }
            emitter.onSuccess(items);
        });
    }

    public static ArrayList<AppItem> getPackagesList() {
        String[] packages = getPackages();
        ArrayList<AppItem> items = new ArrayList<>();
        for (int i = 0; i < packages.length; i += 3) {
            AppItem item = new AppItem(Long.parseLong(packages[i]), Long.parseLong(packages[i + 1]), packages[i + 2], null);
            items.add(item);
        }
        return items;
    }

    public static Completable subscribeInstallDevice(String rpkgPath, String romPath, boolean installRPKG) {
        return Completable.create(emitter -> {
            int installResult = installDevice(rpkgPath, romPath, installRPKG);

            if (installResult == 0) {
                emitter.onComplete();
            } else {
                emitter.onError(new IOException(Integer.toString(installResult)));
            }
        });
    }

    public static Completable subscribeInstallApp(String path) {
        return Completable.create(emitter -> {
            int installResult = installApp(path);

            if (installResult == 0) {
                emitter.onComplete();
            } else {
                emitter.onError(new IOException(Integer.toString(installResult)));
            }
        });
    }

    private static void checkInit() {
        if (!init && load) {
            throw new IllegalStateException("Emulator is not initialized");
        }
        if (!init) {
            init = startNative();
            load = true;
            if (!init) {
                throw new IllegalStateException("Emulator is not initialized");
            }
        }
    }

    public static void initializeForShortcutLaunch(Context context) {
        initializePath(context);
        initializeFolders();

        checkInit();
    }

    public static void setContext(Context context) {
        Emulator.context = context;
    }

    public static void setVibration(boolean vibrationEnabled) {
        Emulator.vibrationEnabled = vibrationEnabled;
    }

    @SuppressLint("unused")
    public static boolean vibrate(int duration) {
        if (!vibrationEnabled) {
            return false;
        }
        if (vibrator == null) {
            vibrator = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
        }
        if (vibrator == null || !vibrator.hasVibrator()) {
            return false;
        }
        vibrator.vibrate(duration);
        return true;
    }

    @SuppressLint("unused")
    public static void stopVibrate() {
        if (vibrator != null) {
            vibrator.cancel();
        }
    }

    @SuppressLint("unused")
    public static ClassLoader getAppClassLoader() {
        return context.getClassLoader();
    }

    public static int openContentUri(String uriString, String mode) {
        try {
            Uri uri = Uri.parse(uriString);
            ParcelFileDescriptor filePfd = context.getContentResolver().openFileDescriptor(uri, mode);
            if (filePfd == null) {
                return -1;
            }
            return filePfd.detachFd();
        } catch (Exception e) {
            e.printStackTrace();
            return -1;
        }
    }

    public static int contentUriCreateDirectory(String rootTreeUri, String dirName) {
        try {
            Uri uri = Uri.parse(rootTreeUri);
            DocumentFile documentFile = DocumentFile.fromTreeUri(context, uri);
            if (documentFile != null) {
                DocumentFile createdDir = documentFile.createDirectory(dirName);
                return createdDir != null ? STORAGE_ERROR_SUCCESS : STORAGE_ERROR_UNKNOWN;
            } else {
                return STORAGE_ERROR_UNKNOWN;
            }
        } catch (Exception e) {
            e.printStackTrace();
            return STORAGE_ERROR_UNKNOWN;
        }
    }

    public static int contentUriCreateFile(String rootTreeUri, String fileName) {
        try {
            Uri uri = Uri.parse(rootTreeUri);
            DocumentFile documentFile = DocumentFile.fromTreeUri(context, uri);
            if (documentFile != null) {
                // TODO: Check the file extension and choose MIME type appropriately.
                DocumentFile createdFile = documentFile.createFile("application/octet-stream", fileName);
                return createdFile != null ? STORAGE_ERROR_SUCCESS : STORAGE_ERROR_UNKNOWN;
            } else {
                return STORAGE_ERROR_UNKNOWN;
            }
        } catch (Exception e) {
            e.printStackTrace();
            return STORAGE_ERROR_UNKNOWN;
        }
    }

    // NOTE: The destination is the parent directory! This means that contentUriCopyFile
    // cannot rename things as part of the operation.
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static int contentUriMoveFile(String srcFileUri, String srcParentDirUri, String dstParentDirUri) {
        try {
            Uri srcUri = Uri.parse(srcFileUri);
            Uri srcParentUri = Uri.parse(srcParentDirUri);
            Uri dstParentUri = Uri.parse(dstParentDirUri);
            return DocumentsContract.moveDocument(context.getContentResolver(), srcUri,
                    srcParentUri, dstParentUri) != null ? STORAGE_ERROR_SUCCESS : STORAGE_ERROR_UNKNOWN;
        } catch (Exception e) {
            e.printStackTrace();
            return STORAGE_ERROR_UNKNOWN;
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.N)
    public static int contentUriCopyFile(String srcFileUri, String dstParentDirUri) {
        try {
            Uri srcUri = Uri.parse(srcFileUri);
            Uri dstParentUri = Uri.parse(dstParentDirUri);
            return DocumentsContract.copyDocument(context.getContentResolver(), srcUri,
                    dstParentUri) != null ? STORAGE_ERROR_SUCCESS : STORAGE_ERROR_UNKNOWN;
        } catch (Exception e) {
            e.printStackTrace();
            return STORAGE_ERROR_UNKNOWN;
        }
    }

    public static int contentUriRemoveFile(String fileName) {
        try {
            Uri uri = Uri.parse(fileName);
            DocumentFile documentFile = DocumentFile.fromSingleUri(context, uri);
            if (documentFile != null) {
                return documentFile.delete() ? STORAGE_ERROR_SUCCESS : STORAGE_ERROR_UNKNOWN;
            } else {
                return STORAGE_ERROR_UNKNOWN;
            }
        } catch (Exception e) {
            e.printStackTrace();
            return STORAGE_ERROR_UNKNOWN;
        }
    }

    public static int contentUriRenameFileTo(String fileUri, String newName) {
        try {
            Uri uri = Uri.parse(fileUri);
            // Due to a design flaw, we can't use DocumentFile.renameTo().
            // Instead we use the DocumentsContract API directly.
            // See https://stackoverflow.com/questions/37168200/android-5-0-new-sd-card-access-api-documentfile-renameto-unsupportedoperation.
            Uri newUri = DocumentsContract.renameDocument(context.getContentResolver(), uri, newName);
            return STORAGE_ERROR_SUCCESS;
        } catch (Exception e) {
            // TODO: More detailed exception processing.
            e.printStackTrace();
            return STORAGE_ERROR_UNKNOWN;
        }
    }

    private static String cursorToString(Cursor c) {
        final int flags = c.getInt(2);
        // Filter out any virtual or partial nonsense.
        // There's a bunch of potentially-interesting flags here btw,
        // to figure out how to set access flags better, etc.
        if ((flags & (DocumentsContract.Document.FLAG_PARTIAL |
                DocumentsContract.Document.FLAG_VIRTUAL_DOCUMENT)) != 0) {
            return null;
        }

        final String mimeType = c.getString(3);
        final boolean isDirectory = mimeType.equals(DocumentsContract.Document.MIME_TYPE_DIR);
        final String documentName = c.getString(0);
        final long size = c.getLong(1);
        final long lastModified = c.getLong(4);

        String str = "F|";
        if (isDirectory) {
            str = "D|";
        }
        return str + size + "|" + documentName + "|" + lastModified;
    }

    public static String contentUriGetFileInfo(String fileName) {
        Cursor c = null;
        try {
            Uri uri = Uri.parse(fileName);
            final ContentResolver resolver = context.getContentResolver();
            c = resolver.query(uri, columns, null, null, null);
            if (c.moveToNext()) {
                String str = cursorToString(c);
                return str;
            } else {
                return null;
            }
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        } finally {
            if (c != null) {
                c.close();
            }
        }
    }

    private static void closeQuietly(AutoCloseable closeable) {
        if (closeable != null) {
            try {
                closeable.close();
            } catch (RuntimeException rethrown) {
                throw rethrown;
            } catch (Exception ignored) {
            }
        }
    }

    public static boolean contentUriFileExists(String fileUri) {
        Cursor c = null;
        try {
            Uri uri = Uri.parse(fileUri);
            c = context.getContentResolver().query(uri, new String[]
                    { DocumentsContract.Document.COLUMN_DOCUMENT_ID },
                    null, null, null);
            return c.getCount() > 0;
        } catch (Exception e) {
            // Log.w(TAG, "Failed query: " + e);
            return false;
        } finally {
            closeQuietly(c);
        }
    }

    public static boolean isExternalStoragePreservedLegacy() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // In 29 and later, we can check whether we got preserved storage legacy.
            return Environment.isExternalStorageLegacy();
        } else {
            // In 28 and earlier, we won't call this - we'll still request an exception.
            return false;
        }
    }

    // TODO: Maybe add a cheaper version that doesn't extract all the file information?
    // TODO: Replace with a proper query:
    // * https://stackoverflow.com/questions/42186820/documentfile-is-very-slow
    public static String[] listContentUriDir(String uriString) {
        Cursor c = null;
        try {
            Uri uri = Uri.parse(uriString);
            final ContentResolver resolver = context.getContentResolver();
            final Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
                    uri, DocumentsContract.getDocumentId(uri));
            final ArrayList<String> listing = new ArrayList<>();
            c = resolver.query(childrenUri, columns, null, null, null);
            while (c.moveToNext()) {
                String str = cursorToString(c);
                if (str != null) {
                    listing.add(str);
                }
            }
            // Is ArrayList weird or what?
            String[] strings = new String[listing.size()];
            return listing.toArray(strings);
        }
        catch (IllegalArgumentException e) {
            // Due to sloppy exception handling in resolver.query, we get this wrapping
            // a FileNotFoundException if the directory doesn't exist.
            return new String[]{};
        }
        catch (Exception e) {
            e.printStackTrace();
            return new String[]{};
        } finally {
            if (c != null) {
                c.close();
            }
        }
    }

    public static void showInputDialog(String initialText, int maxLen) {
        new Handler(Looper.getMainLooper()).post(() -> {
            if (inputDialog == null) {
                EditText editText = new EditText(context);
                editText.setLines(3);
                editText.setText(initialText);
                float density = context.getResources().getDisplayMetrics().density;
                LinearLayout linearLayout = new LinearLayout(context);
                LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT);
                int margin = (int) (density * 20);
                params.setMargins(margin, 0, margin, 0);
                linearLayout.addView(editText, params);
                int paddingVertical = (int) (density * 16);
                int paddingHorizontal = (int) (density * 8);
                editText.setPadding(paddingHorizontal, paddingVertical, paddingHorizontal, paddingVertical);
                AlertDialog.Builder builder = new AlertDialog.Builder(context)
                        .setTitle(R.string.enter_text)
                        .setView(linearLayout)
                        .setPositiveButton(android.R.string.ok, (dialogInterface, i) -> {
                            Emulator.submitInput(editText.getText().toString());
                            inputDialog = null;
                        })
                        .setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> {
                            Emulator.submitInput("");
                            inputDialog = null;
                        });
                inputDialog = builder.create();
                inputDialog.show();
            }
        });
    }

    public static void closeInputDialog() {
        new Handler(Looper.getMainLooper()).post(() -> {
            if (inputDialog != null) {
                Emulator.submitInput("");
                inputDialog.dismiss();
                inputDialog = null;
            }
        });
    }

    public static boolean isInitialized() {
        return init;
    }

    private static native void setDirectory(String path);

    private static native boolean startNative();

    private static native String[] getApps();

    public static native void launchApp(int uid);

    public static native void surfaceChanged(Surface surface, int width, int height);

    public static native void surfaceDestroyed();

    public static native void surfaceRedrawNeeded();

    public static native void pressKey(int key, int keyState);

    public static native void touchScreen(int x, int y, int z, int action, int id);

    public static native int installApp(String path);

    public static native String[] getDevices();

    public static native String[] getDeviceFirmwareCodes();

    public static native void setCurrentDevice(int id, boolean isTemporary);

    public static native void setDeviceName(int id, String newName);

    public static native int getCurrentDevice();

    public static native int installDevice(String rpkgPath, String romPath, boolean installRPKG);

    public static native boolean doesRomNeedRPKG(String romPath);

    public static native String[] getPackages();

    public static native void uninstallPackage(int uid, int extIndex);

    public static native void mountSdCard(String path);

    public static native void loadConfig();

    public static native void setLanguage(int languageId);

    public static native void setRtosLevel(int level);

    public static native void updateAppSetting(int uid);

    public static native Bitmap[] getAppIcon(long uid);

    public static native String[] getLanguageIds();

    public static native String[] getLanguageNames();

    public static native void setScreenParams(int backgroundColor, int scaleRatio, int scaleType, int gravity);

    public static native boolean runTest(String testName);

    public static native void submitInput(String text);
}

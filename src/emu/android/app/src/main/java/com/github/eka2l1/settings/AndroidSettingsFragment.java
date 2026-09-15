/*
 * Copyright (c) 2020 EKA2L1 Team
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

package com.github.eka2l1.settings;

import static com.github.eka2l1.emu.Constants.KEY_RESTART;
import static com.github.eka2l1.emu.Constants.PREF_EMULATOR_DIR;
import static com.github.eka2l1.emu.Constants.PREF_LAUNCH_FILE_DIR;
import static com.github.eka2l1.emu.Constants.PREF_THEME;
import static com.github.eka2l1.emu.Constants.PREF_VIBRATION;

import android.annotation.SuppressLint;
import android.app.ProgressDialog;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.view.MenuItem;
import android.view.View;
import android.widget.Toast;
import android.content.Intent;

import androidx.activity.result.ActivityResultLauncher;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceManager;
import androidx.preference.SwitchPreferenceCompat;

import com.github.eka2l1.R;
import com.github.eka2l1.emu.Emulator;
import com.github.eka2l1.util.AppUtils;
import com.github.eka2l1.util.FileUtils;

import java.io.File;
import java.io.IOException;

import io.reactivex.Completable;
import io.reactivex.android.schedulers.AndroidSchedulers;
import io.reactivex.observers.DisposableCompletableObserver;
import io.reactivex.schedulers.Schedulers;

public class AndroidSettingsFragment extends PreferenceFragmentCompat {
    private AppDataStore dataStore;
    private LongClickPreference emulatorDirPreference;
    private Preference launchFileDirPreference;

    private final ActivityResultLauncher pickEmulatorDirectory = registerForActivityResult(
            FileUtils.getNativeDirPicker(),
            this::onEmulatorDirPickResult);

    private final ActivityResultLauncher pickLaunchFileDirectory = registerForActivityResult(
            FileUtils.getDirPicker(true),
            this::onLaunchFileDirPickResult);

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        dataStore = AppDataStore.getAndroidStore();
        PreferenceManager preferenceManager = getPreferenceManager();
        preferenceManager.setPreferenceDataStore(dataStore);
        setPreferencesFromResource(R.xml.preferences_android, rootKey);
        ListPreference themePreference = findPreference(PREF_THEME);
        themePreference.setOnPreferenceChangeListener((preference, newValue) -> {
            getParentFragmentManager().setFragmentResult(KEY_RESTART, new Bundle());
            return true;
        });
        SwitchPreferenceCompat vibrationPreference = findPreference(PREF_VIBRATION);
        vibrationPreference.setOnPreferenceChangeListener((preference, newValue) -> {
            Emulator.setVibration((Boolean) newValue);
            return true;
        });
        emulatorDirPreference = findPreference(PREF_EMULATOR_DIR);
        updateEmulatorDirSummary(Emulator.getEmulatorDir());

        emulatorDirPreference.setOnPreferenceClickListener(preference -> {
            if (FileUtils.isExternalStorageLegacy()) {
                pickEmulatorDirectory.launch(null);
            } else {
                openPicker();
            }

            return true;
        });

        emulatorDirPreference.setOnPreferenceLongClickListener(preference -> {
            requestWorkingDirectoryChange();
            return true;
        });

        String previousLaunchFileDir = dataStore.getString(PREF_LAUNCH_FILE_DIR, null);

        launchFileDirPreference = findPreference(PREF_LAUNCH_FILE_DIR);
        launchFileDirPreference.setSummary(previousLaunchFileDir);

        launchFileDirPreference.setOnPreferenceClickListener(preference -> {
            pickLaunchFileDirectory.launch(null);
            return true;
        });
    }

    private void openPicker() {
        try {
            startActivity(getFileManagerIntentOnDocumentProvider(Intent.ACTION_VIEW));
            return;
        } catch (ActivityNotFoundException ignored) {}

        try {
            startActivity(getFileManagerIntentOnDocumentProvider("android.provider.action.BROWSE"));
            return;
        } catch (ActivityNotFoundException ignored) {}

        try {
            // Just try to open the file manager, try the package name used on "normal" phones
            startActivity(getFileManagerIntent("com.google.android.documentsui"));
            return;
        } catch (ActivityNotFoundException ignored) {}

        try {
            // Next, try the AOSP package name
            startActivity(getFileManagerIntent("com.android.documentsui"));
        } catch (ActivityNotFoundException ignored) {}
    }

    private Intent getFileManagerIntent(String packageName) {
        // Fragile, but some phones don't expose the system file manager in any better way
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClassName(packageName, "com.android.documentsui.files.FilesActivity");
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    private Intent getFileManagerIntentOnDocumentProvider(String action) {
        String authority = requireContext().getPackageName() + ".documentProvider";
        String root = new File(Emulator.getEmulatorDir()).getAbsolutePath();
        Intent intent = new Intent(action);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.setData(DocumentsContract.buildRootUri(authority, root));
        intent.addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        return intent;
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        setHasOptionsMenu(true);
        ActionBar actionBar = ((AppCompatActivity) requireActivity()).getSupportActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);
        actionBar.setTitle(R.string.pref_android_title);
    }

    @Override
    public void onPause() {
        super.onPause();
        dataStore.save();
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            getParentFragmentManager().popBackStackImmediate();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void updateEmulatorDirSummary(String path) {
        emulatorDirPreference.setSummary(path + "\n" + getString(R.string.pref_emulator_dir_hint));
    }

    private void requestWorkingDirectoryChange() {
        if (FileUtils.hasDirectStorageAccess()) {
            pickEmulatorDirectory.launch(null);
            return;
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Toast.makeText(getContext(), R.string.pref_emulator_dir_cant_change, Toast.LENGTH_SHORT).show();
            return;
        }
        new AlertDialog.Builder(requireContext())
                .setTitle(R.string.pref_emulator_dir)
                .setMessage(R.string.pref_emulator_dir_needs_all_files_access)
                .setPositiveButton(android.R.string.ok, (d, w) -> openAllFilesAccessSettings())
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private void openAllFilesAccessSettings() {
        Uri packageUri = Uri.parse("package:" + requireContext().getPackageName());
        try {
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, packageUri));
            return;
        } catch (ActivityNotFoundException ignored) {
        }

        try {
            startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
        } catch (ActivityNotFoundException ignored) {
            Toast.makeText(getContext(), R.string.error, Toast.LENGTH_SHORT).show();
        }
    }

    private void onEmulatorDirPickResult(String newPath) {
        if (newPath == null) {
            return;
        }
        if (!FileUtils.isUsableWorkingDir(newPath)) {
            Toast.makeText(getContext(), R.string.pref_emulator_dir_not_writable, Toast.LENGTH_LONG).show();
            return;
        }

        String targetPath = FileUtils.ensureTrailingSeparator(newPath);
        String currentPath = FileUtils.ensureTrailingSeparator(Emulator.getEmulatorDir());

        if (targetPath.equals(currentPath)) {
            return;
        }
        if (targetPath.startsWith(currentPath)) {
            Toast.makeText(getContext(), R.string.pref_emulator_dir_inside_current, Toast.LENGTH_LONG).show();
            return;
        }

        String message = getString(R.string.pref_emulator_dir_move_confirm, targetPath);
        String[] targetContent = new File(targetPath).list();
        if (targetContent != null && targetContent.length != 0) {
            message = message + "\n\n" + getString(R.string.pref_emulator_dir_move_replaces);
        }

        new AlertDialog.Builder(requireContext())
                .setTitle(R.string.pref_emulator_dir)
                .setMessage(message)
                .setPositiveButton(android.R.string.ok, (d, w) -> moveWorkingDirectory(currentPath, targetPath))
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    @SuppressWarnings("ResultOfMethodCallIgnored")
    @SuppressLint("CheckResult")
    private void moveWorkingDirectory(String currentPath, String targetPath) {
        ProgressDialog dialog = new ProgressDialog(getActivity());
        dialog.setIndeterminate(true);
        dialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
        dialog.setCancelable(false);
        dialog.setMessage(getText(R.string.processing));
        dialog.show();

        File source = new File(currentPath);
        File target = new File(targetPath);

        // The settings file stays in the persistent directory: it is what names the
        // working directory in the first place.
        String[] excluded = currentPath.equals(FileUtils.ensureTrailingSeparator(Emulator.getPersistentDataDir()))
                ? new String[] { AppDataStore.ANDROID_STORE_FILE_NAME } : new String[0];

        Completable.fromAction(() -> {
            if (!FileUtils.moveContents(source, target, excluded)) {
                throw new IOException("Failed to move " + currentPath + " to " + targetPath);
            }
        }).subscribeOn(Schedulers.io())
                .observeOn(AndroidSchedulers.mainThread())
                .subscribeWith(new DisposableCompletableObserver() {
                    @Override
                    public void onComplete() {
                        dialog.cancel();
                        dataStore.putString(PREF_EMULATOR_DIR, targetPath);
                        dataStore.save();
                        updateEmulatorDirSummary(targetPath);
                        AppUtils.restart(requireContext());
                    }

                    @Override
                    public void onError(@NonNull Throwable e) {
                        e.printStackTrace();
                        dialog.cancel();
                        Toast.makeText(getContext(), R.string.pref_emulator_dir_move_failed,
                                Toast.LENGTH_LONG).show();
                    }
                });
    }

    private void onLaunchFileDirPickResult(String newPath) {
        String previousPathDir = dataStore.getString(PREF_LAUNCH_FILE_DIR, null);
        ContentResolver contentResolver = requireContext().getContentResolver();

        if (previousPathDir != null) {
            if (previousPathDir.equals(newPath)) {
                return;
            }

            Uri oldPathUri = Uri.parse(previousPathDir);

            contentResolver.releasePersistableUriPermission(oldPathUri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        }

        Uri newPathUri = Uri.parse(newPath);

        contentResolver.takePersistableUriPermission(newPathUri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);

        dataStore.putString(PREF_LAUNCH_FILE_DIR, newPath);
        launchFileDirPreference.setSummary(newPath);
    }
}

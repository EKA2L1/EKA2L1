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

import android.content.ContentResolver;
import android.net.Uri;
import android.os.Bundle;
import android.view.MenuItem;
import android.view.View;
import android.widget.Toast;
import android.content.Intent;

import androidx.activity.result.ActivityResultLauncher;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceManager;
import androidx.preference.SwitchPreferenceCompat;

import com.github.eka2l1.R;
import com.github.eka2l1.emu.Emulator;
import com.github.eka2l1.util.FileUtils;

public class AndroidSettingsFragment extends PreferenceFragmentCompat {
    private AppDataStore dataStore;
    private Preference emulatorDirPreference;
    private Preference launchFileDirPreference;

    private final ActivityResultLauncher pickEmulatorDirectory = registerForActivityResult(
            FileUtils.getDirPicker(),
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
        emulatorDirPreference.setSummary(Emulator.getEmulatorDir());

        emulatorDirPreference.setOnPreferenceClickListener(preference -> {
            if (FileUtils.isExternalStorageLegacy()) {
                pickEmulatorDirectory.launch(null);
            } else {
                Toast.makeText(getActivity(), R.string.pref_emulator_dir_cant_change, Toast.LENGTH_SHORT).show();
            }

            return true;
        });

        String previousLaunchFileDir = dataStore.getString(PREF_EMULATOR_DIR, null);

        launchFileDirPreference = findPreference(PREF_LAUNCH_FILE_DIR);
        launchFileDirPreference.setSummary(previousLaunchFileDir);

        launchFileDirPreference.setOnPreferenceClickListener(preference -> {
            pickLaunchFileDirectory.launch(null);
            return true;
        });
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

    private void onEmulatorDirPickResult(String newPath) {
        dataStore.putString(PREF_EMULATOR_DIR, newPath);
        emulatorDirPreference.setSummary(newPath);

        getParentFragmentManager().setFragmentResult(KEY_RESTART, new Bundle());
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

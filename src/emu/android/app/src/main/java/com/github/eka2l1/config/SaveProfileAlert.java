/*
 * Copyright (c) 2021 EKA2L1 Team
 * Copyright (c) 2020 Yury Kharchenko
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

package com.github.eka2l1.config;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.os.Bundle;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import com.github.eka2l1.R;
import com.github.eka2l1.emu.Emulator;
import com.github.eka2l1.settings.AppDataStore;

import java.io.File;
import java.io.IOException;

import static com.github.eka2l1.emu.Constants.*;

public class SaveProfileAlert extends DialogFragment {

    @NonNull
    public static SaveProfileAlert getInstance(String parent) {
        SaveProfileAlert saveProfileAlert = new SaveProfileAlert();
        Bundle bundleSave = new Bundle();
        bundleSave.putString(KEY_CONFIG_PATH, parent);
        saveProfileAlert.setArguments(bundleSave);
        return saveProfileAlert;
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        String configPath = requireArguments().getString(KEY_CONFIG_PATH);
        LayoutInflater inflater = LayoutInflater.from(getActivity());
        @SuppressLint("InflateParams")
        View v = inflater.inflate(R.layout.dialog_save_profile, null);
        EditText editText = v.findViewById(R.id.et_name);
        CheckBox cbConfig = v.findViewById(R.id.cb_config);
        CheckBox cbKeyboard = v.findViewById(R.id.cb_keyboard);
        CheckBox cbBgImg = v.findViewById(R.id.cb_bg_img);
        CheckBox cbDefault = v.findViewById(R.id.cb_default);
        Button btNegative = v.findViewById(R.id.bt_negative);
        Button btPositive = v.findViewById(R.id.bt_positive);
        AlertDialog dialog = new AlertDialog.Builder(requireActivity())
                .setTitle(R.string.save_profile).setView(v).create();
        btNegative.setOnClickListener(v1 -> dismiss());
        btPositive.setOnClickListener(v1 -> {
            String name = editText.getText().toString().trim().replaceAll("[/\\\\:*?\"<>|]", "");
            if (name.isEmpty()) {
                Toast.makeText(getActivity(), R.string.error_name, Toast.LENGTH_SHORT).show();
                return;
            }

            final File config = new File(Emulator.getProfilesDir(), name + Emulator.APP_CONFIG_FILE);
            if (config.exists()) {
                editText.setText(name);
                editText.requestFocus();
                editText.setSelection(name.length());
                final Toast toast = Toast.makeText(getActivity(), R.string.error_name_exists, Toast.LENGTH_SHORT);
                final int[] loc = new int[2];
                editText.getLocationOnScreen(loc);
                toast.setGravity(Gravity.CENTER_HORIZONTAL | Gravity.TOP, 0, loc[1]);
                toast.show();
                return;
            }
            try {
                Profile profile = new Profile(name);
                ProfilesManager.save(profile, configPath,
                        cbConfig.isChecked(), cbKeyboard.isChecked(), cbBgImg.isChecked());
                if (cbDefault.isChecked()) {
                    AppDataStore dataStore = AppDataStore.getAndroidStore();
                    dataStore.putString(PREF_DEFAULT_PROFILE, name);
                    dataStore.save();
                }
                dismiss();
            } catch (IOException e) {
                e.printStackTrace();
                Toast.makeText(getActivity(), R.string.error, Toast.LENGTH_SHORT).show();
            }
        });
        return dialog;
    }
}

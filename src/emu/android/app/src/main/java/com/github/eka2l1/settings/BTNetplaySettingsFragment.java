/*
 * Copyright (c) 2022 EKA2L1 Team
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

import static com.github.eka2l1.emu.Constants.PREF_BT_NETPLAY;
import static com.github.eka2l1.emu.Constants.PREF_BT_NETPLAY_DIRECT_IP_MAN;
import static com.github.eka2l1.emu.Constants.PREF_BT_NETPLAY_PASSWORD;
import static com.github.eka2l1.emu.Constants.PREF_BT_NETPLAY_PORT_OFFSET;
import static com.github.eka2l1.emu.Constants.PREF_LANGUAGE;
import static com.github.eka2l1.emu.Constants.PREF_RTOS_LEVEL;

import android.os.Bundle;
import android.text.InputFilter;
import android.text.InputType;
import android.text.method.DigitsKeyListener;
import android.view.MenuItem;
import android.view.View;
import android.widget.EditText;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.EditTextPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceManager;

import com.github.eka2l1.R;
import com.github.eka2l1.emu.Emulator;

public class BTNetplaySettingsFragment extends PreferenceFragmentCompat {
    private static final String ACCEPTED_PASSWORD_DIGITS = "ABCDEFGHIJKLMNOUPQRSTUVWXYZ0123456789abcdefghijklmnoupqrstuvwxyz";
    private static final int MAXIMUM_PASSWORD_LENGTH = 16;

    private AppDataStore dataStore;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        dataStore = AppDataStore.getEmulatorStore();
        PreferenceManager preferenceManager = getPreferenceManager();
        preferenceManager.setPreferenceDataStore(dataStore);
        setPreferencesFromResource(R.xml.preferences_btnetplay, rootKey);
        Preference preference = findPreference(PREF_BT_NETPLAY_DIRECT_IP_MAN);
        preference.setOnPreferenceClickListener(pref -> {
            getParentFragmentManager().beginTransaction()
                    .replace(R.id.container, new BTNetplayDirectAddressesFragment())
                    .addToBackStack(null)
                    .commit();
            return true;
        });
        EditTextPreference portOffsetPreference = findPreference(PREF_BT_NETPLAY_PORT_OFFSET);
        EditTextPreference passwordPreference = findPreference(PREF_BT_NETPLAY_PASSWORD);
        portOffsetPreference.setOnBindEditTextListener(editText -> editText.setInputType(InputType.TYPE_CLASS_NUMBER));
        passwordPreference.setOnBindEditTextListener(editText -> {
            editText.setKeyListener(DigitsKeyListener.getInstance(ACCEPTED_PASSWORD_DIGITS));
            editText.setInputType(InputType.TYPE_CLASS_TEXT);
            editText.setFilters(new InputFilter[] {new InputFilter.LengthFilter(MAXIMUM_PASSWORD_LENGTH)});
        });
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        setHasOptionsMenu(true);
        ActionBar actionBar = ((AppCompatActivity) requireActivity()).getSupportActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);
        actionBar.setTitle(R.string.pref_bt_netplay);
    }

    @Override
    public void onPause() {
        super.onPause();
        dataStore.save();
        Emulator.loadConfig();
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            getParentFragmentManager().popBackStackImmediate();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}

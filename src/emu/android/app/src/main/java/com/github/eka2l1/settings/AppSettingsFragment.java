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

import android.os.Bundle;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceManager;

import com.github.eka2l1.R;

public class AppSettingsFragment extends PreferenceFragmentCompat {
    public static final String APP_UID_KEY = "appUid";

    private AppDataStore dataStore;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        Bundle args = getArguments();
        long uid = args.getLong(APP_UID_KEY, 0);
        String uidStr = Long.toHexString(uid).toUpperCase();
        dataStore = AppDataStore.getAppStore(uidStr);
        PreferenceManager preferenceManager = getPreferenceManager();
        preferenceManager.setPreferenceDataStore(dataStore);
        setPreferencesFromResource(R.xml.preferences_app, rootKey);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        setHasOptionsMenu(true);
        ActionBar actionBar = ((AppCompatActivity) getActivity()).getSupportActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);
        actionBar.setTitle(R.string.settings);
    }

    @Override
    public void onPause() {
        super.onPause();
        dataStore.save();
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                getParentFragmentManager().popBackStackImmediate();
                return true;
        }
        return super.onOptionsItemSelected(item);
    }
}

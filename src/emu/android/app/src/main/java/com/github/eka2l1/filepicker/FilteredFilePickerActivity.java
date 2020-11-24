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

package com.github.eka2l1.filepicker;

import android.os.Bundle;

import androidx.annotation.Nullable;

import com.github.eka2l1.R;
import com.github.eka2l1.settings.AppDataStore;
import com.nononsenseapps.filepicker.AbstractFilePickerActivity;
import com.nononsenseapps.filepicker.AbstractFilePickerFragment;

import java.io.File;
import java.util.Arrays;

public class FilteredFilePickerActivity extends AbstractFilePickerActivity<File> {
    public static final String EXTRA_EXTENSIONS = "nononsense.intent.EXTENSIONS";

    private FilteredFilePickerFragment currentFragment;
    private String[] extensions;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        AppDataStore dataStore = AppDataStore.getAndroidStore();
        String theme = dataStore.getString("theme", "light");
        if ("dark".equals(theme)) {
            setTheme(R.style.FilePickerTheme);
        } else {
            setTheme(R.style.FilePickerTheme_Light);
        }
        super.onCreate(savedInstanceState);
        extensions = getIntent().getStringArrayExtra(EXTRA_EXTENSIONS);
        if (extensions == null) {
            extensions = new String[0];
        }
    }

    @Override
    protected AbstractFilePickerFragment<File> getFragment(@Nullable String startPath, int mode, boolean allowMultiple,
                                                           boolean allowCreateDir, boolean allowExistingFile, boolean singleClick) {
        currentFragment = new FilteredFilePickerFragment();
        currentFragment.setArgs(startPath, mode, allowMultiple, allowCreateDir, allowExistingFile, singleClick);
        currentFragment.setExtensions(Arrays.asList(extensions));
        return currentFragment;
    }

    @Override
    public void onBackPressed() {
        if (currentFragment == null || currentFragment.isBackTop()) {
            super.onBackPressed();
        } else {
            currentFragment.goBack();
        }
    }
}

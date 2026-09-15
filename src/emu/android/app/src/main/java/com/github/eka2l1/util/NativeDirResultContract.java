/*
 * Copyright (c) 2025 EKA2L1 Team
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

package com.github.eka2l1.util;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.provider.DocumentsContract;

import androidx.activity.result.contract.ActivityResultContract;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.nononsenseapps.filepicker.Utils;

/**
 * Picks a directory as a filesystem path when the app has direct storage access, and
 * falls back to the storage access framework otherwise. The picker in use is decided
 * when the intent is built, so a permission granted while the app runs takes effect
 * without recreating the caller.
 */
public class NativeDirResultContract extends ActivityResultContract<Void, String> {
    private final PickDirResultContract pathPicker = new PickDirResultContract();
    private final SAFDirResultContract documentPicker = new SAFDirResultContract();

    @NonNull
    @Override
    public Intent createIntent(@NonNull Context context, Void input) {
        if (FileUtils.hasDirectStorageAccess()) {
            return pathPicker.createIntent(context, input);
        }
        return documentPicker.createIntent(context, input);
    }

    @Override
    public String parseResult(int resultCode, @Nullable Intent intent) {
        if (resultCode != Activity.RESULT_OK || intent == null || intent.getData() == null) {
            return null;
        }
        Uri uri = intent.getData();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && DocumentsContract.isTreeUri(uri)) {
            return documentPicker.parseResult(resultCode, intent);
        }
        return Utils.getFileForUri(uri).getAbsolutePath();
    }
}

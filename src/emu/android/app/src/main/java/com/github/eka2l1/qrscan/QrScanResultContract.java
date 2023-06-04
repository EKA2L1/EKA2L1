package com.github.eka2l1.qrscan;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.activity.result.contract.ActivityResultContract;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.github.eka2l1.filepicker.FilteredFilePickerActivity;
import com.github.eka2l1.filepicker.FilteredFilePickerFragment;
import com.nononsenseapps.filepicker.FilePickerActivity;

public class QrScanResultContract extends ActivityResultContract<Integer, String> {
    @NonNull
    @Override
    public Intent createIntent(@NonNull Context context, Integer id) {
        Intent intent = new Intent(context, QrScanActivity.class);
        intent.putExtra(QrScanActivity.EXTRA_TITLE_ID, id.intValue());

        return intent;
    }

    @Override
    public String parseResult(int resultCode, @Nullable Intent intent) {
        if (resultCode == Activity.RESULT_OK && intent != null) {
            return intent.getStringExtra("value");
        }
        return null;
    }
}

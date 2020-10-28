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

package com.github.eka2l1.info;

import android.app.Dialog;
import android.os.Bundle;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import com.github.eka2l1.BuildConfig;
import com.github.eka2l1.R;

public class AboutDialogFragment extends DialogFragment {
    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        StringBuilder message = new StringBuilder().append(getText(R.string.version))
                .append(BuildConfig.VERSION_NAME)
                .append('-')
                .append(BuildConfig.GIT_HASH)
                .append(getText(R.string.about_github))
                .append(getText(R.string.about_crowdin))
                .append(getText(R.string.about_copyright));
        TextView tv = new TextView(getActivity());
        tv.setMovementMethod(LinkMovementMethod.getInstance());
        tv.setText(Html.fromHtml(message.toString()));
        tv.setTextSize(16);
        float density = getResources().getDisplayMetrics().density;
        int paddingHorizontal = (int) (density * 20);
        int paddingVertical = (int) (density * 14);
        tv.setPadding(paddingHorizontal, paddingVertical, paddingHorizontal, paddingVertical);
        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
        builder.setTitle(R.string.app_name)
                .setIcon(R.mipmap.ic_launcher)
                .setView(tv);
        return builder.create();
    }
}

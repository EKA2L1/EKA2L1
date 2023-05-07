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

import com.github.eka2l1.R;

public class TranslatorsDialogFragment extends DialogFragment {
    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        StringBuilder message = new StringBuilder().append(getText(R.string.about_translator_content));
        TextView tv = new TextView(getActivity());
        tv.setMovementMethod(LinkMovementMethod.getInstance());
        tv.setText(Html.fromHtml(message.toString()));
        tv.setTextSize(16);
        float density = getResources().getDisplayMetrics().density;
        int paddingHorizontal = (int) (density * 20);
        int paddingVertical = (int) (density * 14);
        tv.setPadding(paddingHorizontal, paddingVertical, paddingHorizontal, paddingVertical);
        AlertDialog.Builder builder = new AlertDialog.Builder(requireActivity());
        builder.setTitle(R.string.about_translator)
                .setIcon(R.mipmap.ic_ducky)
                .setView(tv)
                .setPositiveButton(R.string.about_special_thanks_title, (d, w) -> {
                    SpecialThanksDialogFragment specialThanksFragment = new SpecialThanksDialogFragment();
                    specialThanksFragment.show(getParentFragmentManager(), "specialThanks");
                })
                .setNegativeButton(R.string.about, (d, w) -> {
                    AboutDialogFragment aboutDialogFragment = new AboutDialogFragment();
                    aboutDialogFragment.show(getParentFragmentManager(), "about");
                });
        return builder.create();
    }
}
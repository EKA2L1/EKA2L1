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

package com.github.eka2l1.applist;

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
import com.github.eka2l1.emu.Emulator;

public class NG2LicenseInstallResultDialogFragment extends DialogFragment {
    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        String[] successInstallLicenses = Emulator.getSuccessInstalledLicenseGames();
        String[] failedInstallLicenses = Emulator.getFailedInstalledLicenseGames();

        StringBuilder message = new StringBuilder();

        if ((successInstallLicenses.length + failedInstallLicenses.length) == 1) {
            if (successInstallLicenses.length != 0) {
                message.append(getString(R.string.ng2_license_install_success_single_game, successInstallLicenses[0]));
            } else {
                message.append(getString(R.string.ng2_license_install_failed_single_game, failedInstallLicenses[0]));
            }
        } else {
            message.append(getString(R.string.ng2_license_install_success_multiple_game));

            for (int i = 0; i < successInstallLicenses.length; i++) {
                message.append("  ● " + successInstallLicenses[i]);
            }

            message.append("");
            message.append(getString(R.string.ng2_license_install_failed_multiple_game));

            for (int i = 0; i < failedInstallLicenses.length; i++) {
                message.append("  ● " + failedInstallLicenses[i]);
            }
        }

        TextView tv = new TextView(getActivity());
        tv.setMovementMethod(LinkMovementMethod.getInstance());
        tv.setText(Html.fromHtml(message.toString()));
        tv.setTextSize(16);
        float density = getResources().getDisplayMetrics().density;
        int paddingHorizontal = (int) (density * 20);
        int paddingVertical = (int) (density * 14);
        tv.setPadding(paddingHorizontal, paddingVertical, paddingHorizontal, paddingVertical);
        AlertDialog.Builder builder = new AlertDialog.Builder(requireActivity());
        builder.setTitle(R.string.ng2_license_install_result)
                .setView(tv)
                .setPositiveButton(android.R.string.ok, (d, w) -> getActivity().getFragmentManager().popBackStack());

        return builder.create();
    }
}

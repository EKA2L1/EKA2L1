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
import android.content.Context;
import android.os.Bundle;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import com.github.eka2l1.R;
import com.github.eka2l1.emu.Emulator;

import java.io.File;

import static com.github.eka2l1.emu.Constants.*;

public class EditNameAlert extends DialogFragment {

    private static final String TITLE = "title";
    private static final String ID = "id";
    private String title;
    private int id;

    static EditNameAlert newInstance(String title, int id) {
        EditNameAlert fragment = new EditNameAlert();
        Bundle args = new Bundle();
        args.putString(TITLE, title);
        args.putInt(ID, id);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onAttach(@NonNull Context context) {
        super.onAttach(context);
        final Bundle args = getArguments();
        if (args != null) {
            title = args.getString(TITLE);
            id = args.getInt(ID);
        }
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        LayoutInflater inflater = LayoutInflater.from(getActivity());
        @SuppressLint("InflateParams")
        View v = inflater.inflate(R.layout.dialog_change_name, null);
        EditText editText = v.findViewById(R.id.et_name);
        Button btNegative = v.findViewById(R.id.bt_negative);
        Button btPositive = v.findViewById(R.id.bt_positive);
        AlertDialog dialog = new AlertDialog.Builder(requireActivity())
                .setTitle(title).setView(v).create();
        btNegative.setOnClickListener(v1 -> dismiss());
        btPositive.setOnClickListener(v1 -> onClickOk(editText));
        return dialog;
    }

    private void onClickOk(EditText editText) {
        String name = editText.getText().toString().trim().replaceAll("[/\\\\:*?\"<>|]", "");
        if (name.isEmpty()) {
            editText.setText(name);
            editText.requestFocus();
            editText.setSelection(name.length());
            Toast.makeText(getActivity(), R.string.error_name, Toast.LENGTH_SHORT).show();
            return;
        }
        final File config = new File(Emulator.getProfilesDir(), name + Emulator.APP_CONFIG_FILE);
        if (config.exists()) {
            editText.setText(name);
            editText.requestFocus();
            editText.setSelection(name.length());
            final Toast toast = Toast.makeText(getActivity(), R.string.not_saved_exists, Toast.LENGTH_SHORT);
            toast.setGravity(Gravity.CENTER_HORIZONTAL | Gravity.TOP, 0, 50);
            toast.show();
            return;
        }
        Bundle result = new Bundle();
        result.putString(KEY_NAME, name);
        result.putInt(KEY_ID, id);
        getParentFragmentManager().setFragmentResult(KEY_PROFILE_CHANGED, result);
        dismiss();
    }
}

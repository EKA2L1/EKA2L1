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

package com.github.eka2l1.settings;

import static com.github.eka2l1.emu.Constants.KEY_ADDRESS;
import static com.github.eka2l1.emu.Constants.KEY_ADDRESS_CHANGED;
import static com.github.eka2l1.emu.Constants.KEY_ID;
import static com.github.eka2l1.emu.Constants.KEY_PORT;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import com.github.eka2l1.R;

public class EditAddressAlert extends DialogFragment {

    private static final String ID = "id";
    private static final String ADDRESS = "address";
    private static final String PORT = "port";
    private int id;
    private String address;
    private String port;

    static EditAddressAlert newInstance(int id, String address, String port) {
        EditAddressAlert fragment = new EditAddressAlert();
        Bundle args = new Bundle();
        args.putInt(ID, id);
        args.putString(ADDRESS, address);
        args.putString(PORT, port);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onAttach(@NonNull Context context) {
        super.onAttach(context);
        final Bundle args = getArguments();
        if (args != null) {
            id = args.getInt(ID);
            address = args.getString(ADDRESS);
            port = args.getString(PORT);
        }
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        LayoutInflater inflater = LayoutInflater.from(getActivity());
        @SuppressLint("InflateParams")
        View v = inflater.inflate(R.layout.dialog_change_address, null);
        EditText etAddress = v.findViewById(R.id.et_address);
        EditText etPort = v.findViewById(R.id.et_port);
        etAddress.setText(address);
        etPort.setText(port);
        Button btNegative = v.findViewById(R.id.bt_negative);
        Button btPositive = v.findViewById(R.id.bt_positive);
        AlertDialog dialog = new AlertDialog.Builder(requireActivity())
                .setTitle(R.string.enter_address).setView(v).create();
        btNegative.setOnClickListener(v1 -> dismiss());
        btPositive.setOnClickListener(v1 -> onClickOk(etAddress, etPort));
        return dialog;
    }

    private void onClickOk(EditText etAddress, EditText etPort) {
        Bundle result = new Bundle();
        result.putInt(KEY_ID, id);
        result.putString(KEY_ADDRESS, etAddress.getText().toString());
        result.putString(KEY_PORT, etPort.getText().toString());
        getParentFragmentManager().setFragmentResult(KEY_ADDRESS_CHANGED, result);
        dismiss();
    }
}

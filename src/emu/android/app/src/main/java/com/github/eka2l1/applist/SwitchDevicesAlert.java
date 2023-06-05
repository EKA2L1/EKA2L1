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

package com.github.eka2l1.applist;

import static com.github.eka2l1.emu.Constants.KEY_RESTART;
import static com.github.eka2l1.emu.Constants.KEY_RESTART_IMMEDIATELY;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import com.github.eka2l1.R;
import com.github.eka2l1.emu.Emulator;

public class SwitchDevicesAlert extends DialogFragment {
    private int currentDeviceIndex;

    static SwitchDevicesAlert newInstance() {
        return new SwitchDevicesAlert();
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        LayoutInflater inflater = LayoutInflater.from(getActivity());
        @SuppressLint("InflateParams")
        View v = inflater.inflate(R.layout.dialog_switch_devices, null);
        ListView listView = v.findViewById(android.R.id.list);

        String[] devices = Emulator.getDevices();
        currentDeviceIndex = Emulator.getCurrentDevice();

        ArrayAdapter<String> adapter = new ArrayAdapter<>(requireActivity(), android.R.layout.simple_list_item_single_choice, devices);
        listView.setOnItemClickListener(this::onItemClick);
        listView.setAdapter(adapter);
        listView.setItemChecked(currentDeviceIndex, true);

        AlertDialog.Builder builder = new AlertDialog.Builder(requireActivity());
        builder.setTitle(R.string.switch_devices)
                .setView(v)
                .setNegativeButton(android.R.string.cancel, null);

        return builder.create();
    }

    private void onItemClick(AdapterView<?> adapterView, View view, int i, long l) {
        try {
            if ((i != -1) && (i != currentDeviceIndex)) {
                Toast.makeText(getActivity(), R.string.completed, Toast.LENGTH_SHORT).show();

                Emulator.setCurrentDevice(i, false);
                getParentFragmentManager().setFragmentResult(KEY_RESTART_IMMEDIATELY, new Bundle());
            }

            dismiss();
        } catch (Exception e) {
            e.printStackTrace();
            Toast.makeText(getActivity(), R.string.error, Toast.LENGTH_SHORT).show();
        }
    }
}

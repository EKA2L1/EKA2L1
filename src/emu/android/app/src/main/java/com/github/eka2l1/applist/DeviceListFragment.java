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

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ProgressDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;

import com.github.eka2l1.R;
import com.github.eka2l1.emu.Emulator;
import com.github.eka2l1.filepicker.FilteredFilePickerActivity;
import com.github.eka2l1.filepicker.FilteredFilePickerFragment;
import com.nononsenseapps.filepicker.FilePickerActivity;
import com.nononsenseapps.filepicker.Utils;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import io.reactivex.Completable;
import io.reactivex.android.schedulers.AndroidSchedulers;
import io.reactivex.observers.DisposableCompletableObserver;
import io.reactivex.schedulers.Schedulers;

public class DeviceListFragment extends Fragment {
    private static final int RAW_DUMP_CODE = 0;
    private static final int RPKG_CODE = 1;
    private static final int ROM_CODE = 2;

    private enum INSTALL_MODE {RPKG, RAW_DUMP}

    private INSTALL_MODE mode;
    private TextView tvRawDump;
    private TextView tvRPKG;
    private TextView tvROM;
    private boolean rawDumpSet, rpkgSet, romSet;
    private ArrayAdapter<String> deviceAdapter;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_devicelist, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        setHasOptionsMenu(true);
        ActionBar actionBar = ((AppCompatActivity) getActivity()).getSupportActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);
        actionBar.setTitle(R.string.devices);

        LinearLayout llRawDump = getActivity().findViewById(R.id.ll_raw_dump);
        LinearLayout llRpkg = getActivity().findViewById(R.id.ll_rpkg);
        tvRawDump = getActivity().findViewById(R.id.tv_raw_dump);
        tvRPKG = getActivity().findViewById(R.id.tv_rpkg);
        tvROM = getActivity().findViewById(R.id.tv_rom);

        ArrayList<String> devices = new ArrayList<>(Arrays.asList(Emulator.getDevices()));
        deviceAdapter = new ArrayAdapter<>(getActivity(), android.R.layout.simple_list_item_1, devices);
        Spinner spDevice = getActivity().findViewById(R.id.sp_device_list);
        spDevice.setAdapter(deviceAdapter);
        spDevice.setSelection(Emulator.getCurrentDevice(), false);
        spDevice.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                Emulator.setCurrentDevice(position);
                ((AppsListFragment) getTargetFragment()).setRestartNeeded(true);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });
        Spinner spInstallMethod = getActivity().findViewById(R.id.sp_device_install_method);
        spInstallMethod.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                if (position == 0) {
                    mode = INSTALL_MODE.RPKG;
                    llRpkg.setVisibility(View.VISIBLE);
                    llRawDump.setVisibility(View.GONE);
                } else {
                    mode = INSTALL_MODE.RAW_DUMP;
                    llRawDump.setVisibility(View.VISIBLE);
                    llRpkg.setVisibility(View.GONE);
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });

        Button btRPKG = getActivity().findViewById(R.id.bt_rpkg);
        btRPKG.setOnClickListener(v -> {
            Intent i = new Intent(getActivity(), FilteredFilePickerActivity.class);
            i.putExtra(FilePickerActivity.EXTRA_ALLOW_MULTIPLE, false);
            i.putExtra(FilePickerActivity.EXTRA_SINGLE_CLICK, true);
            i.putExtra(FilePickerActivity.EXTRA_ALLOW_CREATE_DIR, false);
            i.putExtra(FilePickerActivity.EXTRA_MODE, FilePickerActivity.MODE_FILE);
            i.putExtra(FilePickerActivity.EXTRA_START_PATH, FilteredFilePickerFragment.getLastPath());
            i.putExtra(FilteredFilePickerActivity.EXTRA_EXTENSIONS, new String[]{".RPKG", ".rpkg"});
            startActivityForResult(i, RPKG_CODE);
        });
        Button btROM = getActivity().findViewById(R.id.bt_rom);
        btROM.setOnClickListener(v -> {
            Intent i = new Intent(getActivity(), FilteredFilePickerActivity.class);
            i.putExtra(FilePickerActivity.EXTRA_ALLOW_MULTIPLE, false);
            i.putExtra(FilePickerActivity.EXTRA_SINGLE_CLICK, true);
            i.putExtra(FilePickerActivity.EXTRA_ALLOW_CREATE_DIR, false);
            i.putExtra(FilePickerActivity.EXTRA_MODE, FilePickerActivity.MODE_FILE);
            i.putExtra(FilePickerActivity.EXTRA_START_PATH, FilteredFilePickerFragment.getLastPath());
            i.putExtra(FilteredFilePickerActivity.EXTRA_EXTENSIONS, new String[]{".ROM", ".rom"});
            startActivityForResult(i, ROM_CODE);
        });
        Button btRawDump = getActivity().findViewById(R.id.bt_raw_dump);
        btRawDump.setOnClickListener(v -> {
            Intent i = new Intent(getActivity(), FilteredFilePickerActivity.class);
            i.putExtra(FilePickerActivity.EXTRA_ALLOW_MULTIPLE, false);
            i.putExtra(FilePickerActivity.EXTRA_SINGLE_CLICK, false);
            i.putExtra(FilePickerActivity.EXTRA_ALLOW_CREATE_DIR, false);
            i.putExtra(FilePickerActivity.EXTRA_MODE, FilePickerActivity.MODE_DIR);
            i.putExtra(FilePickerActivity.EXTRA_START_PATH, FilteredFilePickerFragment.getLastPath());
            startActivityForResult(i, RAW_DUMP_CODE);
        });
        Button btInstall = getActivity().findViewById(R.id.bt_device_install);
        btInstall.setOnClickListener(v -> installDevice());
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if ((requestCode >= RAW_DUMP_CODE && requestCode <= ROM_CODE) && resultCode == Activity.RESULT_OK) {
            List<Uri> files = Utils.getSelectedFilesFromResult(data);
            for (Uri uri : files) {
                File file = Utils.getFileForUri(uri);
                String path = file.getAbsolutePath();
                if (requestCode == RAW_DUMP_CODE) {
                    tvRawDump.setText(path);
                    rawDumpSet = true;
                } else if (requestCode == RPKG_CODE) {
                    tvRPKG.setText(path);
                    rpkgSet = true;
                } else {
                    tvROM.setText(path);
                    romSet = true;
                }
            }
        }
    }

    private void updateDeviceList() {
        deviceAdapter.clear();
        deviceAdapter.addAll(Emulator.getDevices());
        deviceAdapter.notifyDataSetChanged();
    }

    @SuppressLint("CheckResult")
    private void installDevice() {
        if (mode == INSTALL_MODE.RPKG && (!rpkgSet || !romSet)) {
            Toast.makeText(getContext(), R.string.error, Toast.LENGTH_SHORT).show();
            return;
        } else if (mode == INSTALL_MODE.RAW_DUMP && (!rawDumpSet || !romSet)) {
            Toast.makeText(getContext(), R.string.error, Toast.LENGTH_SHORT).show();
            return;
        }
        ProgressDialog dialog = new ProgressDialog(getActivity());
        dialog.setIndeterminate(true);
        dialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
        dialog.setCancelable(false);
        dialog.setMessage(getText(R.string.processing));
        dialog.show();
        Completable completable;
        if (mode == INSTALL_MODE.RPKG) {
            String rpkg = tvRPKG.getText().toString();
            String rom = tvROM.getText().toString();
            completable = Emulator.subscribeInstallDevice(rpkg, rom, true);
        } else {
            String rawDump = tvRawDump.getText().toString();
            String rom = tvROM.getText().toString();
            completable = Emulator.subscribeInstallDevice(rawDump, rom, false);
        }
        completable.subscribeOn(Schedulers.io())
                .observeOn(AndroidSchedulers.mainThread())
                .subscribeWith(new DisposableCompletableObserver() {
                    @Override
                    public void onComplete() {
                        dialog.cancel();
                        updateDeviceList();
                        ((AppsListFragment) getTargetFragment()).setRestartNeeded(true);
                        Toast.makeText(getContext(), R.string.completed, Toast.LENGTH_SHORT).show();
                    }

                    @Override
                    public void onError(Throwable e) {
                        dialog.cancel();
                        Toast.makeText(getContext(), R.string.error, Toast.LENGTH_SHORT).show();
                    }
                });
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

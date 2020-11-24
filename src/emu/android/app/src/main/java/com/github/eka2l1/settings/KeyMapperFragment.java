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

package com.github.eka2l1.settings;

import android.os.Bundle;
import android.util.SparseIntArray;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;

import com.github.eka2l1.R;
import com.github.eka2l1.emu.Keycode;

public class KeyMapperFragment extends Fragment implements View.OnClickListener {
    private static SparseIntArray idToSymbianKey = new SparseIntArray();
    private static SparseIntArray androidToSymbian;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_keymapper, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        setHasOptionsMenu(true);
        ActionBar actionBar = ((AppCompatActivity) getActivity()).getSupportActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);
        actionBar.setTitle(R.string.pref_control_key_binding_sect_title);

        setupButton(R.id.virtual_key_left_soft, Keycode.KEY_SOFT_LEFT);
        setupButton(R.id.virtual_key_right_soft, Keycode.KEY_SOFT_RIGHT);
        setupButton(R.id.virtual_key_d, Keycode.KEY_SEND);
        setupButton(R.id.virtual_key_c, Keycode.KEY_END);
        setupButton(R.id.virtual_key_left, Keycode.KEY_LEFT);
        setupButton(R.id.virtual_key_right, Keycode.KEY_RIGHT);
        setupButton(R.id.virtual_key_up, Keycode.KEY_UP);
        setupButton(R.id.virtual_key_down, Keycode.KEY_DOWN);
        setupButton(R.id.virtual_key_f, Keycode.KEY_FIRE);
        setupButton(R.id.virtual_key_1, Keycode.KEY_NUM1);
        setupButton(R.id.virtual_key_2, Keycode.KEY_NUM2);
        setupButton(R.id.virtual_key_3, Keycode.KEY_NUM3);
        setupButton(R.id.virtual_key_4, Keycode.KEY_NUM4);
        setupButton(R.id.virtual_key_5, Keycode.KEY_NUM5);
        setupButton(R.id.virtual_key_6, Keycode.KEY_NUM6);
        setupButton(R.id.virtual_key_7, Keycode.KEY_NUM7);
        setupButton(R.id.virtual_key_8, Keycode.KEY_NUM8);
        setupButton(R.id.virtual_key_9, Keycode.KEY_NUM9);
        setupButton(R.id.virtual_key_0, Keycode.KEY_NUM0);
        setupButton(R.id.virtual_key_star, Keycode.KEY_STAR);
        setupButton(R.id.virtual_key_pound, Keycode.KEY_POUND);
        androidToSymbian = KeyMapper.getArrayPref();
    }

    private void setupButton(int resId, int index) {
        idToSymbianKey.put(resId, index);
        Button button = getActivity().findViewById(resId);
        button.setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        int canvasKey = idToSymbianKey.get(v.getId());
        if (canvasKey != 0) {
            showMappingDialog(canvasKey);
        }
    }

    private void showMappingDialog(int canvasKey) {
        int id = androidToSymbian.indexOfValue(canvasKey);
        String keyName = "";
        if (id >= 0) {
            keyName = KeyEvent.keyCodeToString(androidToSymbian.keyAt(id));
        }
        AlertDialog.Builder builder = new AlertDialog.Builder(getContext())
                .setTitle(R.string.mapping_dialog_title)
                .setMessage(getString(R.string.mapping_dialog_message, keyName))
                .setOnKeyListener((dialog, keyCode, event) -> {
                    if (keyCode == KeyEvent.KEYCODE_BACK) {
                        dialog.dismiss();
                        return false;
                    } else {
                        deleteDuplicates(canvasKey);
                        androidToSymbian.put(keyCode, canvasKey);
                        KeyMapper.saveArrayPref(androidToSymbian);

                        dialog.dismiss();
                        return true;
                    }
                });
        builder.show();
    }

    private void deleteDuplicates(int value) {
        for (int i = 0; i < androidToSymbian.size(); i++) {
            if (androidToSymbian.indexOfValue(value) == i) {
                androidToSymbian.removeAt(i);
            }
        }
    }

    @Override
    public void onCreateOptionsMenu(@NonNull Menu menu, @NonNull MenuInflater inflater) {
        super.onCreateOptionsMenu(menu, inflater);
        inflater.inflate(R.menu.keymapper, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                getParentFragmentManager().popBackStackImmediate();
                break;
            case R.id.action_reset_mapping:
                androidToSymbian.clear();
                KeyMapper.initArray(androidToSymbian);
                KeyMapper.saveArrayPref(androidToSymbian);
                break;
        }
        return super.onOptionsItemSelected(item);
    }
}

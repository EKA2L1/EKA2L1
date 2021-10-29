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
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.ListView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.preference.PreferenceManager;

import com.github.eka2l1.R;

import java.util.ArrayList;
import java.util.Collections;

import static com.github.eka2l1.emu.Constants.*;

public class LoadProfileAlert extends DialogFragment {
    private ArrayList<Profile> profiles;
    private CheckBox cbConfig;
    private CheckBox cbKeyboard;

    static LoadProfileAlert newInstance(String parent) {
        LoadProfileAlert fragment = new LoadProfileAlert();
        Bundle args = new Bundle();
        args.putString(KEY_CONFIG_PATH, parent);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        profiles = ProfilesManager.getProfiles();
        Collections.sort(profiles);
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        String configPath = requireArguments().getString(KEY_CONFIG_PATH);
        LayoutInflater inflater = LayoutInflater.from(getActivity());
        @SuppressLint("InflateParams")
        View v = inflater.inflate(R.layout.dialog_load_profile, null);
        ListView listView = v.findViewById(android.R.id.list);
        ArrayAdapter<Profile> adapter = new ArrayAdapter<>(requireActivity(),
                android.R.layout.simple_list_item_single_choice, profiles);
        listView.setOnItemClickListener(this::onItemClick);
        listView.setAdapter(adapter);
        cbConfig = v.findViewById(R.id.cb_config);
        cbKeyboard = v.findViewById(R.id.cb_keyboard);
        AlertDialog.Builder builder = new AlertDialog.Builder(requireActivity());
        builder.setTitle(R.string.load_profile)
                .setView(v)
                .setPositiveButton(android.R.string.ok, (dialog, which) -> {
                    try {
                        final int pos = listView.getCheckedItemPosition();
                        final boolean configChecked = cbConfig.isChecked();
                        final boolean vkChecked = cbKeyboard.isChecked();
                        if (pos == -1) {
                            Toast.makeText(getActivity(), R.string.error, Toast.LENGTH_SHORT).show();
                            return;
                        }
                        ProfilesManager.load((Profile) listView.getItemAtPosition(pos), configPath,
                                configChecked, vkChecked);
                        getParentFragmentManager().setFragmentResult(KEY_PROFILE_LOADED, new Bundle());
                    } catch (Exception e) {
                        e.printStackTrace();
                        Toast.makeText(getActivity(), R.string.error, Toast.LENGTH_SHORT).show();
                    }
                })
                .setNegativeButton(android.R.string.cancel, null);
        final String def = PreferenceManager.getDefaultSharedPreferences(requireContext())
                .getString(PREF_DEFAULT_PROFILE, null);

        if (def != null) {
            for (int i = 0, size = profiles.size(); i < size; i++) {
                Profile profile = profiles.get(i);
                if (profile.getName().equals(def)) {
                    listView.setItemChecked(i, true);
                    onItemClick(listView, null, i, i);
                    break;
                }
            }
        }
        return builder.create();
    }

    private void onItemClick(AdapterView<?> adapterView, View view, int pos, long l) {
        final Profile profile = profiles.get(pos);
        final boolean hasConfig = profile.hasConfig();
        final boolean hasVk = profile.hasKeyLayout();
        cbConfig.setEnabled(hasConfig && hasVk);
        cbConfig.setChecked(hasConfig);
        cbKeyboard.setEnabled(hasVk && hasConfig);
        cbKeyboard.setChecked(hasVk);
    }

}

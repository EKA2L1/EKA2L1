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

import android.os.Bundle;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.ListFragment;

import com.github.eka2l1.R;
import com.github.eka2l1.settings.AppDataStore;

import java.util.ArrayList;

import static com.github.eka2l1.emu.Constants.*;

public class ProfilesFragment extends ListFragment implements AdapterView.OnItemClickListener {
    private ProfilesAdapter adapter;
    private AppDataStore dataStore;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        dataStore = AppDataStore.getAndroidStore();
        ArrayList<Profile> profiles = ProfilesManager.getProfiles();
        adapter = new ProfilesAdapter(getContext(), profiles);
        final String def = dataStore.getString(PREF_DEFAULT_PROFILE, null);
        if (def != null) {
            for (int i = profiles.size() - 1; i >= 0; i--) {
                Profile profile = profiles.get(i);
                if (profile.getName().equals(def)) {
                    adapter.setDefault(profile);
                    break;
                }
            }
        }
        getParentFragmentManager().setFragmentResultListener(KEY_PROFILE_CHANGED, this, (requestKey, bundle) -> {
            final String newName = bundle.getString(KEY_NAME);
            int id = bundle.getInt(KEY_ID);
            if (id == -1) {
                Bundle args = new Bundle();
                args.putString(KEY_ACTION, ACTION_EDIT_PROFILE);
                args.putString(KEY_PROFILE_NAME, newName);
                args.putBoolean(KEY_PROFILE_CREATE, true);
                ConfigFragment configFragment = new ConfigFragment();
                configFragment.setArguments(args);
                getParentFragmentManager().beginTransaction()
                        .replace(R.id.container, configFragment)
                        .addToBackStack(null)
                        .commit();
                return;
            }
            Profile profile = adapter.getItem(id);
            profile.renameTo(newName);
            adapter.notifyDataSetChanged();
            if (adapter.getDefault() == profile) {
                dataStore.putString(PREF_DEFAULT_PROFILE, newName);
                dataStore.save();
            }
        });
        getParentFragmentManager().setFragmentResultListener(KEY_PROFILE_CREATED, this, (requestKey, bundle) -> {
            final String name = bundle.getString(KEY_NAME);
            if (name == null)
                return;
            adapter.addItem(new Profile(name));
            adapter.notifyDataSetChanged();
        });
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_profiles, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        registerForContextMenu(getListView());
        setHasOptionsMenu(true);
        setListAdapter(adapter);
        getListView().setOnItemClickListener(this);
        ActionBar actionBar = ((AppCompatActivity) getActivity()).getSupportActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);
        actionBar.setTitle(R.string.profiles);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, @NonNull MenuInflater inflater) {
        super.onCreateOptionsMenu(menu, inflater);
        inflater.inflate(R.menu.activity_profiles, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int itemId = item.getItemId();
        if (itemId == android.R.id.home) {
            getParentFragmentManager().popBackStackImmediate();
            return true;
        } else if (itemId == R.id.add) {
            EditNameAlert.newInstance(getString(R.string.enter_name), -1)
                    .show(getParentFragmentManager(), "alert_create_profile");
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, v, menuInfo);
        MenuInflater inflater = getActivity().getMenuInflater();
        inflater.inflate(R.menu.context_profile, menu);
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
        final Profile profile = adapter.getItem(info.position);
        if (!profile.hasConfig()) {
            menu.findItem(R.id.action_context_default).setVisible(false);
            menu.findItem(R.id.action_context_edit).setVisible(false);
        }
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        int index = info.position;
        final Profile profile = adapter.getItem(index);
        int itemId = item.getItemId();
        if (itemId == R.id.action_context_default) {
            dataStore.putString(PREF_DEFAULT_PROFILE, profile.getName());
            dataStore.save();
            adapter.setDefault(profile);
            return true;
        } else if (itemId == R.id.action_context_edit) {
            Bundle args = new Bundle();
            args.putString(KEY_ACTION, ACTION_EDIT_PROFILE);
            args.putString(KEY_PROFILE_NAME, profile.getName());
            args.putBoolean(KEY_PROFILE_CREATE, true);
            ConfigFragment configFragment = new ConfigFragment();
            configFragment.setArguments(args);
            getParentFragmentManager().beginTransaction()
                    .replace(R.id.container, configFragment)
                    .addToBackStack(null)
                    .commit();
            return true;
        } else if (itemId == R.id.action_context_rename) {
            EditNameAlert.newInstance(getString(R.string.enter_new_name), index)
                    .show(getParentFragmentManager(), "alert_rename_profile");
        } else if (itemId == R.id.action_context_delete) {
            profile.delete();
            adapter.removeItem(index);
        }
        return super.onContextItemSelected(item);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        parent.showContextMenuForChild(view);
    }
}

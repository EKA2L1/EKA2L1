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
import com.github.eka2l1.emu.Emulator;

import java.util.ArrayList;

public class BTNetplayDirectAddressesFragment extends ListFragment implements AdapterView.OnItemClickListener {
    private BTNetplayDirectAddressesAdapter adapter;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ArrayList<BTNetplayAddress> addresses = BTNetplayDirectAddressesManager.getAddresses();
        adapter = new BTNetplayDirectAddressesAdapter(getContext(), addresses);
        getParentFragmentManager().setFragmentResultListener(KEY_ADDRESS_CHANGED, this, (requestKey, bundle) -> {
            int id = bundle.getInt(KEY_ID);
            final String address = bundle.getString(KEY_ADDRESS);
            final String port = bundle.getString(KEY_PORT);
            if (id == -1) {
                adapter.addItem(new BTNetplayAddress(address, port));
            } else {
                BTNetplayAddress btNetplayAddress = adapter.getItem(id);
                btNetplayAddress.setAddress(address);
                btNetplayAddress.setPort(port);
                adapter.notifyDataSetChanged();
            }
        });
    }

    @Override
    public void onPause() {
        super.onPause();
        BTNetplayDirectAddressesManager.saveAddresses(adapter.getItems());
        Emulator.loadConfig();
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_bt_netplay, container, false);
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
        actionBar.setTitle(R.string.pref_bt_netplay_direct_ips_manager);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, @NonNull MenuInflater inflater) {
        super.onCreateOptionsMenu(menu, inflater);
        inflater.inflate(R.menu.bt_address, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int itemId = item.getItemId();
        if (itemId == android.R.id.home) {
            getParentFragmentManager().popBackStackImmediate();
            return true;
        } else if (itemId == R.id.add) {
            EditAddressAlert.newInstance(-1, "", "")
                    .show(getParentFragmentManager(), "alert_create_address");
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, v, menuInfo);
        MenuInflater inflater = getActivity().getMenuInflater();
        inflater.inflate(R.menu.context_bt_address, menu);
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        int index = info.position;
        final BTNetplayAddress btNetplayAddress = adapter.getItem(index);
        int itemId = item.getItemId();
        if (itemId == R.id.action_context_edit) {
            EditAddressAlert.newInstance(index, btNetplayAddress.getAddress(), btNetplayAddress.getPort())
                    .show(getParentFragmentManager(), "alert_change_address");
            return true;
        } else if (itemId == R.id.action_context_delete) {
            adapter.removeItem(index);
        }
        return super.onContextItemSelected(item);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        parent.showContextMenuForChild(view);
    }
}

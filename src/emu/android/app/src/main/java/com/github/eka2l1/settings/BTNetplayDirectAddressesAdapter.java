/*
 * Copyright (c) 2022 EKA2L1 Team
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

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

import com.github.eka2l1.R;

import java.util.ArrayList;

public class BTNetplayDirectAddressesAdapter extends BaseAdapter {
    private ArrayList<BTNetplayAddress> list;
    private final LayoutInflater layoutInflater;

    BTNetplayDirectAddressesAdapter(Context context, ArrayList<BTNetplayAddress> list) {
        if (list != null) {
            this.list = list;
        }
        this.layoutInflater = LayoutInflater.from(context);
    }

    @Override
    public int getCount() {
        return list.size();
    }

    @Override
    public BTNetplayAddress getItem(int position) {
        return list.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View view, ViewGroup viewGroup) {
        ViewHolder holder;
        if (view == null) {
            view = layoutInflater.inflate(R.layout.list_row_address, viewGroup, false);
            holder = new ViewHolder();
            holder.name = (TextView) view;
            view.setTag(holder);
        } else {
            holder = (ViewHolder) view.getTag();
        }

        BTNetplayAddress address = list.get(position);
        holder.name.setText(address.getAddress() + ":" + address.getPort());

        return view;
    }

    void removeItem(int position) {
        list.remove(position);
        notifyDataSetChanged();
    }

    void addItem(BTNetplayAddress address) {
        if (!list.contains(address)) {
            list.add(address);
            notifyDataSetChanged();
        }
    }

    public ArrayList<BTNetplayAddress> getItems() {
        return list;
    }

    private static class ViewHolder {
        TextView name;
    }
}

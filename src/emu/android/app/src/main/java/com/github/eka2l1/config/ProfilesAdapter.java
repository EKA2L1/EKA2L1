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

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

import com.github.eka2l1.R;

import java.util.ArrayList;
import java.util.Collections;

public class ProfilesAdapter extends BaseAdapter {
    private ArrayList<Profile> list;
    private final LayoutInflater layoutInflater;
    private Profile def;

    ProfilesAdapter(Context context, ArrayList<Profile> list) {
        if (list != null) {
            this.list = list;
            Collections.sort(list);
        }
        this.layoutInflater = LayoutInflater.from(context);
    }

    @Override
    public int getCount() {
        return list.size();
    }

    @Override
    public Profile getItem(int position) {
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
            view = layoutInflater.inflate(R.layout.list_row_profile, viewGroup, false);
            holder = new ViewHolder();
            holder.name = (TextView) view;
            view.setTag(holder);
        } else {
            holder = (ViewHolder) view.getTag();
        }

        Profile profile = list.get(position);
        String name = profile.getName();
        if (profile == def) {
            name = view.getResources().getString(R.string.default_label, name);
        }
        holder.name.setText(name);

        return view;
    }

    void removeItem(int position) {
        list.remove(position);
        notifyDataSetChanged();
    }

    public void setDefault(Profile profile) {
        def = profile;
        notifyDataSetChanged();
    }

    void addItem(Profile profile) {
        if (!list.contains(profile)) {
            list.add(profile);
            Collections.sort(list);
            notifyDataSetChanged();
        }
    }

    public Profile getDefault() {
        return def;
    }

    private static class ViewHolder {
        TextView name;
    }
}

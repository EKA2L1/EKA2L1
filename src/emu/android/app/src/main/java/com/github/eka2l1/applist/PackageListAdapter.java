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

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Filter;
import android.widget.Filterable;
import android.widget.ImageView;
import android.widget.TextView;

import com.github.eka2l1.R;

import java.util.ArrayList;
import java.util.List;


public class PackageListAdapter extends BaseAdapter implements Filterable {

    private List<AppItem> list;
    private List<AppItem> filteredList;
    private final LayoutInflater layoutInflater;
    private final AppFilter appFilter;

    public PackageListAdapter(Context context) {
        this.list = new ArrayList<>();
        this.filteredList = new ArrayList<>();
        this.layoutInflater = LayoutInflater.from(context);
        this.appFilter = new AppFilter();
    }

    @Override
    public int getCount() {
        return filteredList.size();
    }

    @Override
    public AppItem getItem(int position) {
        return filteredList.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View view, ViewGroup viewGroup) {
        ViewHolder holder;
        if (view == null) {
            view = layoutInflater.inflate(R.layout.listitem_detail_icon, viewGroup, false);
            holder = new ViewHolder();
            holder.name = view.findViewById(R.id.appName);
            holder.uid = view.findViewById(R.id.appUid);
            holder.icon = view.findViewById(R.id.iconView);
            view.setTag(holder);
        } else {
            holder = (ViewHolder) view.getTag();
        }
        AppItem item = filteredList.get(position);

        holder.name.setText(item.getTitle());
        holder.uid.setText(String.format("UID: 0x%08X", item.getUid()));

        if (item.getIcon() != null) {
            holder.icon.setImageBitmap(item.getIcon());
        } else {
            holder.icon.setImageResource(R.mipmap.ic_ducky_foreground);
        }

        return view;
    }

    public void setItems(List<AppItem> items) {
        list = items;
        filteredList = items;
        notifyDataSetChanged();
    }

    @Override
    public Filter getFilter() {
        return appFilter;
    }

    private static class ViewHolder {
        ImageView icon;
        TextView name;
        TextView uid;
    }

    private class AppFilter extends Filter {

        @Override
        protected FilterResults performFiltering(CharSequence constraint) {
            FilterResults results = new FilterResults();
            if (constraint.equals("")) {
                results.count = list.size();
                results.values = list;
            } else {
                ArrayList<AppItem> resultList = new ArrayList<>();
                for (AppItem item : list) {
                    if (item.getTitle().toLowerCase().contains(constraint)) {
                        resultList.add(item);
                    }
                }
                results.count = resultList.size();
                results.values = resultList;
            }
            return results;
        }


        @Override
        protected void publishResults(CharSequence constraint, FilterResults results) {
            if (results.values != null) {
                filteredList = (List<AppItem>) results.values;
                notifyDataSetChanged();
            } else {
                notifyDataSetInvalidated();
            }
        }
    }
}

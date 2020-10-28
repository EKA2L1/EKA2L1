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

package com.github.eka2l1.filepicker;

import android.os.Environment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.github.eka2l1.R;
import com.nononsenseapps.filepicker.FilePickerFragment;
import com.nononsenseapps.filepicker.LogicHandler;

import java.io.File;
import java.util.List;
import java.util.Stack;


public class FilteredFilePickerFragment extends FilePickerFragment {
    private static final Stack<File> history = new Stack<>();
    private static File currentDir = Environment.getExternalStorageDirectory();
    private List<String> extList;

    @NonNull
    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View v;
        switch (viewType) {
            case LogicHandler.VIEWTYPE_HEADER:
                v = LayoutInflater.from(getActivity()).inflate(R.layout.listitem_dir,
                        parent, false);
                return new HeaderViewHolder(v);
            case LogicHandler.VIEWTYPE_CHECKABLE:
                v = LayoutInflater.from(getActivity()).inflate(R.layout.listitem_checkable,
                        parent, false);
                return new CheckableViewHolder(v);
            case LogicHandler.VIEWTYPE_DIR:
            default:
                v = LayoutInflater.from(getActivity()).inflate(R.layout.listitem_dir,
                        parent, false);
                return new DirViewHolder(v);
        }
    }

    public void setExtensions(List<String> extList) {
        this.extList = extList;
    }

    private String getExtension(@NonNull File file) {
        String path = file.getPath();
        int i = path.lastIndexOf(".");
        if (i < 0) {
            return null;
        } else {
            return path.substring(i);
        }
    }

    @Override
    protected boolean isItemVisible(final File file) {
        if (!isDir(file) && (mode == MODE_FILE || mode == MODE_FILE_AND_DIR)) {
            String ext = getExtension(file);
            return ext != null && extList.contains(ext.toLowerCase());
        }
        return isDir(file);
    }

    @Override
    public void goToDir(@NonNull File file) {
        history.add(currentDir);
        currentDir = file;
        super.goToDir(file);
    }

    public static String getLastPath() {
        return currentDir.getPath();
    }

    public boolean isBackTop() {
        return history.empty();
    }

    public void goBack() {
        File last = history.pop();
        currentDir = last;
        super.goToDir(last);
    }

    @Override
    public void onBindHeaderViewHolder(@NonNull HeaderViewHolder viewHolder) {
        if (compareFiles(currentDir, getRoot()) != 0) {
            viewHolder.itemView.setEnabled(true);
            super.onBindHeaderViewHolder(viewHolder);
        } else {
            viewHolder.itemView.setEnabled(false);
        }
    }
}
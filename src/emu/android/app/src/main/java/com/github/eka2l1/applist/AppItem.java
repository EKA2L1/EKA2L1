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

import android.graphics.Bitmap;

public class AppItem {
    private long uid;
    private long extIndex;
    private String title;
    private Bitmap icon;

    public AppItem(long uid, long extIndex, String title, Bitmap icon) {
        this.uid = uid;
        this.title = title;
        this.icon = icon;
        this.extIndex = extIndex;
    }

    public long getUid() {
        return uid;
    }

    public long getExtIndex() { return extIndex; }

    public void setUid(long uid) {
        this.uid = uid;
    }

    public String getTitle() {
        return title;
    }

    public void setTitle(String title) {
        this.title = title;
    }

    public Bitmap getIcon() { return icon; }
}

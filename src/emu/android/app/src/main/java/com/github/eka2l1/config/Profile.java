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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.github.eka2l1.emu.Emulator;
import com.github.eka2l1.util.FileUtils;

import java.io.File;

public class Profile implements Comparable<Profile> {

    private String name;

    public Profile(String name) {
        this.name = name;
    }

    public void create() {
        getDir().mkdirs();
    }

    public boolean renameTo(String newName) {
        File oldDir = getDir();
        File newDir = new File(Emulator.getProfilesDir(), newName);
        name = newName;
        return oldDir.renameTo(newDir);
    }

    public void delete() {
        FileUtils.deleteDirectory(getDir());
    }

    public String getName() {
        return name;
    }

    public File getDir() {
        return new File(Emulator.getProfilesDir(), name);
    }

    public File getConfig() {
        return new File(Emulator.getProfilesDir(), name + Emulator.APP_CONFIG_FILE);
    }

    public File getKeyLayout() {
        return new File(Emulator.getProfilesDir(), name + Emulator.APP_KEY_LAYOUT_FILE);
    }

    public File getBackground() {
        return new File(Emulator.getProfilesDir(), name + Emulator.APP_BACKGROUND_IMAGE_FILE);
    }

    @Override
    public String toString() {
        return name;
    }

    @Override
    public int compareTo(@NonNull Profile o) {
        return name.toLowerCase().compareTo(o.name.toLowerCase());
    }

    boolean hasConfig() {
        return getConfig().exists();
    }

    boolean hasKeyLayout() {
        return getKeyLayout().exists();
    }
    boolean hasBackground() {
        return getBackground().exists();
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof Profile)) {
            return false;
        }
        return name.equals(((Profile) obj).name);
    }
}

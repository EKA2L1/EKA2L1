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

import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.github.eka2l1.emu.Emulator;
import com.github.eka2l1.util.FileUtils;
import com.google.gson.Gson;
import com.google.gson.GsonBuilder;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;

public class ProfilesManager {

    private static final String TAG = ProfilesManager.class.getName();
    private static final Gson gson = new GsonBuilder().setPrettyPrinting().create();

    static ArrayList<Profile> getProfiles() {
        File root = new File(Emulator.getProfilesDir());
        return getList(root);
    }

    @NonNull
    private static ArrayList<Profile> getList(File root) {
        File[] dirs = root.listFiles();
        if (dirs == null) {
            return new ArrayList<>();
        }
        int size = dirs.length;
        Profile[] profiles = new Profile[size];
        for (int i = 0; i < size; i++) {
            profiles[i] = new Profile(dirs[i].getName());
        }
        return new ArrayList<>(Arrays.asList(profiles));
    }

    static void load(Profile from, String toPath, boolean config, boolean keyboard)
            throws IOException {
        if (!config && !keyboard) {
            return;
        }
        File dstConfig = new File(toPath, Emulator.APP_CONFIG_FILE);
        File dstKeyLayout = new File(toPath, Emulator.APP_KEY_LAYOUT_FILE);
        try {
            if (config) {
                File source = from.getConfig();
                if (source.exists())
                    FileUtils.copyFileUsingChannel(source, dstConfig);
                else {
                    ProfileModel params = loadConfig(from.getDir());
                    if (params != null) {
                        params.dir = dstConfig.getParentFile();
                        saveConfig(params);
                    }
                }
            }
            if (keyboard) FileUtils.copyFileUsingChannel(from.getKeyLayout(), dstKeyLayout);
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        }
    }

    static void save(Profile profile, String fromPath, boolean config, boolean keyboard)
            throws IOException {
        if (!config && !keyboard) {
            return;
        }
        profile.create();
        File srcConfig = new File(fromPath, Emulator.APP_CONFIG_FILE);
        File srcKeyLayout = new File(fromPath, Emulator.APP_KEY_LAYOUT_FILE);
        try {
            if (config) FileUtils.copyFileUsingChannel(srcConfig, profile.getConfig());
            if (keyboard) FileUtils.copyFileUsingChannel(srcKeyLayout, profile.getKeyLayout());
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        }
    }

    @Nullable
    public static ProfileModel loadConfig(File dir) {
        File file = new File(dir, Emulator.APP_CONFIG_FILE);
        ProfileModel params = null;
        if (file.exists()) {
            try (FileReader reader = new FileReader(file)) {
                params = gson.fromJson(reader, ProfileModel.class);
                params.dir = dir;
            } catch (Exception e) {
                Log.e(TAG, "loadConfig: ", e);
            }
        }
        return params;
    }

    @NonNull
    public static ProfileModel loadConfigOrDefault(File dir, String defProfile) {
        File file = new File(dir, Emulator.APP_CONFIG_FILE);
        ProfileModel params = null;
        if (file.exists()) {
            try (FileReader reader = new FileReader(file)) {
                params = gson.fromJson(reader, ProfileModel.class);
                params.dir = dir;
            } catch (Exception e) {
                Log.e(TAG, "loadConfigOrDefault: ", e);
            }
        }
        if (params == null && defProfile != null) {
            File defFile = new File(Emulator.getProfilesDir() + defProfile, Emulator.APP_CONFIG_FILE);
            try (FileReader reader = new FileReader(defFile)) {
                params = gson.fromJson(reader, ProfileModel.class);
                params.dir = dir;
            } catch (Exception e) {
                Log.e(TAG, "loadConfigOrDefault: ", e);
            }
        }
        if (params == null) {
            params = new ProfileModel(dir);
        }
        return params;
    }

    public static void saveConfig(ProfileModel p) {
        try (FileWriter writer = new FileWriter(new File(p.dir, Emulator.APP_CONFIG_FILE))) {
            gson.toJson(p, writer);
        } catch (Exception e) {
            Log.e(TAG, "saveConfig: ", e);
        }
    }
}

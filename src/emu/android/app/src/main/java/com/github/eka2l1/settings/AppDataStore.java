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

package com.github.eka2l1.settings;

import androidx.preference.PreferenceDataStore;

import com.github.eka2l1.emu.Emulator;

import org.yaml.snakeyaml.DumperOptions;
import org.yaml.snakeyaml.Yaml;
import org.yaml.snakeyaml.nodes.Node;
import org.yaml.snakeyaml.nodes.Tag;
import org.yaml.snakeyaml.representer.Represent;
import org.yaml.snakeyaml.representer.Representer;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

public class AppDataStore extends PreferenceDataStore {
    private final Yaml yaml;
    private final File configFile;
    private Map<String, Object> configMap = new HashMap<>();

    private AppDataStore(File file) {
        DumperOptions options = new DumperOptions();
        options.setDefaultFlowStyle(DumperOptions.FlowStyle.BLOCK);
        StringRepresenter representer = new StringRepresenter();
        yaml = new Yaml(representer, options);
        configFile = file;
        try {
            FileInputStream fis = new FileInputStream(configFile);
            configMap = yaml.load(fis);
            fis.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public static AppDataStore getEmulatorStore() {
        File file = new File(Emulator.EMULATOR_DIR, "config.yml");
        return new AppDataStore(file);
    }

    public static AppDataStore getAndroidStore() {
        File file = new File(Emulator.EMULATOR_DIR, "android.yml");
        return new AppDataStore(file);
    }

    public static AppDataStore getAppStore(String uid) {
        File file = new File(Emulator.COMPAT_DIR, uid + ".yml");
        return new AppDataStore(file);
    }

    @Override
    public boolean getBoolean(String key, boolean defValue) {
        Boolean v = (Boolean) configMap.get(key);
        return v != null ? v : defValue;
    }

    @Override
    public float getFloat(String key, float defValue) {
        Float v = (Float) configMap.get(key);
        return v != null ? v : defValue;
    }

    @Override
    public int getInt(String key, int defValue) {
        Integer v = (Integer) configMap.get(key);
        return v != null ? v : defValue;
    }

    @Override
    public long getLong(String key, long defValue) {
        Long v = (Long) configMap.get(key);
        return v != null ? v : defValue;
    }

    @Override
    public String getString(String key, String defValue) {
        String v = (String) configMap.get(key);
        return v != null ? v : defValue;
    }

    @Override
    public Set<String> getStringSet(String key, Set<String> defValues) {
        Set<String> v = (Set<String>) configMap.get(key);
        return v != null ? v : defValues;
    }

    @Override
    public void putBoolean(String key, boolean value) {
        configMap.put(key, value);
    }

    @Override
    public void putFloat(String key, float value) {
        configMap.put(key, value);
    }

    @Override
    public void putInt(String key, int value) {
        configMap.put(key, value);
    }

    @Override
    public void putLong(String key, long value) {
        configMap.put(key, value);
    }

    @Override
    public void putString(String key, String value) {
        configMap.put(key, value);
    }

    @Override
    public void putStringSet(String key, Set<String> values) {
        configMap.put(key, values);
    }

    public void save() {
        try {
            FileWriter fileWriter = new FileWriter(configFile);
            yaml.dump(configMap, fileWriter);
            fileWriter.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private static class StringRepresenter extends Representer {
        public StringRepresenter() {
            this.representers.put(String.class, new RepresentString());
        }

        private class RepresentString implements Represent {
            public Node representData(Object data) {
                String str = (String) data;
                Node node;
                if (str.isEmpty()) {
                    node = representScalar(Tag.STR, str, DumperOptions.ScalarStyle.DOUBLE_QUOTED);
                } else {
                    node = representScalar(Tag.STR, str);
                }
                return node;
            }
        }
    }
}

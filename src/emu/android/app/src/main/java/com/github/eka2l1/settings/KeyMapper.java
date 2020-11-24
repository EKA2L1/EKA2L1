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

import android.util.SparseIntArray;
import android.view.KeyEvent;

import com.github.eka2l1.emu.Emulator;
import com.github.eka2l1.emu.Keycode;

import org.yaml.snakeyaml.DumperOptions;
import org.yaml.snakeyaml.Yaml;
import org.yaml.snakeyaml.constructor.Constructor;
import org.yaml.snakeyaml.nodes.Tag;
import org.yaml.snakeyaml.representer.Representer;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;

public class KeyMapper {
    static void saveArrayPref(SparseIntArray intArray) {
        ArrayList<KeyBind> keyBinds = new ArrayList<>();
        for (int i = 0; i < intArray.size(); i++) {
            KeyBind bind = new KeyBind(intArray.keyAt(i), intArray.valueAt(i));
            keyBinds.add(bind);
        }
        DumperOptions options = new DumperOptions();
        options.setDefaultFlowStyle(DumperOptions.FlowStyle.BLOCK);
        Representer representer = new Representer(options);
        representer.addClassTag(KeyBind.class, Tag.MAP);
        Yaml yaml = new Yaml(new Constructor(KeyBind.class), representer, options);
        File configFile = new File(Emulator.EMULATOR_DIR, "keybind.yml");
        try {
            FileWriter fileWriter = new FileWriter(configFile);
            yaml.dump(keyBinds, fileWriter);
            fileWriter.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
        Emulator.loadConfig();
    }

    public static SparseIntArray getArrayPref() {
        SparseIntArray intArray = new SparseIntArray();
        Yaml yaml = new Yaml();
        File configFile = new File(Emulator.EMULATOR_DIR, "keybind.yml");
        ArrayList<Object> keyBinds = null;
        try {
            FileInputStream fis = new FileInputStream(configFile);
            keyBinds = yaml.load(fis);
            fis.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
        if (keyBinds != null) {
            try {
                for (Object obj : keyBinds) {
                    String str = yaml.dump(obj);
                    KeyBind bind = yaml.loadAs(str, KeyBind.class);
                    intArray.append(bind.source.data.keycode, bind.target);
                }
            } catch (Exception e) {
                e.printStackTrace();
                initArray(intArray);
            }
        } else {
            initArray(intArray);
        }
        return intArray;
    }

    static void initArray(SparseIntArray intDict) {
        intDict.put(KeyEvent.KEYCODE_0, Keycode.KEY_NUM0);
        intDict.put(KeyEvent.KEYCODE_1, Keycode.KEY_NUM1);
        intDict.put(KeyEvent.KEYCODE_2, Keycode.KEY_NUM2);
        intDict.put(KeyEvent.KEYCODE_3, Keycode.KEY_NUM3);
        intDict.put(KeyEvent.KEYCODE_4, Keycode.KEY_NUM4);
        intDict.put(KeyEvent.KEYCODE_5, Keycode.KEY_NUM5);
        intDict.put(KeyEvent.KEYCODE_6, Keycode.KEY_NUM6);
        intDict.put(KeyEvent.KEYCODE_7, Keycode.KEY_NUM7);
        intDict.put(KeyEvent.KEYCODE_8, Keycode.KEY_NUM8);
        intDict.put(KeyEvent.KEYCODE_9, Keycode.KEY_NUM9);
        intDict.put(KeyEvent.KEYCODE_STAR, Keycode.KEY_STAR);
        intDict.put(KeyEvent.KEYCODE_POUND, Keycode.KEY_POUND);
        intDict.put(KeyEvent.KEYCODE_DPAD_UP, Keycode.KEY_UP);
        intDict.put(KeyEvent.KEYCODE_DPAD_DOWN, Keycode.KEY_DOWN);
        intDict.put(KeyEvent.KEYCODE_DPAD_LEFT, Keycode.KEY_LEFT);
        intDict.put(KeyEvent.KEYCODE_DPAD_RIGHT, Keycode.KEY_RIGHT);
        intDict.put(KeyEvent.KEYCODE_ENTER, Keycode.KEY_FIRE);
        intDict.put(KeyEvent.KEYCODE_SOFT_LEFT, Keycode.KEY_SOFT_LEFT);
        intDict.put(KeyEvent.KEYCODE_SOFT_RIGHT, Keycode.KEY_SOFT_RIGHT);
        intDict.put(KeyEvent.KEYCODE_CALL, Keycode.KEY_SEND);
        intDict.put(KeyEvent.KEYCODE_ENDCALL, Keycode.KEY_END);
    }

    private static class KeyBind {
        Source source;
        int target;

        KeyBind() {
        }

        KeyBind(int keycode, int target) {
            this.source = new Source(keycode);
            this.target = target;
        }
    }

    private static class Source {
        String type;
        Data data;

        Source() {
        }

        Source(int keycode) {
            this.type = "key";
            this.data = new Data(keycode);
        }
    }

    private static class Data {
        int keycode;

        Data() {
        }

        Data(int keycode) {
            this.keycode = keycode;
        }
    }
}

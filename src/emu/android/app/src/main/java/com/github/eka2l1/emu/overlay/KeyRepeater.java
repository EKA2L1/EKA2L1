/*
 * Copyright 2012 Kulikov Dmitriy
 * Copyright 2018 Nikita Shakarun
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.github.eka2l1.emu.overlay;

import com.github.eka2l1.emu.Emulator;
import com.github.eka2l1.emu.overlay.VirtualKeyboard.VirtualKey;

import java.util.HashSet;

public class KeyRepeater implements Runnable {
    private static final long INTERVAL = 150;
    private static final long INITIAL_INTERVAL = 250;

    private Thread thread;
    private final Object waiter;
    private final HashSet<VirtualKey> keys;

    private boolean enabled;

    public KeyRepeater() {
        keys = new HashSet<>();
        waiter = new Object();

        thread = new Thread(this, "MIDletKeyRepeater");
        thread.start();
    }

    public void add(VirtualKey key) {
        synchronized (keys) {
            keys.add(key);
            enabled = true;
        }

        synchronized (waiter) {
            waiter.notifyAll();
        }
    }

    public void remove(VirtualKey key) {
        synchronized (keys) {
            keys.remove(key);

            if (keys.isEmpty()) {
                enabled = false;
            }
        }
    }

    @Override
    public void run() {
        while (true) {
            try {
                synchronized (waiter) {
                    waiter.wait();
                }

                Thread.sleep(INITIAL_INTERVAL);

                while (enabled) {
                    Thread.sleep(INTERVAL);

                    synchronized (keys) {
                        for (VirtualKey key : keys) {
                            Emulator.pressKey(key.getKeyCode(), 3);
                            if (key.getSecondKeyCode() != 0) {
                                Emulator.pressKey(key.getSecondKeyCode(), 3);
                            }
                        }
                    }
                }
            } catch (InterruptedException ie) {
                // Don't need to print stacktrace here
                System.out.println();
            }
        }
    }
}

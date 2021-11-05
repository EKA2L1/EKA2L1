/*
 * Copyright (c) 2021 EKA2L1 Team
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

package com.github.eka2l1.emu.overlay;

import android.content.Context;
import android.graphics.RectF;

import com.github.eka2l1.config.ProfileModel;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class FixedKeyboard extends VirtualKeyboard {

	public final static float KEY_WIDTH_RATIO = 3;
	public final static float KEY_HEIGHT_RATIO = 2.7f;

	private static final int NUM_VARIANTS = 2;

	public FixedKeyboard(Context context) {
		super(context);
	}

	@Override
	protected void resetLayout(int variant) {
		switch (variant) {
			case 0:
				setSnap(KEY_NUM0, SCREEN, RectSnap.INT_SOUTH);
				setSnap(KEY_STAR, KEY_NUM0, RectSnap.EXT_WEST);
				setSnap(KEY_POUND, KEY_NUM0, RectSnap.EXT_EAST);
				setSnap(KEY_NUM7, KEY_STAR, RectSnap.EXT_NORTH);
				setSnap(KEY_NUM8, KEY_NUM7, RectSnap.EXT_EAST);
				setSnap(KEY_NUM9, KEY_NUM8, RectSnap.EXT_EAST);
				setSnap(KEY_NUM4, KEY_NUM7, RectSnap.EXT_NORTH);
				setSnap(KEY_NUM5, KEY_NUM4, RectSnap.EXT_EAST);
				setSnap(KEY_NUM6, KEY_NUM5, RectSnap.EXT_EAST);
				setSnap(KEY_NUM1, KEY_NUM4, RectSnap.EXT_NORTH);
				setSnap(KEY_NUM2, KEY_NUM1, RectSnap.EXT_EAST);
				setSnap(KEY_NUM3, KEY_NUM2, RectSnap.EXT_EAST);

				setSnap(KEY_SOFT_LEFT, KEY_NUM1, RectSnap.EXT_NORTH);
				setSnap(KEY_FIRE, KEY_NUM2, RectSnap.EXT_NORTH);
				setSnap(KEY_SOFT_RIGHT, KEY_NUM3, RectSnap.EXT_NORTH);

				for (int i = KEY_NUM1; i < KEY_DIAL; i++) {
					keypad[i].setVisible(true);
				}
				for (int i = KEY_DIAL; i < KEY_FIRE; i++) {
					keypad[i].setVisible(false);
				}
				break;
			case 1:
				setSnap(KEY_NUM0, SCREEN, RectSnap.INT_SOUTH);
				setSnap(KEY_STAR, KEY_NUM0, RectSnap.EXT_WEST);
				setSnap(KEY_POUND, KEY_NUM0, RectSnap.EXT_EAST);
				setSnap(KEY_NUM7, KEY_STAR, RectSnap.EXT_NORTH);
				setSnap(KEY_DOWN, KEY_NUM7, RectSnap.EXT_EAST);
				setSnap(KEY_NUM9, KEY_DOWN, RectSnap.EXT_EAST);
				setSnap(KEY_LEFT, KEY_NUM7, RectSnap.EXT_NORTH);
				setSnap(KEY_NUM5, KEY_LEFT, RectSnap.EXT_EAST);
				setSnap(KEY_RIGHT, KEY_NUM5, RectSnap.EXT_EAST);
				setSnap(KEY_NUM1, KEY_LEFT, RectSnap.EXT_NORTH);
				setSnap(KEY_UP, KEY_NUM1, RectSnap.EXT_EAST);
				setSnap(KEY_NUM3, KEY_UP, RectSnap.EXT_EAST);

				setSnap(KEY_SOFT_LEFT, KEY_NUM1, RectSnap.EXT_NORTH);
				setSnap(KEY_FIRE, KEY_UP, RectSnap.EXT_NORTH);
				setSnap(KEY_SOFT_RIGHT, KEY_NUM3, RectSnap.EXT_NORTH);

				for (int i = KEY_NUM1; i < KEY_NUM9; i += 2) {
					keypad[i].setVisible(true);
					keypad[i + 1].setVisible(false);
				}
				for (int i = KEY_NUM9; i < KEY_DIAL; i++) {
					keypad[i].setVisible(true);
				}
				keypad[KEY_DIAL].setVisible(false);
				keypad[KEY_CANCEL].setVisible(false);
				keypad[KEY_UP_LEFT].setVisible(false);
				keypad[KEY_UP].setVisible(true);
				keypad[KEY_UP_RIGHT].setVisible(false);
				keypad[KEY_LEFT].setVisible(true);
				keypad[KEY_RIGHT].setVisible(true);
				keypad[KEY_DOWN_LEFT].setVisible(false);
				keypad[KEY_DOWN].setVisible(true);
				keypad[KEY_DOWN_RIGHT].setVisible(false);
				keypad[KEY_FIRE].setVisible(true);
				break;
		}
	}

	@Override
	protected int getLayoutNum() {
		return NUM_VARIANTS;
	}

	@Override
	public void setLayout(int layoutVariant) {
		resetLayout(layoutVariant);
		snapKeys();
		repaint();
	}

	@Override
	public void resize(RectF screen, RectF virtualScreen) {
		this.screen = screen;
		this.virtualScreen = virtualScreen;
		float keyWidth = screen.width() / KEY_WIDTH_RATIO;
		float keyHeight = keyWidth / KEY_HEIGHT_RATIO;
		for (int i = 0; i < keypad.length; i++) {
			keypad[i].resize(keyWidth, keyHeight);
			snapValid[i] = false;
		}
		snapKeys();
		repaint();
	}

	@Override
	public void readLayout(DataInputStream dis) throws IOException {
	}

	@Override
	public void writeLayout(DataOutputStream dos) throws IOException {
	}
}

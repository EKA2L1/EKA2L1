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

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.EditTextPreference;

public class LongEditTextPreference extends EditTextPreference {

    public LongEditTextPreference(Context context) {
        super(context);
    }

    public LongEditTextPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public LongEditTextPreference(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected String getPersistedString(String defaultReturnValue) {
        long defaultValue = Long.parseLong(defaultReturnValue);
        long result = getPersistedLong(defaultValue);
        return String.valueOf(result);
    }

    @Override
    protected boolean persistString(String value) {
        long result = 0;
        try {
            result = Long.parseLong(value);
        } catch (NumberFormatException e) {
            e.printStackTrace();
        }
        return persistLong(result);
    }
}

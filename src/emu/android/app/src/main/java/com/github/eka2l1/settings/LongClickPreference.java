/*
 * Copyright (c) 2025 EKA2L1 Team
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

import androidx.annotation.NonNull;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

public class LongClickPreference extends Preference {
    public interface OnPreferenceLongClickListener {
        boolean onPreferenceLongClick(Preference preference);
    }

    private OnPreferenceLongClickListener longClickListener;

    public LongClickPreference(@NonNull Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
    }

    public LongClickPreference(@NonNull Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public LongClickPreference(@NonNull Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public LongClickPreference(@NonNull Context context) {
        super(context);
    }

    public void setOnPreferenceLongClickListener(OnPreferenceLongClickListener listener) {
        longClickListener = listener;
        notifyChanged();
    }

    @Override
    public void onBindViewHolder(@NonNull PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        holder.itemView.setLongClickable(longClickListener != null);
        holder.itemView.setOnLongClickListener(longClickListener == null ? null
                : v -> longClickListener.onPreferenceLongClick(this));
    }
}

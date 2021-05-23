package com.github.eka2l1.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.ListPreference;

public class IntListPreference extends ListPreference {

    public IntListPreference(Context context) {
        super(context);
    }

    public IntListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public IntListPreference(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected String getPersistedString(String defaultReturnValue) {
        int defaultValue = Integer.parseInt(defaultReturnValue);
        int result = getPersistedInt(defaultValue);
        return String.valueOf(result);
    }

    @Override
    protected boolean persistString(String value) {
        int result = 0;
        try {
            result = Integer.parseInt(value);
        } catch (NumberFormatException e) {
            e.printStackTrace();
        }
        return persistInt(result);
    }
}

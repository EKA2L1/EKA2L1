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

import android.annotation.SuppressLint;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputFilter;
import android.text.Spanned;
import android.text.TextWatcher;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.fragment.app.Fragment;

import com.github.eka2l1.R;
import com.github.eka2l1.emu.Emulator;
import com.github.eka2l1.emu.EmulatorActivity;
import com.github.eka2l1.settings.AppDataStore;
import com.github.eka2l1.settings.KeyMapperFragment;
import com.github.eka2l1.util.FileUtils;

import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import yuku.ambilwarna.AmbilWarnaDialog;

import static com.github.eka2l1.emu.Constants.*;

public class ConfigFragment extends Fragment implements View.OnClickListener {

    protected ScrollView rootContainer;
    protected EditText etScreenRefreshRate;
    protected EditText etSystemTimeDelay;
    protected CompoundButton cbShouldChildInherit;

    protected EditText etScreenBack;
    protected SeekBar sbScaleRatio;
    protected EditText etScaleRatioValue;
    protected Spinner spOrientation;
    protected Spinner spScreenGravity;
    protected Spinner spScaleType;
    protected TextView tvUpscaleShader;
    protected Spinner spUpscaleShader;
    protected CompoundButton cbUseShaderForUpscale;

    protected CompoundButton cbShowKeyboard;

    private View rootInputConfig;
    private View groupVkConfig;
    protected CompoundButton cbVKFeedback;
    protected CompoundButton cbTouchInput;

    private Spinner spVKType;
    private Spinner spButtonsShape;
    protected SeekBar sbVKAlpha;
    protected EditText etVKHideDelay;
    protected EditText etVKFore;
    protected EditText etVKBack;
    protected EditText etVKSelFore;
    protected EditText etVKSelBack;
    protected EditText etVKOutline;

    private File keylayoutFile;
    private ProfileModel params;
    private boolean isProfile;
    private File configDir;
    private String defProfile;
    private boolean needShow;
    private AppDataStore dataStore;
    private long uid;
    private boolean compatChanged;

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        setHasOptionsMenu(true);
        ActionBar actionBar = ((AppCompatActivity) getActivity()).getSupportActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);

        Bundle args = getArguments();
        String action = args.getString(KEY_ACTION, "");
        isProfile = ACTION_EDIT_PROFILE.equals(action);
        needShow = isProfile || ACTION_EDIT.equals(action);
        if (isProfile) {
            String path = args.getString(KEY_PROFILE_NAME, "");
            boolean create = args.getBoolean(KEY_PROFILE_CREATE);
            if (create) {
                Bundle result = new Bundle();
                result.putString(KEY_NAME, path);
                getParentFragmentManager().setFragmentResult(KEY_PROFILE_CREATED, result);
            }

            configDir = new File(Emulator.getProfilesDir(), path);
            actionBar.setTitle(path);
            ConstraintLayout systemPropertiesLayout = view.findViewById(R.id.rootSystemProperties);
            systemPropertiesLayout.setVisibility(View.GONE);
        } else {
            uid = args.getLong(KEY_APP_UID, -1);
            String uidStr = Long.toHexString(uid).toUpperCase();
            actionBar.setTitle(args.getString(KEY_APP_NAME));
            configDir = new File(Emulator.getConfigsDir(), uidStr);
            dataStore = AppDataStore.getAppStore(uidStr);
        }
        configDir.mkdirs();

        defProfile = AppDataStore.getAndroidStore().getString(PREF_DEFAULT_PROFILE, null);
        params = ProfilesManager.loadConfigOrDefault(configDir, defProfile);
        if (!params.isNew && !needShow) {
            startApp();
            getParentFragmentManager().popBackStackImmediate();
            return;
        }
        loadKeyLayout();
        getParentFragmentManager().setFragmentResultListener(KEY_PROFILE_LOADED, this, (requestKey, bundle) -> {
            loadParams(true);
        });
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_config, container, false);
        rootContainer = view.findViewById(R.id.configRoot);
        etScreenRefreshRate = view.findViewById(R.id.etScreenRefreshRate);
        etSystemTimeDelay = view.findViewById(R.id.etSystemTimeDelay);
        cbShouldChildInherit = view.findViewById(R.id.cbShouldChildInherit);

        etScreenBack = view.findViewById(R.id.etScreenBack);
        spScreenGravity = view.findViewById(R.id.spScreenGravity);
        spScaleType = view.findViewById(R.id.spScaleType);
        sbScaleRatio = view.findViewById(R.id.sbScaleRatio);
        etScaleRatioValue = view.findViewById(R.id.etScaleRatioValue);
        spOrientation = view.findViewById(R.id.spOrientation);
        tvUpscaleShader = view.findViewById(R.id.tvUpscaleShader);
        spUpscaleShader = view.findViewById(R.id.spUpscaleShader);
        cbUseShaderForUpscale = view.findViewById(R.id.cbShouldUseShaderForUpscale);

        rootInputConfig = view.findViewById(R.id.rootInputConfig);
        cbTouchInput = view.findViewById(R.id.cbTouchInput);
        cbShowKeyboard = view.findViewById(R.id.cbIsShowKeyboard);
        groupVkConfig = view.findViewById(R.id.groupVkConfig);
        cbVKFeedback = view.findViewById(R.id.cbVKFeedback);

        spVKType = view.findViewById(R.id.spVKType);
        spButtonsShape = view.findViewById(R.id.spButtonsShape);
        sbVKAlpha = view.findViewById(R.id.sbVKAlpha);
        etVKHideDelay = view.findViewById(R.id.etVKHideDelay);
        etVKFore = view.findViewById(R.id.etVKFore);
        etVKBack = view.findViewById(R.id.etVKBack);
        etVKSelFore = view.findViewById(R.id.etVKSelFore);
        etVKSelBack = view.findViewById(R.id.etVKSelBack);
        etVKOutline = view.findViewById(R.id.etVKOutline);

        view.findViewById(R.id.cmdScreenBack).setOnClickListener(this);
        view.findViewById(R.id.cmdKeyMappings).setOnClickListener(this);
        view.findViewById(R.id.cmdVKBack).setOnClickListener(this);
        view.findViewById(R.id.cmdVKFore).setOnClickListener(this);
        view.findViewById(R.id.cmdVKSelBack).setOnClickListener(this);
        view.findViewById(R.id.cmdVKSelFore).setOnClickListener(this);
        view.findViewById(R.id.cmdVKOutline).setOnClickListener(this);

        // Load shaders
        File upscaleShaderDir = new File(Emulator.getEmulatorDir() + "resources/upscale");
        File []upscaleShaderFiles = upscaleShaderDir.listFiles(file -> file.getPath().endsWith(".frag"));

        List<String> upscaleShaderNames = new ArrayList<>();
        upscaleShaderNames.add(getString(R.string.pref_screen_default_shader));

        if (upscaleShaderFiles != null) {
            for (File upscaleShaderFile: upscaleShaderFiles) {
                String singleName = upscaleShaderFile.getName();
                singleName = singleName.substring(0, singleName.length() - 5);
                upscaleShaderNames.add(singleName);
            }
        }

        ArrayAdapter<String> adapter = new ArrayAdapter<>(getContext(),
                android.R.layout.simple_spinner_dropdown_item,
                upscaleShaderNames);

        spUpscaleShader.setAdapter(adapter);

        etScreenRefreshRate.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                if (etScreenRefreshRate.isFocused()) compatChanged = true;
            }
        });
        etSystemTimeDelay.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                if (etSystemTimeDelay.isFocused()) compatChanged = true;
            }
        });
        cbShouldChildInherit.setOnCheckedChangeListener((buttonView, isChecked) -> compatChanged = true);
        sbScaleRatio.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) etScaleRatioValue.setText(String.valueOf(progress));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });
        etScaleRatioValue.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                int length = s.length();
                if (length > 3) {
                    if (start >= 3) {
                        etScaleRatioValue.getText().delete(3, length);
                    } else {
                        int st = start + count;
                        int end = st + (before == 0 ? count : before);
                        etScaleRatioValue.getText().delete(st, Math.min(end, length));
                    }
                }
            }

            @Override
            public void afterTextChanged(Editable s) {
                if (s.length() == 0) return;
                try {
                    int progress = Integer.parseInt(s.toString());
                    if (progress <= 100) {
                        sbScaleRatio.setProgress(progress);
                    } else {
                        s.replace(0, s.length(), "100");
                    }
                } catch (NumberFormatException e) {
                    s.clear();
                }
            }
        });
        cbUseShaderForUpscale.setOnCheckedChangeListener((buttonView, isChecked) -> {
            if (isChecked) {
                tvUpscaleShader.setVisibility(View.VISIBLE);
                spUpscaleShader.setVisibility(View.VISIBLE);
            } else {
                tvUpscaleShader.setVisibility(View.GONE);
                spUpscaleShader.setVisibility(View.GONE);
            }

            compatChanged = true;
        });
        spUpscaleShader.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                compatChanged = true;
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
            }
        });
        cbShowKeyboard.setOnClickListener((b) -> {
            View.OnLayoutChangeListener onLayoutChangeListener = new View.OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft, int oldTop, int oldRight, int oldBottom) {
                    View focus = rootContainer.findFocus();
                    if (focus != null) focus.clearFocus();
                    v.scrollTo(0, rootInputConfig.getTop());
                    v.removeOnLayoutChangeListener(this);
                }
            };
            rootContainer.addOnLayoutChangeListener(onLayoutChangeListener);
            groupVkConfig.setVisibility(cbShowKeyboard.isChecked() ? View.VISIBLE : View.GONE);
        });
        etScreenBack.addTextChangedListener(new ColorTextWatcher(etScreenBack));
        etVKFore.addTextChangedListener(new ColorTextWatcher(etVKFore));
        etVKBack.addTextChangedListener(new ColorTextWatcher(etVKBack));
        etVKSelFore.addTextChangedListener(new ColorTextWatcher(etVKSelFore));
        etVKSelBack.addTextChangedListener(new ColorTextWatcher(etVKSelBack));
        etVKOutline.addTextChangedListener(new ColorTextWatcher(etVKOutline));
        return view;
    }

    private void loadKeyLayout() {
        File file = new File(configDir, Emulator.APP_KEY_LAYOUT_FILE);
        keylayoutFile = file;
        if (isProfile || file.exists()) {
            return;
        }
        if (defProfile == null) {
            return;
        }
        File defaultKeyLayoutFile = new File(Emulator.getProfilesDir() + defProfile, Emulator.APP_KEY_LAYOUT_FILE);
        if (!defaultKeyLayoutFile.exists()) {
            return;
        }
        try {
            FileUtils.copyFileUsingChannel(defaultKeyLayoutFile, file);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onPause() {
        if (needShow && configDir != null) {
            saveParams();
        }
        super.onPause();
    }

    @Override
    public void onResume() {
        super.onResume();
        if (needShow) {
            loadParams(true);
        }
    }

    private int parseInt(String s) {
        return parseInt(s, 10);
    }

    private int parseInt(String s, int radix) {
        int result;
        try {
            result = Integer.parseInt(s, radix);
        } catch (NumberFormatException e) {
            result = 0;
        }
        return result;
    }

    @SuppressLint("SetTextI18n")
    public void loadParams(boolean reloadFromFile) {
        if (reloadFromFile) {
            params = ProfilesManager.loadConfigOrDefault(configDir, defProfile);
        }
        if (!isProfile) {
            etScreenRefreshRate.setText(dataStore.getString("fps", "60"));
            etSystemTimeDelay.setText(dataStore.getString("time-delay", "0"));
            cbShouldChildInherit.setChecked(dataStore.getBoolean("should-child-inherit-setting", true));

            cbUseShaderForUpscale.setChecked(dataStore.getString("screen-upscale-method", "0") != "0");
            if (!cbUseShaderForUpscale.isChecked()) {
                tvUpscaleShader.setVisibility(View.GONE);
                spUpscaleShader.setVisibility(View.GONE);
            } else {
                tvUpscaleShader.setVisibility(View.VISIBLE);
                spUpscaleShader.setVisibility(View.VISIBLE);
            }

            String shaderName = dataStore.getString("filter-shader-path", "");
            spUpscaleShader.setSelection(0);
            if (!shaderName.isEmpty()) {
                for (int i = 1; i < spUpscaleShader.getCount(); i++) {
                    String spinnerShaderName = (String)spUpscaleShader.getItemAtPosition(i);
                    if (spinnerShaderName.equals(shaderName)) {
                        spUpscaleShader.setSelection(i);
                        break;
                    }
                }
            }
        }

        etScreenBack.setText(String.format("%06X", params.screenBackgroundColor));
        sbScaleRatio.setProgress(params.screenScaleRatio);
        etScaleRatioValue.setText(Integer.toString(params.screenScaleRatio));
        spOrientation.setSelection(params.orientation);
        spScaleType.setSelection(params.screenScaleType);
        spScreenGravity.setSelection(params.screenGravity);

        boolean showVk = params.showKeyboard;
        cbShowKeyboard.setChecked(showVk);
        groupVkConfig.setVisibility(showVk ? View.VISIBLE : View.GONE);
        cbVKFeedback.setChecked(params.vkFeedback);
        cbTouchInput.setChecked(params.touchInput);

        spVKType.setSelection(params.vkType);
        spButtonsShape.setSelection(params.vkButtonShape);
        sbVKAlpha.setProgress(params.vkAlpha);
        int vkHideDelay = params.vkHideDelay;
        if (vkHideDelay > 0) {
            etVKHideDelay.setText(Integer.toString(vkHideDelay));
        }

        etVKBack.setText(String.format("%06X", params.vkBgColor));
        etVKFore.setText(String.format("%06X", params.vkFgColor));
        etVKSelBack.setText(String.format("%06X", params.vkBgColorSelected));
        etVKSelFore.setText(String.format("%06X", params.vkFgColorSelected));
        etVKOutline.setText(String.format("%06X", params.vkOutlineColor));
    }

    private void saveParams() {
        try {
            try {
                params.screenBackgroundColor = Integer.parseInt(etScreenBack.getText().toString(), 16);
            } catch (NumberFormatException ignored) {
            }
            params.screenScaleRatio = sbScaleRatio.getProgress();
            params.orientation = spOrientation.getSelectedItemPosition();
            params.screenGravity = spScreenGravity.getSelectedItemPosition();
            params.screenScaleType = spScaleType.getSelectedItemPosition();

            params.showKeyboard = cbShowKeyboard.isChecked();
            params.vkFeedback = cbVKFeedback.isChecked();
            params.touchInput = cbTouchInput.isChecked();

            params.vkType = spVKType.getSelectedItemPosition();
            params.vkButtonShape = spButtonsShape.getSelectedItemPosition();
            params.vkAlpha = sbVKAlpha.getProgress();
            params.vkHideDelay = parseInt(etVKHideDelay.getText().toString());
            try {
                params.vkBgColor = Integer.parseInt(etVKBack.getText().toString(), 16);
            } catch (Exception ignored) {
            }
            try {
                params.vkFgColor = Integer.parseInt(etVKFore.getText().toString(), 16);
            } catch (Exception ignored) {
            }
            try {
                params.vkBgColorSelected = Integer.parseInt(etVKSelBack.getText().toString(), 16);
            } catch (Exception ignored) {
            }
            try {
                params.vkFgColorSelected = Integer.parseInt(etVKSelFore.getText().toString(), 16);
            } catch (Exception ignored) {
            }
            try {
                params.vkOutlineColor = Integer.parseInt(etVKOutline.getText().toString(), 16);
            } catch (Exception ignored) {
            }
            ProfilesManager.saveConfig(params);

            if (!isProfile && compatChanged) {
                dataStore.putString("fps", etScreenRefreshRate.getText().toString());
                dataStore.putString("time-delay", etSystemTimeDelay.getText().toString());
                dataStore.putBoolean("should-child-inherit-setting", cbShouldChildInherit.isChecked());
                dataStore.putString("screen-upscale-method", cbUseShaderForUpscale.isChecked() ? "1" : "0");
                String toSaveFilterName = "Default";
                if (spUpscaleShader.getSelectedItemPosition() != 0) {
                    toSaveFilterName = (String)spUpscaleShader.getSelectedItem();
                }
                dataStore.putString("filter-shader-path", toSaveFilterName);
                dataStore.save();
                Emulator.updateAppSetting((int) uid);
            }
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        super.onCreateOptionsMenu(menu, inflater);
        inflater.inflate(R.menu.config, menu);
        if (isProfile) {
            menu.findItem(R.id.action_start).setVisible(false);
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int itemId = item.getItemId();
        if (itemId == R.id.action_start) {
            startApp();
        } else if (itemId == R.id.action_reset_settings) {
            params = new ProfileModel(configDir);
            loadParams(false);
        } else if (itemId == R.id.action_reset_layout) {//noinspection ResultOfMethodCallIgnored
            keylayoutFile.delete();
            loadKeyLayout();
        } else if (itemId == R.id.action_load_profile) {
            LoadProfileAlert.newInstance(keylayoutFile.getParent())
                    .show(getParentFragmentManager(), "load_profile");
        } else if (itemId == R.id.action_save_profile) {
            saveParams();
            SaveProfileAlert.getInstance(keylayoutFile.getParent())
                    .show(getParentFragmentManager(), "save_profile");
        } else if (itemId == android.R.id.home) {
            getParentFragmentManager().popBackStackImmediate();
        }
        return super.onOptionsItemSelected(item);
    }

    private void startApp() {
        Intent intent = new Intent(getContext(), EmulatorActivity.class);
        intent.putExtras(requireArguments());
        startActivity(intent);
    }

    @SuppressLint("SetTextI18n")
    @Override
    public void onClick(View v) {
        int id = v.getId();
        if (id == R.id.cmdScreenBack) {
            showColorPicker(etScreenBack);
        } else if (id == R.id.cmdVKBack) {
            showColorPicker(etVKBack);
        } else if (id == R.id.cmdVKFore) {
            showColorPicker(etVKFore);
        } else if (id == R.id.cmdVKSelFore) {
            showColorPicker(etVKSelFore);
        } else if (id == R.id.cmdVKSelBack) {
            showColorPicker(etVKSelBack);
        } else if (id == R.id.cmdVKOutline) {
            showColorPicker(etVKOutline);
        } else if (id == R.id.cmdKeyMappings) {
            KeyMapperFragment keyMapperFragment = new KeyMapperFragment();
            Bundle args = new Bundle();
            args.putString(KEY_CONFIG_PATH, configDir.getAbsolutePath());
            keyMapperFragment.setArguments(args);
            getParentFragmentManager().beginTransaction()
                    .replace(R.id.container, keyMapperFragment)
                    .addToBackStack(null)
                    .commit();
        }
    }

    private void showColorPicker(EditText et) {
        AmbilWarnaDialog.OnAmbilWarnaListener colorListener = new AmbilWarnaDialog.OnAmbilWarnaListener() {
            @Override
            public void onOk(AmbilWarnaDialog dialog, int color) {
                et.setText(String.format("%06X", color & 0xFFFFFF));
                ColorDrawable drawable = (ColorDrawable) et.getCompoundDrawablesRelative()[2];
                drawable.setColor(color);
            }

            @Override
            public void onCancel(AmbilWarnaDialog dialog) {
            }
        };
        int color = parseInt(et.getText().toString().trim(), 16);
        new AmbilWarnaDialog(getContext(), color | 0xFF000000, colorListener).show();
    }

    private static class ColorTextWatcher implements TextWatcher {
        private final EditText editText;
        private final ColorDrawable drawable;

        ColorTextWatcher(EditText editText) {
            this.editText = editText;
            int size = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 32,
                    editText.getResources().getDisplayMetrics());
            ColorDrawable colorDrawable = new ColorDrawable();
            colorDrawable.setBounds(0, 0, size, size);
            editText.setCompoundDrawablesRelative(null, null, colorDrawable, null);
            drawable = colorDrawable;
            editText.setFilters(new InputFilter[]{this::filter});
        }

        private CharSequence filter(CharSequence src, int ss, int se, Spanned dst, int ds, int de) {
            StringBuilder sb = new StringBuilder(se - ss);
            for (int i = ss; i < se; i++) {
                char c = src.charAt(i);
                if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) {
                    sb.append(c);
                } else if (c >= 'a' && c <= 'f') {
                    sb.append((char) (c - 32));
                }
            }
            return sb;
        }

        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) {
        }

        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
            if (s.length() > 6) {
                if (start >= 6) editText.getText().delete(6, s.length());
                else {
                    int st = start + count;
                    int end = st + (before == 0 ? count : before);
                    editText.getText().delete(st, Math.min(end, s.length()));
                }
            }
        }

        @Override
        public void afterTextChanged(Editable s) {
            if (s.length() == 0) return;
            try {
                int color = Integer.parseInt(s.toString(), 16);
                drawable.setColor(color | Color.BLACK);
            } catch (NumberFormatException e) {
                drawable.setColor(Color.BLACK);
                s.clear();
            }
        }
    }
}

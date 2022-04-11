/*
 * Copyright (c) 2020 EKA2L1 Team
 * Copyright (c) 2019 Kharchenko Yury
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

package com.github.eka2l1.emu;

import static com.github.eka2l1.emu.Constants.*;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Bundle;
import android.os.Process;
import android.util.Log;
import android.util.SparseIntArray;
import android.view.Display;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;

import com.github.eka2l1.R;
import com.github.eka2l1.config.ProfileModel;
import com.github.eka2l1.config.ProfilesManager;
import com.github.eka2l1.emu.overlay.FixedKeyboard;
import com.github.eka2l1.emu.overlay.OverlayView;
import com.github.eka2l1.emu.overlay.VirtualKeyboard;
import com.github.eka2l1.settings.AppDataStore;
import com.github.eka2l1.settings.KeyMapper;
import com.github.eka2l1.util.LogUtils;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Objects;

public class EmulatorActivity extends AppCompatActivity {
    private static final int ORIENTATION_DEFAULT = 0;
    private static final int ORIENTATION_AUTO = 1;
    private static final int ORIENTATION_PORTRAIT = 2;
    private static final int ORIENTATION_LANDSCAPE = 3;

    private Toolbar toolbar;
    private OverlayView overlayView;
    private long uid;
    private boolean launched;
    private boolean statusBarEnabled;
    private boolean actionBarEnabled;
    private VirtualKeyboard keyboard;
    private float displayWidth;
    private float displayHeight;
    private SparseIntArray androidToSymbian;
    private ProfileModel params;

    @SuppressLint("ClickableViewAccessibility")
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        AppDataStore dataStore = AppDataStore.getAndroidStore();
        setTheme(dataStore.getString(PREF_THEME, "light"));
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_emulator);
        overlayView = findViewById(R.id.overlay);

        SurfaceView surfaceView = findViewById(R.id.surface_view);
        ViewCallbacks callbacks = new ViewCallbacks(surfaceView);
        surfaceView.setFocusableInTouchMode(true);
        surfaceView.setWillNotDraw(true);
        surfaceView.setOnTouchListener(callbacks);
        surfaceView.setOnKeyListener(callbacks);
        surfaceView.getHolder().addCallback(callbacks);
        surfaceView.requestFocus();

        toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        boolean wakelockEnabled = dataStore.getBoolean(PREF_KEEP_SCREEN, false);
        if (wakelockEnabled) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
        actionBarEnabled = dataStore.getBoolean(PREF_ACTIONBAR, true);
        statusBarEnabled = dataStore.getBoolean(PREF_STATUSBAR, false);
        if (!actionBarEnabled) {
            getSupportActionBar().hide();
        }

        Intent intent = getIntent();
        if (intent.getBooleanExtra(KEY_APP_IS_SHORTCUT, false)) {
            Emulator.initializeForShortcutLaunch(this);
        } else {
            Emulator.setContext(this);
        }

        uid = intent.getLongExtra(KEY_APP_UID, -1);

        String name = intent.getStringExtra(KEY_APP_NAME);
        String deviceCode = intent.getStringExtra(KEY_DEVICE_CODE);

        if (deviceCode != null) {
            String []availableDevices = Emulator.getDeviceFirmwareCodes();
            for (int id = 0; id < availableDevices.length; id++) {
                if (availableDevices[id].compareToIgnoreCase(deviceCode) == 0) {
                    Emulator.setCurrentDevice(id, true);
                    break;
                }
            }
        }

        setActionBar(name);
        hideSystemUI();

        Display display = ((WindowManager) getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();
        displayWidth = display.getWidth();
        displayHeight = display.getHeight();

        String uidStr = Long.toHexString(uid).toUpperCase();
        File configDir = new File(Emulator.getConfigsDir(), uidStr);
        String defProfile = dataStore.getString(PREF_DEFAULT_PROFILE, null);
        params = ProfilesManager.loadConfigOrDefault(configDir, defProfile);
        androidToSymbian = (params.keyMappings != null) ? params.keyMappings : KeyMapper.getDefaultKeyMap();

        if (params.showKeyboard) {
            setVirtualKeyboard(uidStr);
        }
        if (params.showKeyboard && keyboard instanceof FixedKeyboard) {
            setOrientation(ORIENTATION_PORTRAIT);
        } else {
            setOrientation(params.orientation);
        }
        Emulator.setScreenParams(params.screenBackgroundColor, params.screenScaleRatio,
                params.screenScaleType, params.screenGravity);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemUI();
        }
    }

    @SuppressLint("SourceLockedOrientationActivity")
    private void setOrientation(int orientation) {
        switch (orientation) {
            case ORIENTATION_AUTO:
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR);
                break;
            case ORIENTATION_PORTRAIT:
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT);
                break;
            case ORIENTATION_LANDSCAPE:
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
                break;
            case ORIENTATION_DEFAULT:
            default:
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
                break;
        }
    }

    @Override
    public void openOptionsMenu() {
        if (!actionBarEnabled) {
            showSystemUI();
        }
        super.openOptionsMenu();
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            if (actionBarEnabled) {
                showExitConfirmation();
            } else {
                openOptionsMenu();
            }
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    private void setTheme(String theme) {
        if (theme.equals("dark")) {
            setTheme(R.style.AppTheme_NoActionBar);
        } else {
            setTheme(R.style.AppTheme_Light_NoActionBar);
        }
    }

    private void showExitConfirmation() {
        AlertDialog.Builder alertBuilder = new AlertDialog.Builder(this);
        alertBuilder.setTitle(R.string.confirmation_required)
                .setMessage(R.string.force_close_confirmation)
                .setPositiveButton(android.R.string.ok, (d, w) -> Process.killProcess(Process.myPid()))
                .setNegativeButton(android.R.string.cancel, null);
        alertBuilder.create().show();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.emulator, menu);
        if (keyboard != null && !(keyboard instanceof FixedKeyboard)) {
            inflater.inflate(R.menu.emulator_keys, menu);
        }
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.action_exit) {
            showExitConfirmation();
        } else if (id == R.id.action_save_log) {
            saveLog();
        } else if (keyboard != null) {
            handleVkOptions(id);
        }
        return super.onOptionsItemSelected(item);
    }

    private void saveLog() {
        try {
            LogUtils.writeLog();
            Toast.makeText(this, R.string.log_saved, Toast.LENGTH_SHORT).show();
        } catch (IOException e) {
            e.printStackTrace();
            Toast.makeText(this, R.string.error, Toast.LENGTH_SHORT).show();
        }
    }

    private void handleVkOptions(int id) {
        if (id == R.id.action_layout_edit_mode) {
            keyboard.setLayoutEditMode(VirtualKeyboard.LAYOUT_KEYS);
            Toast.makeText(this, R.string.layout_edit_mode,
                    Toast.LENGTH_SHORT).show();
        } else if (id == R.id.action_layout_scale_mode) {
            keyboard.setLayoutEditMode(VirtualKeyboard.LAYOUT_SCALES);
            Toast.makeText(this, R.string.layout_scale_mode,
                    Toast.LENGTH_SHORT).show();
        } else if (id == R.id.action_layout_edit_finish) {
            keyboard.setLayoutEditMode(VirtualKeyboard.LAYOUT_EOF);
            Toast.makeText(this, R.string.layout_edit_finished,
                    Toast.LENGTH_SHORT).show();
        } else if (id == R.id.action_layout_switch) {
            showSetLayoutDialog();
        } else if (id == R.id.action_hide_buttons) {
            showHideButtonDialog();
        }
    }

    private void showSetLayoutDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this)
                .setTitle(R.string.layout_switch)
                .setSingleChoiceItems(keyboard.getLayoutNames(), -1,
                        (dialogInterface, i) -> keyboard.setLayout(i))
                .setPositiveButton(android.R.string.ok, null);
        builder.show();
    }

    private void showHideButtonDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this)
                .setTitle(R.string.hide_buttons)
                .setMultiChoiceItems(keyboard.getKeyNames(), keyboard.getKeyVisibility(),
                        (dialogInterface, i, b) -> keyboard.setKeyVisibility(i, b))
                .setPositiveButton(android.R.string.ok, null);
        builder.show();
    }

    private void setActionBar(String title) {
        ActionBar actionBar = Objects.requireNonNull(getSupportActionBar());
        LinearLayout.LayoutParams layoutParams = (LinearLayout.LayoutParams) toolbar.getLayoutParams();
        actionBar.setTitle(title);
        layoutParams.height = (int) (getToolBarHeight() / 1.5);
    }

    private int getToolBarHeight() {
        int[] attrs = new int[]{android.R.attr.actionBarSize};
        TypedArray ta = obtainStyledAttributes(attrs);
        int toolBarHeight = ta.getDimensionPixelSize(0, -1);
        ta.recycle();
        return toolBarHeight;
    }

    private void setVirtualKeyboard(String appDirName) {
        int vkType = params.vkType;
        if (vkType == VirtualKeyboard.CUSTOMIZABLE_TYPE) {
            keyboard = new VirtualKeyboard(this);
        } else if (vkType == VirtualKeyboard.PHONE_DIGITS_TYPE) {
            keyboard = new FixedKeyboard(this);
        } else {
            keyboard = new FixedKeyboard(this);
        }
        keyboard.setHideDelay(params.vkHideDelay);
        keyboard.setHasHapticFeedback(params.vkFeedback);
        keyboard.setButtonShape(params.vkButtonShape);

        File keyLayoutFile = new File(Emulator.getConfigsDir(),
                appDirName + Emulator.APP_KEY_LAYOUT_FILE);
        if (keyLayoutFile.exists()) {
            try {
                FileInputStream fis = new FileInputStream(keyLayoutFile);
                DataInputStream dis = new DataInputStream(fis);
                keyboard.readLayout(dis);
                fis.close();
            } catch (IOException ioe) {
                ioe.printStackTrace();
            }
        }

        int vkAlpha = params.vkAlpha << 24;
        keyboard.setColor(VirtualKeyboard.BACKGROUND, vkAlpha | params.vkBgColor);
        keyboard.setColor(VirtualKeyboard.FOREGROUND, vkAlpha | params.vkFgColor);
        keyboard.setColor(VirtualKeyboard.BACKGROUND_SELECTED, vkAlpha | params.vkBgColorSelected);
        keyboard.setColor(VirtualKeyboard.FOREGROUND_SELECTED, vkAlpha | params.vkFgColorSelected);
        keyboard.setColor(VirtualKeyboard.OUTLINE, vkAlpha | params.vkOutlineColor);
        overlayView.setOverlay(keyboard);
        keyboard.setView(overlayView);

        keyboard.setLayoutListener(vk -> {
            try {
                File parentFile = new File(Emulator.getConfigsDir(), appDirName);
                if (!parentFile.exists()) {
                    parentFile.mkdirs();
                }
                FileOutputStream fos = new FileOutputStream(keyLayoutFile);
                DataOutputStream dos = new DataOutputStream(fos);
                vk.writeLayout(dos);
                fos.close();
            } catch (IOException ioe) {
                ioe.printStackTrace();
            }
        });
    }

    private void hideSystemUI() {
        int flags = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        if (!statusBarEnabled) {
            flags |= View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_FULLSCREEN;
        }
        getWindow().getDecorView().setSystemUiVisibility(flags);
    }

    private void showSystemUI() {
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
    }

    private void updateScreenSize() {
        RectF screen = new RectF(0, 0, displayWidth, displayHeight);
        if (keyboard != null) {
            keyboard.resize(screen, screen);
        }
    }

    private int convertAndroidKeyCode(int keyCode) {
        return androidToSymbian.get(keyCode, Integer.MAX_VALUE);
    }

    private class ViewCallbacks implements View.OnTouchListener, SurfaceHolder.Callback, SurfaceHolder.Callback2, View.OnKeyListener {
        private final View view;
        private final FrameLayout rootView;

        public ViewCallbacks(View view) {
            this.view = view;
            rootView = ((Activity) view.getContext()).findViewById(R.id.emulator_frame);
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            // Ignore it
        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            Rect offsetViewBounds = new Rect(0, 0, width, height);
            rootView.offsetDescendantRectToMyCoords(view, offsetViewBounds);
            overlayView.setTargetBounds(offsetViewBounds);
            displayWidth = width;
            displayHeight = height;
            updateScreenSize();

            Emulator.surfaceChanged(holder.getSurface(), width, height);
            if (!launched) {
                Emulator.launchApp((int) uid);
                launched = true;
            }
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            Emulator.surfaceDestroyed();
        }

        @Override
        public boolean onKey(View v, int keyCode, KeyEvent event) {
            switch (event.getAction()) {
                case KeyEvent.ACTION_DOWN:
                    return onKeyDown(keyCode, event);
                case KeyEvent.ACTION_UP:
                    return onKeyUp(keyCode, event);
            }
            return false;
        }

        public boolean onKeyDown(int keyCode, KeyEvent event) {
            keyCode = convertAndroidKeyCode(keyCode);
            if (keyCode == Integer.MAX_VALUE) {
                return false;
            }
            if (event.getRepeatCount() == 0) {
                if (keyboard == null || !keyboard.keyPressed(keyCode)) {
                    Emulator.pressKey(keyCode, 0);
                }
            }
            return true;
        }

        public boolean onKeyUp(int keyCode, KeyEvent event) {
            keyCode = convertAndroidKeyCode(keyCode);
            if (keyCode == Integer.MAX_VALUE) {
                return false;
            }
            if (keyboard == null || !keyboard.keyReleased(keyCode)) {
                Emulator.pressKey(keyCode, 1);
            }
            return true;
        }

        @Override
        @SuppressLint("ClickableViewAccessibility")
        public boolean onTouch(View v, MotionEvent event) {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    if (keyboard != null) {
                        keyboard.show();
                    }
                case MotionEvent.ACTION_POINTER_DOWN:
                    int index = event.getActionIndex();
                    int id = event.getPointerId(index);
                    float x = event.getX(index);
                    float y = event.getY(index);
                    float z = event.getPressure(index) * 0x7FFFFFFF;    // Max pressure
                    if ((keyboard == null || !keyboard.pointerPressed(id, x, y))
                            && id == 0 && params.touchInput) {
                        Emulator.touchScreen((int) x, (int) y, (int)z, 0, id);
                    }
                    break;
                case MotionEvent.ACTION_MOVE:
                    int pointerCount = event.getPointerCount();
                    int historySize = event.getHistorySize();
                    for (int h = 0; h < historySize; h++) {
                        for (int p = 0; p < pointerCount; p++) {
                            id = event.getPointerId(p);
                            x = event.getHistoricalX(p, h);
                            y = event.getHistoricalY(p, h);
                            z = event.getHistoricalPressure(p, h) * 0x7FFFFFFF;    // Max pressure
                            if ((keyboard == null || !keyboard.pointerDragged(id, x, y))
                                    && id == 0 && params.touchInput) {
                                Emulator.touchScreen((int) x, (int) y, (int)z, 1, id);
                            }
                        }
                    }
                    for (int p = 0; p < pointerCount; p++) {
                        id = event.getPointerId(p);
                        x = event.getX(p);
                        y = event.getY(p);
                        z = event.getPressure(p) * 0x7FFFFFFF;    // Max pressure
                        if ((keyboard == null || !keyboard.pointerDragged(id, x, y))
                                && id == 0 && params.touchInput) {
                            Emulator.touchScreen((int) x, (int) y, (int)z, 1, id);
                        }
                    }
                    break;
                case MotionEvent.ACTION_UP:
                    if (keyboard != null) {
                        keyboard.hide();
                    }
                case MotionEvent.ACTION_POINTER_UP:
                    index = event.getActionIndex();
                    id = event.getPointerId(index);
                    x = event.getX(index);
                    y = event.getY(index);
                    if ((keyboard == null || !keyboard.pointerReleased(id, x, y))
                            && id == 0 && params.touchInput) {
                        Emulator.touchScreen((int) x, (int) y, (int)0, 2, id);
                    }
                    break;
                default:
                    return false;
            }
            return true;
        }

        @Override
        public void surfaceRedrawNeeded(SurfaceHolder surfaceHolder) {
            Emulator.surfaceRedrawNeeded();
        }
    }

}

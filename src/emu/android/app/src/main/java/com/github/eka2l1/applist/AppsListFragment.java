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

package com.github.eka2l1.applist;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.RectF;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SearchView;
import androidx.core.content.pm.ShortcutInfoCompat;
import androidx.core.content.pm.ShortcutManagerCompat;
import androidx.core.graphics.drawable.IconCompat;
import androidx.documentfile.provider.DocumentFile;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.github.eka2l1.R;
import com.github.eka2l1.config.ConfigFragment;
import com.github.eka2l1.config.ProfilesFragment;
import com.github.eka2l1.emu.Emulator;
import com.github.eka2l1.emu.EmulatorActivity;
import com.github.eka2l1.info.AboutDialogFragment;
import com.github.eka2l1.qrscan.QrScanResultContract;
import com.github.eka2l1.settings.AppDataStore;
import com.github.eka2l1.settings.SettingsFragment;
import com.github.eka2l1.util.FileUtils;
import com.github.eka2l1.util.LogUtils;
import com.github.eka2l1.util.ZipUtils;
import com.google.android.material.floatingactionbutton.FloatingActionButton;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import io.reactivex.Completable;
import io.reactivex.Observable;
import io.reactivex.ObservableOnSubscribe;
import io.reactivex.Single;
import io.reactivex.android.schedulers.AndroidSchedulers;
import io.reactivex.disposables.CompositeDisposable;
import io.reactivex.disposables.Disposable;
import io.reactivex.observers.DisposableCompletableObserver;
import io.reactivex.observers.DisposableSingleObserver;
import io.reactivex.schedulers.Schedulers;

import static com.github.eka2l1.emu.Constants.*;

import org.w3c.dom.Text;

public class AppsListFragment extends Fragment {
    private CompositeDisposable compositeDisposable;
    private AppsListAdapter adapter;
    private RecyclerView rvApplist;
    private TextView tvEmptyList;
    private DividerItemDecoration dividerItemDecoration;
    private boolean restartNeeded;
    private final ActivityResultLauncher<String[]> openSisLauncher = registerForActivityResult(
            FileUtils.getFilePicker(),
            this::onSisResult);
    private final ActivityResultLauncher<Void> openSDCardLauncher = registerForActivityResult(
            FileUtils.getDirPicker(),
            this::onSDCardResult);
    private final ActivityResultLauncher<Void> openNGageGameLauncher = registerForActivityResult(
            FileUtils.getDirPicker(),
            this::onNGageGameResult
    );

    private final ActivityResultLauncher<Integer> scanNG2LicenseLauncher = registerForActivityResult(
            new QrScanResultContract(),
            this::onNG2LicenseScanResult
    );

    private final ActivityResultLauncher<Integer> scanMMCIDLauncher = registerForActivityResult(
            new QrScanResultContract(),
            this::onMMCIDScanResult
    );

    private final ActivityResultLauncher<String[]> openPreconfiguredPackZIPLauncher = registerForActivityResult(
            FileUtils.getFilePicker(),
            this::onPreconfiguredPackZIPResult);

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        adapter = new AppsListAdapter(getContext());
        compositeDisposable = new CompositeDisposable();
        getParentFragmentManager().setFragmentResultListener(KEY_RESTART, this, (key, bundle) -> {
            restartNeeded = true;
        });
        getParentFragmentManager().setFragmentResultListener(KEY_RESTART_IMMEDIATELY, this, (key, bundle) -> {
            restart();
        });

    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_appslist, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        rvApplist = view.findViewById(R.id.rv_applist);
        tvEmptyList = view.findViewById(R.id.tv_empty_list);

        registerForContextMenu(rvApplist);

        AppDataStore store = AppDataStore.getAndroidStore();
        boolean isGrid = store.getBoolean(PREF_GRID_APP_LIST, true);

        rvApplist.setAdapter(adapter);
        setDisplay(view, isGrid);

        adapter.registerAdapterDataObserver(new RecyclerView.AdapterDataObserver() {
            @Override
            public void onChanged() {
                boolean empty = adapter.getItemCount() == 0;

                rvApplist.setVisibility(empty ? View.INVISIBLE : View.VISIBLE);
                tvEmptyList.setVisibility(empty ? View.VISIBLE : View.INVISIBLE);
            }
        });

        setHasOptionsMenu(true);
        prepareApps();
        ActionBar actionBar = ((AppCompatActivity) requireActivity()).getSupportActionBar();
        actionBar.setDisplayHomeAsUpEnabled(false);
        actionBar.setTitle(R.string.app_name);
        FloatingActionButton fab = view.findViewById(R.id.fab);
        fab.setOnClickListener(v -> openSisLauncher.launch(new String[]{".sis", ".sisx"}));
    }

    private void setDisplay(View view, boolean isGrid) {
        if (view == null) {
            view = getView();
        }

        Context context = view.getContext();

        rvApplist.setPadding(0, 0, 0, isGrid ? 200 : 0);
        rvApplist.requestLayout();

        if (isGrid) {
            rvApplist.setLayoutManager(new GridLayoutManager(context, 4));
            if (dividerItemDecoration != null) {
                rvApplist.removeItemDecoration(dividerItemDecoration);
            }
        } else {
            if (dividerItemDecoration == null) {
                dividerItemDecoration = new DividerItemDecoration(context, DividerItemDecoration.VERTICAL);
            }

            rvApplist.setLayoutManager(new LinearLayoutManager(context));
            rvApplist.addItemDecoration(dividerItemDecoration);
        }

        adapter.setDisplay(isGrid);
    }

    private void switchToDeviceList() {
        DeviceListFragment deviceListFragment = new DeviceListFragment();
        getParentFragmentManager().beginTransaction()
                .replace(R.id.container, deviceListFragment)
                .addToBackStack(null)
                .commit();
    }

    @SuppressWarnings("ResultOfMethodCallIgnored")
    @SuppressLint("CheckResult")
    private void prepareApps() {
        ProgressDialog dialog = new ProgressDialog(getActivity());
        dialog.setIndeterminate(true);
        dialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
        dialog.setCancelable(false);
        dialog.setMessage(getText(R.string.processing));
        dialog.show();
        Single<ArrayList<AppItem>> appsSingle = Emulator.getAppsList();
        appsSingle.subscribeOn(Schedulers.io())
                .observeOn(AndroidSchedulers.mainThread())
                .subscribeWith(new DisposableSingleObserver<ArrayList<AppItem>>() {
                    @Override
                    public void onSuccess(@NonNull ArrayList<AppItem> appItems) {
                        adapter.setItems(appItems);
                        dialog.cancel();
                    }

                    @Override
                    public void onError(@NonNull Throwable e) {
                        Toast.makeText(getContext(), R.string.no_devices_found, Toast.LENGTH_SHORT).show();
                        dialog.cancel();

                        AlertDialog installDeviceAskDialog = new AlertDialog.Builder(getActivity())
                                .setTitle(R.string.no_device_dialog_title)
                                .setMessage(R.string.no_device_installed)
                                .setPositiveButton(R.string.install, (d, id) -> {
                                    d.cancel();

                                    new AlertDialog.Builder(getActivity())
                                            .setTitle(R.string.choose_installation_type)
                                            .setMessage(R.string.choose_installation_type_instruction)
                                            .setPositiveButton(R.string.rom_or_firmware, (d2, id2) -> {
                                                d2.cancel();
                                                switchToDeviceList();
                                            })
                                            .setNegativeButton(R.string.preconfigured_pack, (d2, i2) -> {
                                                d2.cancel();
                                                openPreconfiguredZIPFilePicker();
                                            })
                                            .show();
                                }).setNegativeButton(android.R.string.cancel, (d, id) -> d.cancel()).create();
                        installDeviceAskDialog.show();
                    }
                });
    }

    @Override
    public void onResume() {
        super.onResume();
        if (restartNeeded) {
            restart();
        }
    }

    private void restart() {
        PackageManager packageManager = requireContext().getPackageManager();
        Intent intent = packageManager.getLaunchIntentForPackage(requireContext().getPackageName());
        ComponentName componentName = intent.getComponent();
        Intent mainIntent = Intent.makeRestartActivityTask(componentName);
        startActivity(mainIntent);
        Runtime.getRuntime().exit(0);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        compositeDisposable.clear();
    }

    private void onSisResult(String path) {
        if (!Emulator.isInitialized()) {
            Toast.makeText(getContext(), R.string.error, Toast.LENGTH_SHORT).show();
            return;
        }
        if (path == null) {
            return;
        }
        installApp(path);
    }

    private void onSDCardResult(String path) {
        if (!Emulator.isInitialized()) {
            Toast.makeText(getContext(), R.string.error, Toast.LENGTH_SHORT).show();
            return;
        }
        if (path == null) {
            return;
        }
        mountSdCard(path);
    }

    private void onNGageGameResult(String path) {
        if (!Emulator.isInitialized()) {
            Toast.makeText(getContext(), R.string.error, Toast.LENGTH_SHORT).show();
            return;
        }
        if (path == null) {
            return;
        }
        installNGageGame(path);
    }

    @SuppressWarnings("ResultOfMethodCallIgnored")
    @SuppressLint("CheckResult")
    private void installApp(String path) {
        Completable completable = Emulator.subscribeInstallApp(path);
        completable.subscribeOn(Schedulers.io())
                .observeOn(AndroidSchedulers.mainThread())
                .subscribeWith(new DisposableCompletableObserver() {
                    @Override
                    public void onComplete() {
                        Toast.makeText(getContext(), R.string.completed, Toast.LENGTH_SHORT).show();
                        restart();
                    }

                    @Override
                    public void onError(@NonNull Throwable e) {
                        Toast.makeText(getContext(), R.string.error, Toast.LENGTH_SHORT).show();
                    }
                });
    }

    private void mountSdCard(String path) {
        if (path == null) {
            return;
        }

        String folderName = FileUtils.getFilenameFromPath(getContext(), path);

        Pattern cidPattern = Pattern.compile("[0-9a-fA-F]+-[0-9a-fA-F]+-[0-9a-fA-F]+-[0-9a-fA-F]+");
        Matcher matcher = cidPattern.matcher(folderName);

        boolean mmcIdFoundInFolderName = matcher.find();

        Emulator.mountSdCard(path);

        if (mmcIdFoundInFolderName) {
            Emulator.setCurrentMMCID(matcher.group());
            Toast.makeText(getContext(), R.string.mounted_mmc_ok_and_use_id_in_folder_name, Toast.LENGTH_LONG).show();
        } else {
            Toast.makeText(getContext(), R.string.completed, Toast.LENGTH_SHORT).show();
        }

        prepareApps();
    }

    private void installNGageGame(String path) {
        int result = Emulator.installNGageGame(path);
        if (result == 0) {
            Toast.makeText(getContext(), R.string.completed, Toast.LENGTH_SHORT).show();
            restart();
        } else {
            int errorResId = R.string.error;
            switch (result) {
                case Emulator.INSTALL_NG_GAME_ERROR_NO_GAME_DATA_FOLDER:
                    errorResId = R.string.install_ng_game_cant_find_data_folder;
                    break;

                case Emulator.INSTALL_NG_GAME_ERROR_MORE_THAN_ONE_DATA_FOLDER:
                    errorResId = R.string.install_ng_game_more_than_one_game;
                    break;

                case Emulator.INSTALL_NG_GAME_ERROR_NO_GAME_REGISTERATION_INFO:
                    errorResId = R.string.install_ng_game_no_game_info_file;
                    break;

                case Emulator.INSTALL_NG_GAME_ERROR_REGISTERATION_CORRUPTED:
                    errorResId = R.string.install_ng_game_corrupted_game_info;
                    break;

                default:
                    break;
            }
            Toast.makeText(getContext(), errorResId, Toast.LENGTH_SHORT).show();
        }
    }

    private void onNG2LicenseScanResult(String content) {
        if (content == null) {
            return;
        }

        if (!Emulator.installNG2Licenses(content)) {
            Toast.makeText(getContext(), R.string.ng2_qr_code_invalid, Toast.LENGTH_LONG).show();
        } else {
            NG2LicenseInstallResultDialogFragment resultFragment = new NG2LicenseInstallResultDialogFragment();
            resultFragment.show(getParentFragmentManager(), "ng2_license_result");
        }
    }

    private void onMMCIDScanResult(String content) {
        if (content == null) {
            return;
        }

        String[] values = content.split("-");
        boolean failed = false;

        if (values.length != 4) {
            failed = true;
        } else {
            for (int i = 0; i < values.length; i++) {
                try {
                    Long.parseLong(values[i], 16);
                } catch (Exception ex) {
                    failed = true;
                    break;
                }
            }
        }

        Toast.makeText(getContext(), failed ? R.string.scan_qr_mmc_id_failed : R.string.scan_qr_mmc_id_success, failed ? Toast.LENGTH_LONG : Toast.LENGTH_SHORT).show();

        if (!failed) {
            AppDataStore store = AppDataStore.getEmulatorStore();
            store.putString("mmc-id", content);
            store.save();

            restart();
        }
    }

    private void setToggleGridIconStatus(MenuItem item, boolean isGrid) {
        item.setChecked(isGrid);
        if (isGrid) {
            item.setIcon(R.drawable.ic_list_white);
            item.setTitle(R.string.list_app_list_mode);
        } else {
            item.setIcon(R.drawable.ic_grid_white);
            item.setTitle(R.string.grid_app_list_mode);
        }
    }

    @SuppressLint("CheckResult")
    private void extractPreconfiguredPack(String inputPath, String destPath, String destPathTemp) {
        InputStream sourceZipStream;

        try {
            sourceZipStream = getContext().getContentResolver().openInputStream(Uri.parse(inputPath));
        } catch (FileNotFoundException ex) {
            Toast.makeText(getContext(), R.string.fail_to_install_preconfigured_pack, Toast.LENGTH_LONG).show();
            return;
        }

        ProgressDialog dialog = new ProgressDialog(getActivity());
        dialog.setIndeterminate(true);
        dialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
        dialog.setCancelable(false);
        dialog.setMessage(getText(R.string.processing));
        dialog.show();

        Single<Boolean> extractPackSingle = Single.create(emitter -> {
           try {
               ZipUtils.unzip(sourceZipStream, new File(destPathTemp));

               if (destPath != null) {
                    File destRealFile = new File(destPath);
                    FileUtils.deleteDirectory(destRealFile);

                    new File(destPathTemp + "/data").renameTo(destRealFile);
                    FileUtils.deleteDirectory(new File(destPathTemp));
               }
           } catch (IOException io)
           {
               // Delete temporary folder
               FileUtils.deleteDirectory(new File(destPathTemp));
               emitter.onError(io);
               return;
           }

           emitter.onSuccess(true);
        });

        extractPackSingle.subscribeOn(Schedulers.io())
                .observeOn(AndroidSchedulers.mainThread())
                .subscribeWith(new DisposableSingleObserver<Boolean>() {
                    @SuppressLint("CheckResult")
                    @Override
                    public void onSuccess(@NonNull Boolean result) {
                        Toast.makeText(getContext(), R.string.completed, Toast.LENGTH_SHORT).show();
                        dialog.cancel();

                        restart();
                    }

                    @Override
                    public void onError(@NonNull Throwable e) {
                        Toast.makeText(getContext(), (destPath != null) ? R.string.fail_to_install_preconfigured_pack_and_rollback : R.string.fail_to_install_preconfigured_pack, Toast.LENGTH_LONG).show();
                        dialog.cancel();
                    }
                });
    }

    private void onPreconfiguredPackZIPResult(String sourcePath) {
        if (sourcePath == null) {
            return;
        }

        String dataFolderPath = Emulator.getEmulatorDir() + "/data";
        String dataFolderPathTemp = Emulator.getEmulatorDir() + "/data_temp";

        File dataFolder = new File(dataFolderPath);

        if (dataFolder.exists()) {
            new AlertDialog.Builder(getContext())
                    .setTitle(R.string.delete_existing_data_title)
                    .setMessage(R.string.delete_existing_data)
                    .setPositiveButton(android.R.string.yes, (d, i) -> {
                        extractPreconfiguredPack(sourcePath, dataFolderPath, dataFolderPathTemp);
                    })
                    .setNegativeButton(android.R.string.cancel, (d, i) -> {
                        Toast.makeText(getContext(), R.string.cancelled, Toast.LENGTH_SHORT).show();
                    })
                    .show();
        } else {
            extractPreconfiguredPack(sourcePath, null, Emulator.getEmulatorDir());
        }
    }

    private void openPreconfiguredZIPFilePicker() {
        openPreconfiguredPackZIPLauncher.launch(new String[] { "*.zip" });
    }

    @Override
    public void onCreateOptionsMenu(@NonNull Menu menu, @NonNull MenuInflater inflater) {
        super.onCreateOptionsMenu(menu, inflater);
        inflater.inflate(R.menu.main, menu);
        final MenuItem searchItem = menu.findItem(R.id.action_search);
        SearchView searchView = (SearchView) searchItem.getActionView();
        Disposable searchViewDisposable = Observable.create((ObservableOnSubscribe<String>) emitter -> {
            searchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
                @Override
                public boolean onQueryTextSubmit(String query) {
                    emitter.onNext(query);
                    return true;
                }

                @Override
                public boolean onQueryTextChange(String newText) {
                    emitter.onNext(newText);
                    return true;
                }
            });
        }).debounce(300, TimeUnit.MILLISECONDS)
                .map(String::toLowerCase)
                .distinctUntilChanged()
                .observeOn(AndroidSchedulers.mainThread())
                .subscribe(charSequence -> adapter.getFilter().filter(charSequence));
        compositeDisposable.add(searchViewDisposable);

        AppDataStore store = AppDataStore.getAndroidStore();
        boolean isGrid = store.getBoolean(PREF_GRID_APP_LIST, true);

        final MenuItem gridModeItem = menu.findItem(R.id.action_toggle_grid);

        setToggleGridIconStatus(gridModeItem, isGrid);
        gridModeItem.setOnMenuItemClickListener(item -> {
            boolean gridEnabled = !item.isChecked();

            store.putBoolean(PREF_GRID_APP_LIST, gridEnabled);
            store.save();

            setDisplay(null, gridEnabled);
            setToggleGridIconStatus(item, gridEnabled);

            return true;
        });
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        int itemId = item.getItemId();
        if (itemId == R.id.action_about) {
            AboutDialogFragment aboutDialogFragment = new AboutDialogFragment();
            aboutDialogFragment.show(getParentFragmentManager(), "about");
        } else if (itemId == R.id.action_settings) {
            SettingsFragment settingsFragment = new SettingsFragment();
            getParentFragmentManager().beginTransaction()
                    .replace(R.id.container, settingsFragment)
                    .addToBackStack(null)
                    .commit();
        } else if (itemId == R.id.action_packages) {
            PackageListFragment packageListFragment = new PackageListFragment();
            getParentFragmentManager().beginTransaction()
                    .replace(R.id.container, packageListFragment)
                    .addToBackStack(null)
                    .commit();
        } else if (itemId == R.id.action_devices) {
            switchToDeviceList();
        } else if (itemId == R.id.action_mount_sd) {
            openSDCardLauncher.launch(null);
        } else if (itemId == R.id.action_save_log) {
            try {
                LogUtils.writeLog();
                Toast.makeText(getActivity(), R.string.log_saved, Toast.LENGTH_SHORT).show();
            } catch (IOException e) {
                e.printStackTrace();
                Toast.makeText(getActivity(), R.string.error, Toast.LENGTH_SHORT).show();
            }
        } else if (itemId == R.id.action_profiles) {
            ProfilesFragment profilesFragment = new ProfilesFragment();
            getParentFragmentManager().beginTransaction()
                    .replace(R.id.container, profilesFragment)
                    .addToBackStack(null)
                    .commit();
        } else if (itemId == R.id.action_install_ng_game) {
            openNGageGameLauncher.launch(null);
        } else if (itemId == R.id.action_install_ng2_licenses) {
            scanNG2LicenseLauncher.launch(R.string.ng2_scan_qr_title);
        } else if (itemId == R.id.action_scan_mmc_id) {
            scanMMCIDLauncher.launch(R.string.scan_qr_mmc_id_title);
        } else if (itemId == R.id.action_install_preconfigured_pack) {
            openPreconfiguredZIPFilePicker();
        } else if (itemId == R.id.action_switch_devices) {
            SwitchDevicesAlert.newInstance()
                    .show(getParentFragmentManager(), "alert_switch_devices");
        }
        return super.onOptionsItemSelected(item);
    }

    private void addAppShortcut(AppItem item) {
        // Get current device code
        int currentDeviceId = Emulator.getCurrentDevice();
        String []deviceCode = Emulator.getDeviceFirmwareCodes();

        if (currentDeviceId >= deviceCode.length) {
            return;
        }

        Bitmap iconBitmap = item.getIcon();
        IconCompat icon = null;

        if (iconBitmap == null) {
            icon = IconCompat.createWithResource(getActivity(), R.mipmap.ic_ducky_foreground);
        } else {
            int width = iconBitmap.getWidth();
            int height = iconBitmap.getHeight();
            ActivityManager am = (ActivityManager) getActivity().getSystemService(Context.ACTIVITY_SERVICE);
            int iconSize = am.getLauncherLargeIconSize();
            Rect src;
            if (width > height) {
                int left = (width - height) / 2;
                src = new Rect(left, 0, left + height, height);
            } else if (width < height) {
                int top = (height - width) / 2;
                src = new Rect(0, top, width, top + width);
            } else {
                src = null;
            }
            Bitmap scaled = Bitmap.createBitmap(iconSize, iconSize, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(scaled);
            canvas.drawBitmap(iconBitmap, src, new RectF(0, 0, iconSize, iconSize), null);
            icon = IconCompat.createWithBitmap(scaled);
        }

        Intent launchIntent = new Intent(Intent.ACTION_DEFAULT, null, getActivity(), EmulatorActivity.class);
        launchIntent.putExtra(KEY_APP_UID, item.getUid());
        launchIntent.putExtra(KEY_APP_NAME, item.getTitle());
        launchIntent.putExtra(KEY_DEVICE_CODE, deviceCode[currentDeviceId]);
        launchIntent.putExtra(KEY_APP_IS_SHORTCUT, true);

        ShortcutInfoCompat shortcut = new ShortcutInfoCompat.Builder(getActivity(), item.getTitle())
                .setIntent(launchIntent)
                .setShortLabel(item.getTitle())
                .setIcon(icon)
                .build();

        try {
            ShortcutManagerCompat.requestPinShortcut(getActivity(), shortcut, null);
        } catch (Exception ex) {
            ex.printStackTrace();
        }
    }

    @Override
    public boolean onContextItemSelected(@NonNull MenuItem item) {
        int index = AppItemViewHolder.contextIndex;
        AppItem appItem = adapter.getItem(index);
        if (item.getItemId() == R.id.action_context_settings) {
            ConfigFragment configFragment = new ConfigFragment();
            Bundle args = new Bundle();
            args.putLong(KEY_APP_UID, appItem.getUid());
            args.putString(KEY_APP_NAME, appItem.getTitle());
            args.putString(KEY_ACTION, ACTION_EDIT);
            configFragment.setArguments(args);
            getParentFragmentManager().beginTransaction()
                    .replace(R.id.container, configFragment)
                    .addToBackStack(null)
                    .commit();
        } else if (item.getItemId() == R.id.action_context_shortcut) {
            addAppShortcut(appItem);
        }
        return super.onContextItemSelected(item);
    }
}

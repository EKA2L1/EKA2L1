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
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Intent;
import android.os.Bundle;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SearchView;
import androidx.fragment.app.ListFragment;

import com.github.eka2l1.MainActivity;
import com.github.eka2l1.R;
import com.github.eka2l1.emu.Emulator;
import com.github.eka2l1.emu.EmulatorActivity;
import com.github.eka2l1.info.AboutDialogFragment;
import com.github.eka2l1.settings.AppSettingsFragment;
import com.github.eka2l1.settings.SettingsFragment;
import com.github.eka2l1.util.LogUtils;
import com.github.eka2l1.util.PickDirResultContract;
import com.github.eka2l1.util.PickFileResultContract;
import com.google.android.material.floatingactionbutton.FloatingActionButton;

import java.io.IOException;
import java.util.ArrayList;
import java.util.concurrent.TimeUnit;

import io.reactivex.Observable;
import io.reactivex.ObservableOnSubscribe;
import io.reactivex.Single;
import io.reactivex.android.schedulers.AndroidSchedulers;
import io.reactivex.disposables.CompositeDisposable;
import io.reactivex.disposables.Disposable;
import io.reactivex.observers.DisposableSingleObserver;
import io.reactivex.schedulers.Schedulers;

public class AppsListFragment extends ListFragment {
    private CompositeDisposable compositeDisposable;
    private AppsListAdapter adapter;
    private boolean restartNeeded;
    private final ActivityResultLauncher<String[]> openSisLauncher = registerForActivityResult(
            new PickFileResultContract(),
            this::onSisResult);
    private final ActivityResultLauncher<Void> openSDCardLauncher = registerForActivityResult(
            new PickDirResultContract(),
            this::onSDCardResult);

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        adapter = new AppsListAdapter(getContext());
        compositeDisposable = new CompositeDisposable();
        getParentFragmentManager().setFragmentResultListener("result", this, (key, bundle) -> {
            restartNeeded = bundle.getBoolean("restartNeeded");
        });
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_appslist, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        registerForContextMenu(getListView());
        setHasOptionsMenu(true);
        setListAdapter(adapter);
        prepareApps();
        ActionBar actionBar = ((AppCompatActivity) getActivity()).getSupportActionBar();
        actionBar.setDisplayHomeAsUpEnabled(false);
        actionBar.setTitle(R.string.app_name);
        FloatingActionButton fab = view.findViewById(R.id.fab);
        fab.setOnClickListener(v -> openSisLauncher.launch(new String[]{".sis", ".sisx"}));
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
                                    switchToDeviceList();
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
        Intent intent = new Intent(getActivity(), MainActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent);
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

    private void installApp(String path) {
        if (Emulator.installApp(path) == 0) {
            Toast.makeText(getContext(), R.string.completed, Toast.LENGTH_SHORT).show();
            restart();
        } else {
            Toast.makeText(getContext(), R.string.error, Toast.LENGTH_SHORT).show();
        }
    }

    private void mountSdCard(String path) {
        Emulator.mountSdCard(path);
        Toast.makeText(getContext(), R.string.completed, Toast.LENGTH_SHORT).show();
        prepareApps();
    }

    @Override
    public void onListItemClick(@NonNull ListView l, @NonNull View v, int position, long id) {
        AppItem item = adapter.getItem(position);
        Intent intent = new Intent(getContext(), EmulatorActivity.class);
        intent.putExtra(EmulatorActivity.APP_UID_KEY, item.getUid());
        intent.putExtra(EmulatorActivity.APP_NAME_KEY, item.getTitle());
        startActivity(intent);
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
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onCreateContextMenu(@NonNull ContextMenu menu, @NonNull View v, @Nullable ContextMenu.ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, v, menuInfo);
        MenuInflater inflater = getActivity().getMenuInflater();
        inflater.inflate(R.menu.context_appslist, menu);
    }

    @Override
    public boolean onContextItemSelected(@NonNull MenuItem item) {
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        int index = info.position;
        AppItem appItem = adapter.getItem(index);
        if (item.getItemId() == R.id.action_context_settings) {
            AppSettingsFragment appSettingsFragment = new AppSettingsFragment();
            Bundle args = new Bundle();
            args.putLong(AppSettingsFragment.APP_UID_KEY, appItem.getUid());
            appSettingsFragment.setArguments(args);
            getParentFragmentManager().beginTransaction()
                    .replace(R.id.container, appSettingsFragment)
                    .addToBackStack(null)
                    .commit();
        }
        return super.onContextItemSelected(item);
    }
}

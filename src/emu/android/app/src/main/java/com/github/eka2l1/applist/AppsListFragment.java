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
import android.app.Activity;
import android.app.ProgressDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;
import android.widget.Toast;

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
import com.github.eka2l1.filepicker.FilteredFilePickerActivity;
import com.github.eka2l1.filepicker.FilteredFilePickerFragment;
import com.github.eka2l1.info.AboutDialogFragment;
import com.github.eka2l1.settings.SettingsFragment;
import com.google.android.material.floatingactionbutton.FloatingActionButton;
import com.nononsenseapps.filepicker.FilePickerActivity;
import com.nononsenseapps.filepicker.Utils;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

import io.reactivex.Observable;
import io.reactivex.ObservableOnSubscribe;
import io.reactivex.Single;
import io.reactivex.android.schedulers.AndroidSchedulers;
import io.reactivex.disposables.CompositeDisposable;
import io.reactivex.disposables.Disposable;
import io.reactivex.observers.DisposableSingleObserver;
import io.reactivex.schedulers.Schedulers;

import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;

public class AppsListFragment extends ListFragment {
    private static final int SISX_CODE = 0;
    private static final int SDCARD_CODE = 1;

    private CompositeDisposable compositeDisposable;
    private AppsListAdapter adapter;
    private boolean restartNeeded;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        adapter = new AppsListAdapter(getContext());
        compositeDisposable = new CompositeDisposable();
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
        FloatingActionButton fab = getActivity().findViewById(R.id.fab);
        fab.setOnClickListener(v -> {
            Intent i = new Intent(getActivity(), FilteredFilePickerActivity.class);
            i.putExtra(FilePickerActivity.EXTRA_ALLOW_MULTIPLE, false);
            i.putExtra(FilePickerActivity.EXTRA_SINGLE_CLICK, true);
            i.putExtra(FilePickerActivity.EXTRA_ALLOW_CREATE_DIR, false);
            i.putExtra(FilePickerActivity.EXTRA_MODE, FilePickerActivity.MODE_FILE);
            i.putExtra(FilePickerActivity.EXTRA_START_PATH, FilteredFilePickerFragment.getLastPath());
            i.putExtra(FilteredFilePickerActivity.EXTRA_EXTENSIONS, new String[]{".sis", ".sisx"});
            startActivityForResult(i, SISX_CODE);
        });
    }

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
                    public void onSuccess(ArrayList<AppItem> appItems) {
                        adapter.setItems(appItems);
                        dialog.cancel();
                    }

                    @Override
                    public void onError(Throwable e) {
                        Toast.makeText(getContext(), R.string.error, Toast.LENGTH_SHORT).show();
                        dialog.cancel();
                    }
                });
    }

    public void setRestartNeeded(boolean restartNeeded) {
        this.restartNeeded = restartNeeded;
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
        intent.addFlags(FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent);
        Runtime.getRuntime().exit(0);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        compositeDisposable.clear();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (!Emulator.isInitialized()) {
            Toast.makeText(getContext(), R.string.error, Toast.LENGTH_SHORT).show();
            return;
        }
        if (resultCode == Activity.RESULT_OK) {
            List<Uri> files = Utils.getSelectedFilesFromResult(data);
            for (Uri uri : files) {
                File file = Utils.getFileForUri(uri);
                String path = file.getAbsolutePath();
                if (requestCode == SISX_CODE) {
                    installApp(path);
                } else {
                    mountSdCard(path);
                }
            }
        }
    }

    private void installApp(String path) {
        if (Emulator.installApp(path)) {
            Toast.makeText(getContext(), R.string.completed, Toast.LENGTH_SHORT).show();
            restart();
        } else {
            Toast.makeText(getContext(), R.string.error, Toast.LENGTH_SHORT).show();
        }
    }

    private void mountSdCard(String path) {
        Emulator.mountSdCard(path);
        Toast.makeText(getContext(), R.string.completed, Toast.LENGTH_SHORT).show();
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
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
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
        switch (item.getItemId()) {
            case R.id.action_about:
                AboutDialogFragment aboutDialogFragment = new AboutDialogFragment();
                aboutDialogFragment.show(getParentFragmentManager(), "about");
                break;
            case R.id.action_settings:
                SettingsFragment settingsFragment = new SettingsFragment();
                getParentFragmentManager().beginTransaction()
                        .replace(R.id.container, settingsFragment)
                        .addToBackStack(null)
                        .commit();
                break;
            case R.id.action_packages:
                PackageListFragment packageListFragment = new PackageListFragment();
                packageListFragment.setTargetFragment(this, 0);
                getParentFragmentManager().beginTransaction()
                        .replace(R.id.container, packageListFragment)
                        .addToBackStack(null)
                        .commit();
                break;
            case R.id.action_devices:
                DeviceListFragment deviceListFragment = new DeviceListFragment();
                deviceListFragment.setTargetFragment(this, 0);
                getParentFragmentManager().beginTransaction()
                        .replace(R.id.container, deviceListFragment)
                        .addToBackStack(null)
                        .commit();
                break;
            case R.id.action_mount_sd:
                Intent i = new Intent(getActivity(), FilteredFilePickerActivity.class);
                i.putExtra(FilePickerActivity.EXTRA_ALLOW_MULTIPLE, false);
                i.putExtra(FilePickerActivity.EXTRA_SINGLE_CLICK, false);
                i.putExtra(FilePickerActivity.EXTRA_ALLOW_CREATE_DIR, false);
                i.putExtra(FilePickerActivity.EXTRA_MODE, FilePickerActivity.MODE_DIR);
                i.putExtra(FilePickerActivity.EXTRA_START_PATH, FilteredFilePickerFragment.getLastPath());
                startActivityForResult(i, SDCARD_CODE);
                break;
        }
        return super.onOptionsItemSelected(item);
    }
}

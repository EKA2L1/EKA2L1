// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later

package com.github.eka2l1.settings;

import android.os.Bundle;
import android.text.InputType;
import android.view.MenuItem;
import android.view.View;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import com.github.eka2l1.R;
import com.github.eka2l1.emu.Emulator;

import java.util.Locale;
import java.util.Map;
import java.util.TreeMap;

public class HostOverridesFragment extends PreferenceFragmentCompat {
    private Map<String, String> hosts;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        hosts = new TreeMap<>(AppDataStore.getEmulatorStore().getStringMap("hosts"));
        refresh();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        setHasOptionsMenu(true);
        ((AppCompatActivity) requireActivity()).getSupportActionBar().setTitle(R.string.hosts_title);
    }

    private void refresh() {
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(requireContext());
        Preference hint = new Preference(requireContext());
        hint.setSummary(R.string.hosts_hint);
        hint.setSelectable(false);
        screen.addPreference(hint);
        for (Map.Entry<String, String> entry : hosts.entrySet()) {
            Preference row = new Preference(requireContext());
            row.setTitle(entry.getKey());
            row.setSummary(entry.getValue());
            row.setOnPreferenceClickListener(preference -> {
                edit(entry.getKey());
                return true;
            });
            screen.addPreference(row);
        }
        Preference add = new Preference(requireContext());
        add.setTitle(R.string.hosts_add);
        add.setOnPreferenceClickListener(preference -> {
            edit(null);
            return true;
        });
        screen.addPreference(add);
        setPreferenceScreen(screen);
    }

    private static String normalize(String value) {
        String name = value.trim().toLowerCase(Locale.ROOT);
        return name.endsWith(".") ? name.substring(0, name.length() - 1) : name;
    }

    private void edit(@Nullable String oldName) {
        LinearLayout fields = new LinearLayout(requireContext());
        fields.setOrientation(LinearLayout.VERTICAL);
        int padding = (int) (24 * getResources().getDisplayMetrics().density);
        fields.setPadding(padding, 0, padding, 0);
        EditText name = new EditText(requireContext());
        EditText target = new EditText(requireContext());
        for (EditText field : new EditText[]{name, target}) {
            field.setSingleLine(true);
            field.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
            fields.addView(field);
        }
        name.setHint(R.string.hosts_hostname);
        target.setHint(R.string.hosts_target);
        if (oldName != null) {
            name.setText(oldName);
            target.setText(hosts.get(oldName));
        }
        AlertDialog.Builder builder = new AlertDialog.Builder(requireContext())
                .setTitle(R.string.hosts_mapping)
                .setView(fields)
                .setPositiveButton(android.R.string.ok, null)
                .setNegativeButton(android.R.string.cancel, null);
        if (oldName != null) builder.setNeutralButton(R.string.remove, null);
        AlertDialog dialog = builder.create();
        dialog.setOnShowListener(ignored -> {
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(view -> {
                String hostname = normalize(name.getText().toString());
                String address = normalize(target.getText().toString());
                if (!Emulator.validHostMapping(hostname, address)) {
                    target.setError(getString(R.string.hosts_invalid));
                    return;
                }
                for (String existing : hosts.keySet()) {
                    if (!existing.equals(oldName) && normalize(existing).equals(hostname)) {
                        name.setError(getString(R.string.hosts_duplicate));
                        return;
                    }
                }
                Map<String, String> updated = new TreeMap<>(hosts);
                if (oldName != null) updated.remove(oldName);
                updated.put(hostname, address);
                if (save(updated)) dialog.dismiss();
            });
            if (oldName != null) {
                dialog.getButton(AlertDialog.BUTTON_NEUTRAL).setOnClickListener(view -> {
                    Map<String, String> updated = new TreeMap<>(hosts);
                    updated.remove(oldName);
                    if (save(updated)) dialog.dismiss();
                });
            }
        });
        dialog.show();
    }

    private boolean save(Map<String, String> updated) {
        AppDataStore store = AppDataStore.getEmulatorStore();
        store.putStringMap("hosts", updated);
        if (!store.save()) {
            Toast.makeText(requireContext(), R.string.hosts_save_error, Toast.LENGTH_LONG).show();
            return false;
        }
        Emulator.loadConfig();
        hosts = updated;
        refresh();
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            getParentFragmentManager().popBackStackImmediate();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}

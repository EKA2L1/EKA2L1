/*
 * Copyright (c) 2026 EKA2L1 Team
 *
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
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

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Looper;
import android.util.Log;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;

public class EmulatorLocation {
    private static final String TAG = "EKA2L1_Location";
    private static final long UPDATE_INTERVAL_MS = 1000;

    private static AppCompatActivity applicationActivity;
    private static LocationListener listener;
    private static boolean running;

    public static void setActivity(AppCompatActivity activity) {
        applicationActivity = activity;
    }

    public static synchronized void start() {
        if (running || applicationActivity == null) {
            return;
        }

        running = true;

        // Asking for permission blocks until the user answers, which the emulator thread must not wait on.
        new Thread(() -> {
            if (!hasPermission() && (applicationActivity instanceof EmulatorActivity)) {
                try {
                    ((EmulatorActivity) applicationActivity).requestPermissionsAndWait(new String[]{
                            Manifest.permission.ACCESS_FINE_LOCATION,
                            Manifest.permission.ACCESS_COARSE_LOCATION
                    });
                } catch (InterruptedException ex) {
                    return;
                }
            }

            if (!hasPermission()) {
                Log.i(TAG, "Location permission denied, guest positioning gets no fix");
                return;
            }

            applicationActivity.runOnUiThread(EmulatorLocation::requestUpdates);
        }).start();
    }

    public static synchronized void stop() {
        running = false;

        if (applicationActivity != null) {
            applicationActivity.runOnUiThread(EmulatorLocation::removeUpdates);
        }
    }

    private static boolean hasPermission() {
        return (ContextCompat.checkSelfPermission(applicationActivity, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED)
                || (ContextCompat.checkSelfPermission(applicationActivity, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED);
    }

    @SuppressLint("MissingPermission")
    private static synchronized void requestUpdates() {
        if (!running || (listener != null)) {
            return;
        }

        LocationManager manager = (LocationManager) applicationActivity.getSystemService(Context.LOCATION_SERVICE);
        if (manager == null) {
            return;
        }

        listener = new LocationListener() {
            @Override
            public void onLocationChanged(Location location) {
                deliver(location);
            }

            @Override
            public void onStatusChanged(String provider, int status, Bundle extras) {
            }

            @Override
            public void onProviderEnabled(String provider) {
            }

            @Override
            public void onProviderDisabled(String provider) {
            }
        };

        for (String provider : new String[]{ LocationManager.GPS_PROVIDER, LocationManager.NETWORK_PROVIDER }) {
            try {
                if (manager.getAllProviders().contains(provider)) {
                    manager.requestLocationUpdates(provider, UPDATE_INTERVAL_MS, 0, listener, Looper.getMainLooper());
                }
            } catch (SecurityException | IllegalArgumentException ex) {
                Log.w(TAG, "Unable to request updates from " + provider + ": " + ex);
            }
        }
    }

    private static synchronized void removeUpdates() {
        if (running || (listener == null)) {
            return;
        }

        LocationManager manager = (LocationManager) applicationActivity.getSystemService(Context.LOCATION_SERVICE);
        if (manager != null) {
            manager.removeUpdates(listener);
        }

        listener = null;
    }

    private static void deliver(Location location) {
        final boolean modern = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;

        onLocationChanged(location.getLatitude(), location.getLongitude(),
                location.hasAltitude() ? (float) location.getAltitude() : Float.NaN,
                location.hasAccuracy() ? location.getAccuracy() : Float.NaN,
                (modern && location.hasVerticalAccuracy()) ? location.getVerticalAccuracyMeters() : Float.NaN,
                location.hasSpeed() ? location.getSpeed() : Float.NaN,
                (modern && location.hasSpeedAccuracy()) ? location.getSpeedAccuracyMetersPerSecond() : Float.NaN,
                location.hasBearing() ? location.getBearing() : Float.NaN,
                (modern && location.hasBearingAccuracy()) ? location.getBearingAccuracyDegrees() : Float.NaN);
    }

    private static native void onLocationChanged(double latitude, double longitude, float altitude,
                                                 float horizontalAccuracy, float verticalAccuracy,
                                                 float speed, float speedAccuracy,
                                                 float course, float courseAccuracy);
}

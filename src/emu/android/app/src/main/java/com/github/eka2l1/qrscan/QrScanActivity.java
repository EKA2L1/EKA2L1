/*
 * Copyright (c) 2023 EKA2L1 Team
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

package com.github.eka2l1.qrscan;

import static androidx.camera.view.CameraController.COORDINATE_SYSTEM_VIEW_REFERENCED;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.camera.core.CameraSelector;
import androidx.camera.mlkit.vision.MlKitAnalyzer;
import androidx.camera.view.LifecycleCameraController;
import androidx.camera.view.PreviewView;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.github.eka2l1.R;
import com.github.eka2l1.emu.Emulator;
import com.github.eka2l1.util.FileUtils;
import com.google.android.material.floatingactionbutton.FloatingActionButton;
import com.google.mlkit.vision.barcode.BarcodeScanner;
import com.google.mlkit.vision.barcode.BarcodeScannerOptions;
import com.google.mlkit.vision.barcode.BarcodeScanning;
import com.google.mlkit.vision.barcode.common.Barcode;
import com.google.mlkit.vision.common.InputImage;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Executor;

public class QrScanActivity extends AppCompatActivity {
    public static final String EXTRA_TITLE_ID = "qrscan.intent.title";
    public static final int DEFAULT_TITLE_ID = -1;

    private final ActivityResultLauncher<String> permissionLauncher = registerForActivityResult(
            new ActivityResultContracts.RequestPermission(),
            this::onPermissionResult);

    private final ActivityResultLauncher<String[]> openImageFileLauncher = registerForActivityResult(
            FileUtils.getFilePicker(),
            this::onImageFileResult);

    private PreviewView pvQrScan;
    private TextView tvNoCam;
    private FloatingActionButton fabLoadImg;
    private LifecycleCameraController cameraController;
    private BarcodeScanner barcodeScanner;

    private boolean processBarcodes(List<Barcode> barcodes) {
        for (int i = 0; i < barcodes.size(); i++) {
            Barcode barCode = barcodes.get(i);

            if (barCode.getValueType() == Barcode.TYPE_TEXT) {
                Intent returnIntent = new Intent();
                returnIntent.putExtra("value", barCode.getDisplayValue());

                setResult(RESULT_OK, returnIntent);
                finish();

                return true;
            }
        }
        
        return false;
    }

    private void onImageFileResult(String path) {
        if (path == null) {
            return;
        }

        InputImage image;
        try {
            image = InputImage.fromFilePath(this, Uri.parse(path));
        } catch (IOException e) {
            Toast.makeText(this, R.string.qr_not_found_in_image, Toast.LENGTH_LONG).show();
            return;
        }

        barcodeScanner.process(image)
                .addOnSuccessListener(barcodes -> {
                    if (!processBarcodes(barcodes)) {
                        Toast.makeText(this, R.string.qr_not_found_in_image, Toast.LENGTH_LONG).show();
                    }
                })
                .addOnFailureListener(e -> {
                    Toast.makeText(this, R.string.qr_not_found_in_image, Toast.LENGTH_LONG).show();
                });
    }

    private void bindPreview() {
        cameraController.bindToLifecycle(this);

        CameraSelector cameraSelector = new CameraSelector.Builder()
                .requireLensFacing(CameraSelector.LENS_FACING_BACK)
                .build();

        if (cameraController.hasCamera(cameraSelector)) {
            cameraController.setCameraSelector(cameraSelector);
        } else {
            Toast.makeText(this, R.string.camera_not_available, Toast.LENGTH_SHORT);
            finishActivity(RESULT_CANCELED);

            return;
        }

        // Attach PreviewView after we know the camera is available.
        pvQrScan.setController(cameraController);

        Executor executor = ContextCompat.getMainExecutor(this);

        cameraController.setImageAnalysisAnalyzer(executor,
            new MlKitAnalyzer(Arrays.asList(barcodeScanner), COORDINATE_SYSTEM_VIEW_REFERENCED,
                executor, image -> {
                    List<Barcode> result = image.getValue(barcodeScanner);
                    processBarcodes(result);
                })
        );
    }

    private void prepareCameraWork() {
        cameraController = new LifecycleCameraController(this);
        cameraController.getInitializationFuture().addListener(() -> {
            bindPreview();
        }, ContextCompat.getMainExecutor(this));
    }

    public void onPermissionResult(boolean status) {
        if (status) {
            prepareCameraWork();
        } else {
            // Allow it but put our duck in
            tvNoCam.setVisibility(View.VISIBLE);
        }
    }

    private void requestCameraPermissions() {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            permissionLauncher.launch(Manifest.permission.CAMERA);
        } else {
            prepareCameraWork();
        }
    }

    private void prepareQrScan() {
        // Attach QR scanner
        BarcodeScannerOptions options = new BarcodeScannerOptions.Builder()
                .setBarcodeFormats(Barcode.FORMAT_QR_CODE)
                .build();

        barcodeScanner = BarcodeScanning.getClient(options);

        fabLoadImg.setOnClickListener(v -> {
            openImageFileLauncher.launch(new String[] { ".bmp", ".png", ".jpg", ".jpeg" });
        });

        requestCameraPermissions();
    }

    @Override
    public boolean onSupportNavigateUp() {
        onBackPressed();
        return true;
    }

    @SuppressLint("NewApi")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Emulator.initializePath(this);

        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_qrscan);

        Intent intent = getIntent();
        int customTitleResId = intent.getIntExtra(EXTRA_TITLE_ID, -1);

        if (customTitleResId < 0) {
            setTitle(R.string.qr_scan_title);
        } else {
            setTitle(customTitleResId);
        }

        // Setup action bar
        ActionBar actionBar = getSupportActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);

        // Setup scan view
        pvQrScan = findViewById(R.id.pv_qrscan);
        tvNoCam = findViewById(R.id.tv_no_cam);
        fabLoadImg = findViewById(R.id.fab_loadimg);

        tvNoCam.setVisibility(View.INVISIBLE);
        tvNoCam.setOnClickListener(v -> {
            MediaPlayer player = MediaPlayer.create(this, R.raw.se_quack);
            player.start();
        });

        prepareQrScan();
    }
}

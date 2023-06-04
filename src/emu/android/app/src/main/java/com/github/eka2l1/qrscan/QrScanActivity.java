package com.github.eka2l1.qrscan;

import static androidx.camera.view.CameraController.COORDINATE_SYSTEM_VIEW_REFERENCED;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
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
import com.google.mlkit.vision.barcode.BarcodeScanner;
import com.google.mlkit.vision.barcode.BarcodeScannerOptions;
import com.google.mlkit.vision.barcode.BarcodeScanning;
import com.google.mlkit.vision.barcode.common.Barcode;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Executor;

public class QrScanActivity extends AppCompatActivity {
    public static final String EXTRA_TITLE_ID = "qrscan.intent.title";
    public static final int DEFAULT_TITLE_ID = -1;

    private final ActivityResultLauncher<String> permissionLauncher = registerForActivityResult(
            new ActivityResultContracts.RequestPermission(),
            this::onPermissionResult);

    private PreviewView pvQrScan;
    private LifecycleCameraController cameraController;

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

        // Attach QR scanner
        BarcodeScannerOptions options = new BarcodeScannerOptions.Builder()
                .setBarcodeFormats(Barcode.FORMAT_QR_CODE)
                .build();

        BarcodeScanner barcodeScanner = BarcodeScanning.getClient(options);
        Executor executor = ContextCompat.getMainExecutor(this);

        cameraController.setImageAnalysisAnalyzer(executor,
            new MlKitAnalyzer(Arrays.asList(barcodeScanner), COORDINATE_SYSTEM_VIEW_REFERENCED,
                executor, image -> {
                    List<Barcode> result = image.getValue(barcodeScanner);
                    for (int i = 0; i < result.size(); i++) {
                        Barcode barCode = result.get(i);

                        if (barCode.getValueType() == Barcode.TYPE_TEXT) {
                            Intent returnIntent = new Intent();
                            returnIntent.putExtra("value", barCode.getDisplayValue());

                            setResult(RESULT_OK, returnIntent);
                            finish();

                            return;
                        }
                    }
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
            finishActivity(RESULT_CANCELED);
        }
    }

    private void requestCameraPermissions() {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            permissionLauncher.launch(Manifest.permission.CAMERA);
        } else {
            prepareCameraWork();
        }
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

        pvQrScan = findViewById(R.id.pv_qrscan);
        requestCameraPermissions();
    }
}

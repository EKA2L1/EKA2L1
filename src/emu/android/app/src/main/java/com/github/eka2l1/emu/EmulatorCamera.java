package com.github.eka2l1.emu;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.ImageFormat;
import android.graphics.Matrix;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.os.Looper;
import android.util.Log;
import android.util.Size;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.camera.camera2.interop.Camera2CameraInfo;
import androidx.camera.core.Camera;
import androidx.camera.core.CameraControl;
import androidx.camera.core.CameraInfoUnavailableException;
import androidx.camera.core.CameraSelector;
import androidx.camera.core.ImageAnalysis;
import androidx.camera.core.ImageCapture;
import androidx.camera.core.ImageCaptureException;
import androidx.camera.core.ImageProxy;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.io.ByteArrayOutputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

public class EmulatorCamera {
    private static ProcessCameraProvider cameraProvider;
    private static AppCompatActivity applicationActivity;

    private static final String TAG = "EKA2L1_Camera";
    private static final int FLASH_MODE_SPECIAL_TORCH = 0x1000;

    // TODO: Add YUV... But game commonly uses RGBA8888 or 565 (YES, games)
    private static final int FORMAT_DRIVER_RGB565 = 0x04;
    private static final int FORMAT_DRIVER_ARGB8888 = 0x08;
    private static final int FORMAT_DRIVER_JPEG = 0x10;
    private static final int FORMAT_DRIVER_EXIF = 0x20;
    private static final int FORMAT_DRIVER_FBS_BMP_64K = 0x80;
    private static final int FORMAT_DRIVER_FBS_BMP_16M = 0x100;
    private static final int FORMAT_DRIVER_FBS_BMP_16MU = 0x10000;

    // Symbian values
    private static final int FLASH_MODE_DRIVER_NONE = 0x00;
    private static final int FLASH_MODE_DRIVER_AUTO = 0x01;
    private static final int FLASH_MODE_DRIVER_FORCED = 0x02;
    private static final int FLASH_MODE_DRIVER_VIDEO_LIGHT = 0x80;

    private static HashMap<Integer, EmulatorCamera> cameraWrappers;
    private static int cameraIdCounter = 1;

    private int flashMode;
    private int index;
    private boolean pendingViewfinder;
    private boolean pendingImageCapture;
    private boolean configChanged;
    private int previousResolutionIndex;
    private ImageAnalysis currentAnalysis;
    private ImageCapture previousImageCapture;
    private Size[] outputImageSizes;
    private Camera camera;
    private CameraCharacteristics characteristics;

    // Cached parameters for orientation change
    private int previousViewfinderFormat = 0;
    private int previousImageFormat = 0;
    private Size previousSize;

    private Executor cameraExecutor;
    private int useCount = 1;

    public static void setActivity(AppCompatActivity activity) {
        applicationActivity = activity;
        cameraWrappers = new HashMap<>();

        if (cameraProvider == null) {
            try {
                cameraProvider = ProcessCameraProvider.getInstance(applicationActivity).get();
            } catch (Exception ex) {
                Log.e(TAG, "Unable to initialize camera provider, exception=" + ex);
            }
        }
    }

    public static int getCameraCount() throws CameraInfoUnavailableException {
        int camCount = 0;

        // To ensure best compatibility, only provide one back and one front camera
        if (cameraProvider.hasCamera(CameraSelector.DEFAULT_BACK_CAMERA)) {
            camCount++;
        }

        if (cameraProvider.hasCamera(CameraSelector.DEFAULT_FRONT_CAMERA)) {
            camCount++;
        }

        return camCount;
    }

    public static int initializeCamera(int index) {
        try {
            if (index < 0) {
                Log.e(TAG, "Invalid camera index " + index);
                return -1;
            }

            for (HashMap.Entry<Integer, EmulatorCamera> entry : cameraWrappers.entrySet()) {
                if (entry.getValue().index == index) {
                    entry.getValue().useCount++;
                    return entry.getKey();
                }
            }

            if (ActivityCompat.checkSelfPermission(applicationActivity, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
                if (applicationActivity instanceof EmulatorActivity) {
                    ((EmulatorActivity) applicationActivity).requestPermissionsAndWait(new String[]{ Manifest.permission.CAMERA });
                }

                if (ActivityCompat.checkSelfPermission(applicationActivity, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
                    Log.i(TAG, "Permission denied accessing camera, return null camera instance!");
                    return -1;
                }
            }

            EmulatorCamera newCam = new EmulatorCamera(index);
            cameraWrappers.put(cameraIdCounter, newCam);

            return cameraIdCounter++;
        } catch (Exception ex) {
            return -1;
        }
    }

    public static void releaseCamera(int handle) throws InterruptedException {
        if (!cameraWrappers.containsKey(handle)) {
            Log.e(TAG, "No camera found with handle " + handle);
            return;
        }

        EmulatorCamera cam = cameraWrappers.get(handle);
        cam.useCount--;

        if (cam.useCount == 0) {
            cam.stopViewfinderFeed();

            if (cam.previousImageCapture != null) {
                applicationActivity.runOnUiThread(() -> {
                    cameraProvider.unbind(cam.previousImageCapture);
                });
            }

            cameraWrappers.remove(handle);
        }
    }

    public static void handleOrientationChangeForAllInstances() {
        for (int i = 0; i < cameraWrappers.size(); i++) {
            cameraWrappers.get(i).handleOrientationChange();
        }
    }

    /* WRAPPER */
    public static EmulatorCamera getCameraWith(int handle) {
        if (!cameraWrappers.containsKey(handle)) {
            Log.e(TAG, "No camera found with handle " + handle);
            return null;
        }

        return cameraWrappers.get(handle);
    }

    public static boolean setFlashMode(int handle, int flashMode) {
        EmulatorCamera cam = getCameraWith(handle);
        if (cam != null) {
            return cam.setFlashMode(flashMode);
        }
        return false;
    }

    public static int getFlashMode(int handle) {
        EmulatorCamera cam = getCameraWith(handle);
        if (cam != null) {
            return cam.getFlashMode();
        }
        return FLASH_MODE_DRIVER_NONE;
    }

    public static boolean captureImage(int handle, int resolutionIndex, int format) {
        EmulatorCamera cam = getCameraWith(handle);
        if (cam != null) {
            return cam.captureImage(resolutionIndex, format);
        }
        return false;
    }

    public static boolean receiveViewfinderFeed(int handle, int width, int height, int requestedFormat) {
        EmulatorCamera cam = getCameraWith(handle);
        if (cam != null) {
            return cam.receiveViewfinderFeed(width, height, requestedFormat);
        }
        return false;
    }

    public static void stopViewfinderFeed(int handle) throws InterruptedException {
        EmulatorCamera cam = getCameraWith(handle);
        if (cam != null) {
            cam.stopViewfinderFeed();
        }
    }

    public static Size[] getOutputImageSizes(int handle) {
        EmulatorCamera cam = getCameraWith(handle);
        if (cam != null) {
            return cam.getOutputImageSizes();
        }
        return null;
    }

    @SuppressLint("RestrictedApi")
    public static boolean isCameraFacingFront(int handle) {
        EmulatorCamera cam = getCameraWith(handle);
        if (cam != null) {
            return cam.getCameraSelector().getLensFacing() == CameraSelector.LENS_FACING_FRONT;
        }
        return false;
    }

    public static int[] getSupportedImageOutputFormats() {
        return new int[]{ FORMAT_DRIVER_ARGB8888, FORMAT_DRIVER_JPEG, FORMAT_DRIVER_RGB565,
                FORMAT_DRIVER_FBS_BMP_64K, FORMAT_DRIVER_FBS_BMP_16M, FORMAT_DRIVER_FBS_BMP_16MU,
                FORMAT_DRIVER_EXIF };
    }

    /* END WRAPPER */

    @SuppressLint({"RestrictedApi", "UnsafeOptInUsageError"})
    public EmulatorCamera(int index) throws InterruptedException {
        this.flashMode = ImageCapture.FLASH_MODE_OFF;
        this.index = index;
        this.pendingImageCapture = false;
        this.pendingViewfinder = false;
        this.configChanged = false;
        this.previousResolutionIndex = -1;

        // Retrieve possible output sizes
        characteristics = Camera2CameraInfo.extractCameraCharacteristics(getCameraSelector().filter(cameraProvider.getAvailableCameraInfos()).get(0));
        StreamConfigurationMap streamConfigurationMap = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        outputImageSizes = streamConfigurationMap.getOutputSizes(ImageFormat.JPEG);
        cameraExecutor = Executors.newSingleThreadExecutor();
    }

    public void handleOrientationChange() {
        configChanged = true;

        if ((camera != null)) {
            if (pendingViewfinder)  {
                if (currentAnalysis != null) {
                    try {
                        stopViewfinderFeed();
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                    receiveViewfinderFeed(previousSize.getWidth(), previousSize.getHeight(), previousViewfinderFormat);
                }
            }
        }
    }

    private CameraSelector getCameraSelector() {
        return (index == 0) ? CameraSelector.DEFAULT_BACK_CAMERA : CameraSelector.DEFAULT_FRONT_CAMERA;
    }

    public boolean setFlashMode(int flashMode) {
        int previousFlashMode = this.flashMode;
        switch (flashMode) {
            case FLASH_MODE_DRIVER_NONE:
                this.flashMode = ImageCapture.FLASH_MODE_OFF;
                break;

            case FLASH_MODE_DRIVER_AUTO:
                this.flashMode = ImageCapture.FLASH_MODE_AUTO;
                break;

            case FLASH_MODE_DRIVER_FORCED:
                this.flashMode = ImageCapture.FLASH_MODE_ON;
                break;

            case FLASH_MODE_DRIVER_VIDEO_LIGHT:
                this.flashMode = FLASH_MODE_SPECIAL_TORCH;
                return true;

            default:
                return false;
        }

        if (previousFlashMode != flashMode) {
            configChanged = true;
        }

        return true;
    }

    public int getFlashMode() {
        if (this.flashMode == FLASH_MODE_SPECIAL_TORCH) {
            return FLASH_MODE_DRIVER_VIDEO_LIGHT;
        }

        if (this.flashMode == ImageCapture.FLASH_MODE_OFF) {
            return FLASH_MODE_DRIVER_NONE;
        }

        if (this.flashMode == ImageCapture.FLASH_MODE_ON) {
            return FLASH_MODE_DRIVER_FORCED;
        }

        return FLASH_MODE_DRIVER_AUTO;
    }

    private boolean isSupportedFormat(int requestedFormat) {
        return ((requestedFormat == FORMAT_DRIVER_ARGB8888) || (requestedFormat == FORMAT_DRIVER_RGB565) ||
                (requestedFormat == FORMAT_DRIVER_JPEG) || (requestedFormat == FORMAT_DRIVER_FBS_BMP_64K) ||
                (requestedFormat == FORMAT_DRIVER_FBS_BMP_16M) || (requestedFormat == FORMAT_DRIVER_FBS_BMP_16MU) ||
                (requestedFormat == FORMAT_DRIVER_EXIF));
    }

    public Size[] getOutputImageSizes() {
        return outputImageSizes;
    }

    public void stopViewfinderFeed() throws InterruptedException {
        if (!pendingViewfinder) {
            Log.w(TAG, "No operation is active on this camera!");
            return;
        }

        if (currentAnalysis == null) {
            Log.w(TAG, "No analysis is pending!");
            return;
        }
        if (Looper.getMainLooper().getThread() == Thread.currentThread()) {
            cameraProvider.unbind(currentAnalysis);
            pendingViewfinder = false;
        } else {
            Runnable unbindSync = new Runnable() {
                @Override
                public void run() {
                    cameraProvider.unbind(currentAnalysis);
                    pendingViewfinder = false;

                    synchronized (this) {
                        this.notify();
                    }
                }
            };

            synchronized (unbindSync) {
                applicationActivity.runOnUiThread(unbindSync);
                unbindSync.wait();
            }
        }
    }

    private int getTargetCaptureRotation() {
        int rotation = applicationActivity.getWindowManager().getDefaultDisplay().getRotation() * 90;
        int cameraOrientation = characteristics.get(CameraCharacteristics.SENSOR_ORIENTATION);

        if (index == 1) {   // Back camera
            return (360 + cameraOrientation - rotation) % 360;
        }

        return (cameraOrientation + rotation) % 360;
    }

    public boolean receiveViewfinderFeed(int width, int height, int requestedFormat) {
        if (pendingViewfinder) {
            Log.w(TAG, "Another operation is active on this camera!");
            return false;
        }

        if ((width <= 0) || (height <= 0)) {
            Log.w(TAG, "Invalid width/height argument value!");
            return false;
        }

        if (!isSupportedFormat(requestedFormat)) {
            Log.w(TAG, "Image capture format " + requestedFormat + " is not supported!");
            return false;
        }

        previousViewfinderFormat = requestedFormat;
        previousSize = new Size(width, height);
        pendingViewfinder = true;

        applicationActivity.runOnUiThread(() -> {
            Size sizeFinned = new Size(width, height);

            currentAnalysis = new ImageAnalysis.Builder()
                    .setTargetResolution(sizeFinned)
                    .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_RGBA_8888)
                    .build();

            camera = cameraProvider.bindToLifecycle(applicationActivity, getCameraSelector(), currentAnalysis);
            currentAnalysis.getTargetRotation();

            currentAnalysis.setAnalyzer(cameraExecutor,
                    image -> {
                        if (!doesCameraAllowNewFrame(index)) {
                            image.close();
                            return;
                        }

                        ByteBuffer imageBuffer = image.getPlanes()[0].getBuffer();

                        Size sizeRotatedReceived = new Size(image.getWidth(), image.getHeight());
                        int rotationGivenByImage = image.getImageInfo().getRotationDegrees();

                        Bitmap finalBitmap = null;
                        int stride = image.getPlanes()[0].getRowStride();

                        if (rotationGivenByImage != 0) {
                            Bitmap bmp;

                            if (sizeRotatedReceived.getWidth() != stride) {
                                bmp = Bitmap.createBitmap(stride / image.getPlanes()[0].getPixelStride(), sizeRotatedReceived.getHeight(), Bitmap.Config.ARGB_8888);
                                bmp.copyPixelsFromBuffer(imageBuffer);
                            } else {
                                bmp = Bitmap.createBitmap(sizeRotatedReceived.getWidth(), sizeRotatedReceived.getHeight(), Bitmap.Config.ARGB_8888);
                                bmp.copyPixelsFromBuffer(imageBuffer);
                            }

                            Matrix bmpMat = new Matrix();
                            bmpMat.postRotate(rotationGivenByImage);

                            finalBitmap = Bitmap.createBitmap(bmp, 0, 0, sizeRotatedReceived.getWidth(), sizeRotatedReceived.getHeight(), bmpMat, true);
                        }

                        Size authenticateSize = sizeRotatedReceived;
                        if ((rotationGivenByImage % 180) != 0) {
                            authenticateSize = new Size(sizeRotatedReceived.getHeight(), sizeRotatedReceived.getWidth());
                        }

                        if (!authenticateSize.equals(sizeFinned)) {
                            if (finalBitmap == null) {
                                if (sizeRotatedReceived.getWidth() != stride) {

                                    finalBitmap = Bitmap.createBitmap(stride / image.getPlanes()[0].getPixelStride(), sizeRotatedReceived.getHeight(), Bitmap.Config.ARGB_8888);
                                    finalBitmap.copyPixelsFromBuffer(imageBuffer);

                                    // Not efficient but do the job )
                                    finalBitmap = Bitmap.createBitmap(finalBitmap, 0, 0, sizeRotatedReceived.getWidth(), sizeRotatedReceived.getHeight());
                                } else {
                                    finalBitmap = Bitmap.createBitmap(sizeRotatedReceived.getWidth(), sizeRotatedReceived.getHeight(), Bitmap.Config.ARGB_8888);
                                    finalBitmap.copyPixelsFromBuffer(imageBuffer);
                                }
                            }

                            finalBitmap = Bitmap.createScaledBitmap(finalBitmap, sizeFinned.getWidth(), sizeFinned.getHeight(), false);
                            stride = finalBitmap.getWidth() * 4;
                        }

                        if (finalBitmap != null) {
                            imageBuffer = ByteBuffer.allocate(finalBitmap.getRowBytes() * sizeFinned.getHeight());

                            finalBitmap.copyPixelsToBuffer(imageBuffer);
                            imageBuffer.position(0);
                        }

                        if (requestedFormat == FORMAT_DRIVER_FBS_BMP_64K) {
                            int byteWidth = (sizeFinned.getWidth() * 2 + 3) / 4 * 4;
                            byte []buffer = new byte[byteWidth * sizeFinned.getHeight()];

                            for (int i = 0; i < sizeFinned.getWidth(); i++) {
                                for (int j = 0; j < sizeFinned.getHeight(); j++) {
                                    short pixel565 = (short) ((((imageBuffer.get(j * stride + 3)) & 0xF8) >> 3) |
                                            ((imageBuffer.get(j * stride + 2) & 0xFC) << 5) |
                                            ((imageBuffer.get(j * stride + 1) & 0xF8) << 11));

                                    buffer[j * byteWidth + i * 2] = (byte) ((pixel565 >> 8) & 0xFF);
                                    buffer[j * byteWidth + i * 2 + 1] = (byte) (pixel565 & 0xFF);
                                }
                            }

                            onFrameViewfinderDelivered(index, buffer, buffer.length);
                        } else if (requestedFormat == FORMAT_DRIVER_FBS_BMP_16M) {
                            int byteWidth = (sizeFinned.getWidth() * 3 + 3) / 4 * 4;
                            byte []buffer = new byte[byteWidth * sizeFinned.getHeight()];

                            for (int i = 0; i < sizeFinned.getWidth(); i++) {
                                for (int j = 0; j < sizeFinned.getHeight(); j++) {
                                    buffer[j * byteWidth + i * 3] = imageBuffer.get(j * stride + i * 4 + 2);
                                    buffer[j * byteWidth + i * 3 + 1] = imageBuffer.get(j * stride + i * 4 + 1);
                                    buffer[j * byteWidth + i * 3 + 2] = imageBuffer.get(j * stride + i * 4);
                                }
                            }

                            onFrameViewfinderDelivered(index, buffer, buffer.length);
                        } else if (requestedFormat == FORMAT_DRIVER_FBS_BMP_16MU) {
                            int byteWidth = sizeFinned.getWidth() * 4;
                            byte []buffer = new byte[byteWidth * sizeFinned.getHeight()];

                            for (int i = 0; i < sizeFinned.getWidth(); i++) {
                                for (int j = 0; j < sizeFinned.getHeight(); j++) {
                                    buffer[j * byteWidth + i * 4] = imageBuffer.get(j * stride + i * 4 + 2);
                                    buffer[j * byteWidth + i * 4 + 1] = imageBuffer.get(j * stride + i * 4 + 1);
                                    buffer[j * byteWidth + i * 4 + 2] = imageBuffer.get(j * stride + i * 4);
                                    buffer[j * byteWidth + i * 4 + 3] = imageBuffer.get(j * stride + i * 4 + 3);
                                }
                            }

                            onFrameViewfinderDelivered(index, buffer, buffer.length);
                        }

                        image.close();
                    });
        });

        return true;
    }

    public boolean captureImage(int resolutionIndex, int requestedFormat) {
        if (pendingImageCapture) {
            Log.w(TAG, "Another operation is active on this camera!");
            return false;
        }

        if ((resolutionIndex < 0) || (resolutionIndex >= outputImageSizes.length)) {
            Log.w(TAG, "Resolution index is out-of-range (expect [0.." + outputImageSizes.length + "], got " + resolutionIndex);
            return false;
        }

        if (!isSupportedFormat(requestedFormat)) {
            Log.w(TAG, "Image capture format " + requestedFormat + " is not supported!");
            return false;
        }

        if (resolutionIndex != previousResolutionIndex) {
            previousResolutionIndex = resolutionIndex;
            configChanged = true;
        }

        previousImageFormat = requestedFormat;
        pendingImageCapture = true;

        applicationActivity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Size outputSize = outputImageSizes[resolutionIndex];

                if (configChanged || (camera == null)) {
                    if (configChanged && (previousImageCapture != null)) {
                        cameraProvider.unbind(previousImageCapture);
                    }

                    ImageCapture imageCaptureRequest = new ImageCapture.Builder()
                            .setFlashMode(flashMode)
                            .setTargetResolution(outputSize)
                            .build();

                    camera = cameraProvider.bindToLifecycle(applicationActivity, getCameraSelector(), imageCaptureRequest);

                    previousImageCapture = imageCaptureRequest;
                    configChanged = false;
                }

                CameraControl control = camera.getCameraControl();

                if (flashMode == FLASH_MODE_SPECIAL_TORCH) {
                    control.enableTorch(true);
                } else {
                    control.enableTorch(false);
                }

                Size finalOutputSize = outputSize;
                previousImageCapture.takePicture(cameraExecutor,
                        new ImageCapture.OnImageCapturedCallback() {
                            @Override
                            public void onCaptureSuccess(@NonNull ImageProxy image) {
                                ByteBuffer jpegBuffer = image.getPlanes()[0].getBuffer();
                                int imageRot = image.getImageInfo().getRotationDegrees();

                                Size finalSizeFin = ((imageRot % 180) != 0) ?
                                    new Size(image.getHeight(), image.getWidth()) :
                                    new Size(image.getWidth(), image.getHeight());

                                if ((requestedFormat == FORMAT_DRIVER_JPEG) || (requestedFormat == FORMAT_DRIVER_EXIF)) {
                                    if (!finalSizeFin.equals(finalOutputSize)) {
                                        byte []dataBufferDecoded = new byte[jpegBuffer.remaining()];
                                        jpegBuffer.get(dataBufferDecoded);

                                        Bitmap finalBitmap = BitmapFactory.decodeByteArray(dataBufferDecoded, 0,
                                                dataBufferDecoded.length, new BitmapFactory.Options());

                                        finalBitmap = Bitmap.createScaledBitmap(finalBitmap,
                                                finalOutputSize.getWidth(),
                                                finalOutputSize.getHeight(),
                                                false);

                                        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
                                        finalBitmap.compress(Bitmap.CompressFormat.JPEG, 50, bytes);

                                        onCaptureImageDelivered(index, bytes.toByteArray(), 0);
                                    } else {
                                        // Throw immediately
                                        onCaptureImageDelivered(index, jpegBuffer.array(), 0);
                                    }
                                } else {
                                    BitmapFactory.Options opt = new BitmapFactory.Options();
                                    switch (requestedFormat) {
                                        case FORMAT_DRIVER_RGB565:
                                        case FORMAT_DRIVER_FBS_BMP_64K:
                                            opt.inPreferredConfig = Bitmap.Config.RGB_565;
                                            break;

                                        case FORMAT_DRIVER_ARGB8888:
                                        case FORMAT_DRIVER_FBS_BMP_16MU:
                                            opt.inPreferredConfig = Bitmap.Config.ARGB_8888;
                                            break;

                                        default:
                                            break;
                                    }

                                    Bitmap decodedBitmap = null;

                                    try {
                                        byte []dataBufferDecoded = new byte[jpegBuffer.remaining()];
                                        jpegBuffer.get(dataBufferDecoded);

                                        decodedBitmap = BitmapFactory.decodeByteArray(dataBufferDecoded, 0,
                                                dataBufferDecoded.length, opt);
                                    } catch (Exception ex) {
                                        Log.i(TAG, "Decoding bitmap encountered exception: " + ex);
                                    }

                                    if (!finalSizeFin.equals(finalOutputSize)) {
                                        decodedBitmap = Bitmap.createScaledBitmap(decodedBitmap, finalOutputSize.getWidth(),
                                                finalOutputSize.getHeight(), false);
                                    }

                                    if (decodedBitmap == null) {
                                        Log.e(TAG, "Error decoding bitmap image, null encountered");
                                        onCaptureImageDelivered(index, null, - 1);
                                    } else {
                                        int size = decodedBitmap.getRowBytes() * decodedBitmap.getHeight();
                                        ByteBuffer byteBuffer = ByteBuffer.allocate(size);
                                        decodedBitmap.copyPixelsToBuffer(byteBuffer);

                                        onCaptureImageDelivered(index, byteBuffer.array(), 0);
                                    }
                                }

                                pendingImageCapture = false;
                                image.close();

                                super.onCaptureSuccess(image);
                            }

                            @Override
                            public void onError(@NonNull ImageCaptureException ex) {
                                Log.i(TAG, "Image capture encountered exception: " + ex);
                                onCaptureImageDelivered(index, null, -1);

                                pendingImageCapture = false;

                                super.onError(ex);
                            }
                        });
            }
        });

        return true;
    }

    private static native boolean doesCameraAllowNewFrame(int index);
    private static native void onCaptureImageDelivered(int index, byte[] rawData, int errorCode);
    public static native void onFrameViewfinderDelivered(int index, byte[] rawData, int errorCode);
}

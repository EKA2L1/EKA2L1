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

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

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

    private static List<EmulatorCamera> cameraWrappers;

    private int flashMode;
    private int index;
    private boolean pending;
    private boolean configChanged;
    private int previousResolutionIndex;
    private ImageAnalysis currentAnalysis;
    private ImageCapture previousImageCapture;
    private Size[] outputImageSizes;
    private Camera camera;
    private CameraCharacteristics characteristics;

    // Cached parameters for orientation change
    private int previousFormat = 0;
    private Size previousSize;

    public static void setActivity(AppCompatActivity activity) {
        applicationActivity = activity;
        cameraWrappers = new ArrayList<>();

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

            for (int i = 0; i < cameraWrappers.size(); i++) {
                if (cameraWrappers.get(i).index == index) {
                    return i;
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
            cameraWrappers.add(newCam);

            return cameraWrappers.size() - 1;
        } catch (Exception ex) {
            return -1;
        }
    }

    public static void handleOrientationChangeForAllInstances() {
        for (int i = 0; i < cameraWrappers.size(); i++) {
            cameraWrappers.get(i).handleOrientationChange();
        }
    }

    /* WRAPPER */
    public static EmulatorCamera getCameraWith(int handle) {
        if ((handle < 0) || (handle >= cameraWrappers.size())) {
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
        this.pending = false;
        this.configChanged = false;
        this.previousResolutionIndex = -1;

        // Retrieve possible output sizes
        characteristics = Camera2CameraInfo.extractCameraCharacteristics(getCameraSelector().filter(cameraProvider.getAvailableCameraInfos()).get(0));
        StreamConfigurationMap streamConfigurationMap = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        outputImageSizes = streamConfigurationMap.getOutputSizes(ImageFormat.JPEG);
    }

    public void handleOrientationChange() {
        configChanged = true;

        if ((camera != null) && pending) {
            if (currentAnalysis != null) {
                try {
                    stopViewfinderFeed();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                receiveViewfinderFeed(previousSize.getWidth(), previousSize.getHeight(), previousFormat);
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
        if (!pending) {
            Log.w(TAG, "No operation is active on this camera!");
            return;
        }

        if (currentAnalysis == null) {
            Log.w(TAG, "No analysis is pending!");
            return;
        }
        if (Looper.getMainLooper().getThread() == Thread.currentThread()) {
            cameraProvider.unbind(currentAnalysis);
            pending = false;
        } else {
            Runnable unbindSync = new Runnable() {
                @Override
                public void run() {
                    cameraProvider.unbind(currentAnalysis);
                    pending = false;

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
        if (pending) {
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

        previousFormat = requestedFormat;
        previousSize = new Size(width, height);
        pending = true;

        applicationActivity.runOnUiThread(() -> {
            int rotation = getTargetCaptureRotation();
            Size sizeRotated = ((rotation % 180) != 0) ? new Size(height, width) : new Size(width, height);

            currentAnalysis = new ImageAnalysis.Builder()
                    .setTargetResolution(sizeRotated)
                    .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_RGBA_8888)
                    .build();

            camera = cameraProvider.bindToLifecycle(applicationActivity, getCameraSelector(), currentAnalysis);
            currentAnalysis.getTargetRotation();

            currentAnalysis.setAnalyzer(ContextCompat.getMainExecutor(applicationActivity),
                    image -> {
                        if (!doesCameraAllowNewFrame(index)) {
                            image.close();
                            return;
                        }

                        ByteBuffer imageBuffer = image.getPlanes()[0].getBuffer();
                        int rotationGivenByImage = image.getImageInfo().getRotationDegrees();

                        if (rotationGivenByImage != 0) {
                            Bitmap bmp = Bitmap.createBitmap(sizeRotated.getWidth(), sizeRotated.getHeight(), Bitmap.Config.ARGB_8888);
                            bmp.copyPixelsFromBuffer(imageBuffer);

                            Matrix bmpMat = new Matrix();
                            bmpMat.postRotate(rotationGivenByImage);

                            Bitmap bmpRotated = Bitmap.createBitmap(bmp, 0, 0, sizeRotated.getWidth(), sizeRotated.getHeight(), bmpMat, true);
                            imageBuffer = ByteBuffer.allocate(bmpRotated.getRowBytes() * height);

                            bmpRotated.copyPixelsToBuffer(imageBuffer);
                            imageBuffer.position(0);
                        }

                        if (requestedFormat == FORMAT_DRIVER_FBS_BMP_64K) {
                            int byteWidth = (width * 2 + 3) / 4 * 4;
                            byte []buffer = new byte[byteWidth * height];

                            for (int i = 0; i < width; i++) {
                                for (int j = 0; j < height; j++) {
                                    short pixel565 = (short) ((((imageBuffer.get(j * width * 4 + 3)) & 0xF8) >> 3) |
                                            ((imageBuffer.get(j * width * 4 + 2) & 0xFC) << 5) |
                                            ((imageBuffer.get(j * width * 4 + 1) & 0xF8) << 11));

                                    buffer[j * byteWidth + i * 2] = (byte) ((pixel565 >> 8) & 0xFF);
                                    buffer[j * byteWidth + i * 2 + 1] = (byte) (pixel565 & 0xFF);
                                }
                            }

                            onCaptureImageDelivered(index, buffer, buffer.length);
                        } else if (requestedFormat == FORMAT_DRIVER_FBS_BMP_16M) {
                            int byteWidth = (width * 3 + 3) / 4 * 4;
                            byte []buffer = new byte[byteWidth * height];

                            for (int i = 0; i < width; i++) {
                                for (int j = 0; j < height; j++) {
                                    buffer[j * byteWidth + i * 3] = imageBuffer.get(j * width * 4 + i * 4 + 1);
                                    buffer[j * byteWidth + i * 3 + 1] = imageBuffer.get(j * width * 4 + i * 4 + 2);
                                    buffer[j * byteWidth + i * 3 + 2] = imageBuffer.get(j * width * 4 + i * 4 + 3);
                                }
                            }

                            onCaptureImageDelivered(index, buffer, buffer.length);
                        } else if (requestedFormat == FORMAT_DRIVER_FBS_BMP_16MU) {
                            // Copy the buffer directly
                            byte []imageBufferArr = new byte[imageBuffer.remaining()];
                            imageBuffer.get(imageBufferArr);

                            onCaptureImageDelivered(index, imageBufferArr, imageBufferArr.length);
                        }

                        image.close();
                    });
        });

        return true;
    }

    public boolean captureImage(int resolutionIndex, int requestedFormat) {
        if (pending) {
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

        previousFormat = requestedFormat;
        pending = true;

        applicationActivity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (configChanged || (camera == null)) {
                    if (configChanged && (previousImageCapture != null)) {
                        cameraProvider.unbind(previousImageCapture);
                    }

                    int rotation = getTargetCaptureRotation();
                    Size outputSize = outputImageSizes[resolutionIndex];

                    if ((rotation % 180) != 0) {
                        outputSize = new Size(outputImageSizes[resolutionIndex].getHeight(), outputImageSizes[resolutionIndex].getWidth());
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

                previousImageCapture.takePicture(ContextCompat.getMainExecutor(applicationActivity),
                        new ImageCapture.OnImageCapturedCallback() {
                            @Override
                            public void onCaptureSuccess(@NonNull ImageProxy image) {
                                ByteBuffer jpegBuffer = image.getPlanes()[0].getBuffer();
                                int imageRot = image.getImageInfo().getRotationDegrees();

                                if ((requestedFormat == FORMAT_DRIVER_JPEG) || (requestedFormat == FORMAT_DRIVER_EXIF)) {
                                    // Throw immediately
                                    onCaptureImageDelivered(index, jpegBuffer.array(), 0);
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

                                    if (imageRot != 0) {
                                        Matrix rotMatrix = new Matrix();
                                        rotMatrix.postRotate(imageRot);

                                        decodedBitmap = Bitmap.createBitmap(decodedBitmap, 0, 0,
                                                outputImageSizes[resolutionIndex].getWidth(),
                                                outputImageSizes[resolutionIndex].getHeight(),
                                                rotMatrix, true);
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

                                pending = false;
                                image.close();

                                super.onCaptureSuccess(image);
                            }

                            @Override
                            public void onError(@NonNull ImageCaptureException ex) {
                                Log.i(TAG, "Image capture encountered exception: " + ex);
                                onCaptureImageDelivered(index, null, -1);

                                pending = false;

                                super.onError(ex);
                            }
                        });
            }
        });

        return true;
    }

    private static native boolean doesCameraAllowNewFrame(int index);
    private static native void onCaptureImageDelivered(int index, byte[] rawData, int errorCode);
}

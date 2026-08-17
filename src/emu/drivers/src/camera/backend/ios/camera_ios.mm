/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#import <AVFoundation/AVFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <ImageIO/ImageIO.h>

#include <drivers/camera/backend/ios/camera_ios.h>
#include <drivers/camera/backend/ios/camera_pixel_ios.h>

#include <common/log.h>

#include <algorithm>

using eka2l1::drivers::camera::instance_ios;

@interface EKACameraDevice : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate,
    AVCapturePhotoCaptureDelegate>

+ (BOOL)ensureCameraPermission;
+ (BOOL)hasCameraAtIndex:(int)index;

- (instancetype)initWithIndex:(int)index;
- (BOOL)valid;
- (BOOL)facingFront;
- (void)setOwner:(instance_ios *)owner;
- (void)clearOwner;
- (std::vector<eka2l1::vec2>)outputImageSizes;

- (BOOL)startViewfinderWithWidth:(int)width height:(int)height
                          format:(eka2l1::drivers::camera::frame_format)format;
- (void)stopViewfinder;
- (BOOL)captureWithResolutionIndex:(int)resolutionIndex
                            format:(eka2l1::drivers::camera::frame_format)format
                   flashModeDriver:(int)flashModeDriver;
- (void)shutdown;

@end

@implementation EKACameraDevice {
    AVCaptureSession *_session;
    AVCaptureDevice *_device;
    AVCaptureDeviceInput *_input;
    AVCaptureVideoDataOutput *_videoOutput;
    AVCapturePhotoOutput *_photoOutput;

    // Session mutations run on _sessionQueue; viewfinder frames arrive on
    // _videoQueue. Keeping them separate means a stop request never waits
    // behind an in-flight frame conversion.
    dispatch_queue_t _sessionQueue;
    dispatch_queue_t _videoQueue;

    // _owner is only read/cleared under _ownerLock. Deliveries into the owner
    // happen while holding it: the owner's own delivery path releases every
    // lock before invoking guest callbacks, so this lock is never held across
    // a kernel-lock acquisition initiated here except through those (already
    // copy-then-invoke) paths.
    std::mutex _ownerLock;
    instance_ios *_owner;

    std::vector<eka2l1::vec2> _imageSizes;

    int _viewfinderWidth;
    int _viewfinderHeight;
    eka2l1::drivers::camera::frame_format _viewfinderFormat;
    BOOL _viewfinderActive;

    eka2l1::drivers::camera::frame_format _captureFormat;
    int _captureWidth;
    int _captureHeight;
    int _captureFlashModeDriver;
    BOOL _capturePending;
}

+ (AVCaptureDevice *)deviceAtIndex:(int)index {
    // Index semantics mirror the Android backend: 0 = back, 1 = front.
    const AVCaptureDevicePosition position = (index == 0)
        ? AVCaptureDevicePositionBack : AVCaptureDevicePositionFront;
    return [AVCaptureDevice defaultDeviceWithDeviceType:AVCaptureDeviceTypeBuiltInWideAngleCamera
                                              mediaType:AVMediaTypeVideo
                                               position:position];
}

+ (BOOL)hasCameraAtIndex:(int)index {
    return [self deviceAtIndex:index] != nil;
}

+ (BOOL)ensureCameraPermission {
    const AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    if (status == AVAuthorizationStatusAuthorized) {
        return YES;
    }

    if (status == AVAuthorizationStatusNotDetermined) {
        // The caller (ecam_create) drops the kernel lock around camera
        // creation exactly so this prompt can block the guest thread.
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        __block BOOL granted = NO;

        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
                                 completionHandler:^(BOOL result) {
            granted = result;
            dispatch_semaphore_signal(semaphore);
        }];

        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        return granted;
    }

    return NO;
}

- (instancetype)initWithIndex:(int)index {
    self = [super init];
    if (!self) {
        return nil;
    }

    _owner = nullptr;
    _viewfinderActive = NO;
    _capturePending = NO;
    _captureFlashModeDriver = 0;

    _device = [EKACameraDevice deviceAtIndex:index];
    if (!_device) {
        LOG_ERROR(eka2l1::DRIVER_CAM, "No capture device available for camera index {}", index);
        return self;
    }

    NSError *error = nil;
    _input = [AVCaptureDeviceInput deviceInputWithDevice:_device error:&error];
    if (!_input) {
        LOG_ERROR(eka2l1::DRIVER_CAM, "Unable to create capture input: {}",
            error.localizedDescription.UTF8String);
        _device = nil;
        return self;
    }

    _session = [[AVCaptureSession alloc] init];
    _sessionQueue = dispatch_queue_create("com.eka2l1.camera.session", DISPATCH_QUEUE_SERIAL);
    _videoQueue = dispatch_queue_create("com.eka2l1.camera.video", DISPATCH_QUEUE_SERIAL);

    _photoOutput = [[AVCapturePhotoOutput alloc] init];

    [_session beginConfiguration];
    if ([_session canAddInput:_input]) {
        [_session addInput:_input];
    }
    if ([_session canAddOutput:_photoOutput]) {
        [_session addOutput:_photoOutput];
    }
    [_session commitConfiguration];

    [self buildImageSizeLadder];
    return self;
}

- (BOOL)valid {
    return _device != nil;
}

- (BOOL)facingFront {
    return _device.position == AVCaptureDevicePositionFront;
}

- (void)setOwner:(instance_ios *)owner {
    const std::lock_guard<std::mutex> guard(_ownerLock);
    _owner = owner;
}

- (void)clearOwner {
    const std::lock_guard<std::mutex> guard(_ownerLock);
    _owner = nullptr;
}

// Advertise a ladder of Symbian-era capture sizes clamped to the sensor's
// maximum, largest first — the same shape (descending concrete sizes) the
// Android backend gets from Camera2's stream configuration map.
- (void)buildImageSizeLadder {
    CMVideoDimensions max_dims = { 640, 480 };

    for (NSValue *value in _device.activeFormat.supportedMaxPhotoDimensions) {
        CMVideoDimensions dims;
        [value getValue:&dims];
        if ((static_cast<std::int64_t>(dims.width) * dims.height) >
            (static_cast<std::int64_t>(max_dims.width) * max_dims.height)) {
            max_dims = dims;
        }
    }

    static const eka2l1::vec2 CANDIDATES[] = {
        eka2l1::vec2(2592, 1944), eka2l1::vec2(2048, 1536), eka2l1::vec2(1600, 1200),
        eka2l1::vec2(1280, 960), eka2l1::vec2(1024, 768), eka2l1::vec2(640, 480),
        eka2l1::vec2(320, 240), eka2l1::vec2(160, 120)
    };

    _imageSizes.clear();
    _imageSizes.push_back(eka2l1::vec2(max_dims.width, max_dims.height));

    for (const eka2l1::vec2 &candidate: CANDIDATES) {
        if ((candidate.x <= max_dims.width) && (candidate.y <= max_dims.height) &&
            !((candidate.x == max_dims.width) && (candidate.y == max_dims.height))) {
            _imageSizes.push_back(candidate);
        }
    }
}

- (std::vector<eka2l1::vec2>)outputImageSizes {
    return _imageSizes;
}

- (void)orientConnection:(AVCaptureConnection *)connection {
    if (!connection) {
        return;
    }

    // Deliver upright (portrait) pixels, matching what the Android backend
    // produces after rotating by the Camera2-reported rotation degrees.
    if (@available(iOS 17.0, *)) {
        if ([connection isVideoRotationAngleSupported:90.0]) {
            connection.videoRotationAngle = 90.0;
        }
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        if (connection.supportsVideoOrientation) {
            connection.videoOrientation = AVCaptureVideoOrientationPortrait;
        }
#pragma clang diagnostic pop
    }
}

- (void)applySessionPresetCovering:(int)width height:(int)height {
    struct preset_candidate {
        AVCaptureSessionPreset preset;
        int width;
        int height;
    };

    const preset_candidate candidates[] = {
        { AVCaptureSessionPreset352x288, 352, 288 },
        { AVCaptureSessionPreset640x480, 640, 480 },
        { AVCaptureSessionPreset1280x720, 1280, 720 },
        { AVCaptureSessionPreset1920x1080, 1920, 1080 },
    };

    // The rotated (portrait) buffer swaps width/height relative to the
    // preset's landscape dimensions; cover the larger requested edge.
    const int long_edge = std::max(width, height);
    const int short_edge = std::min(width, height);

    for (const preset_candidate &candidate: candidates) {
        if ((candidate.width >= long_edge) && (candidate.height >= short_edge) &&
            [_session canSetSessionPreset:candidate.preset]) {
            _session.sessionPreset = candidate.preset;
            return;
        }
    }
}

- (BOOL)startViewfinderWithWidth:(int)width height:(int)height
                          format:(eka2l1::drivers::camera::frame_format)format {
    if (!_device) {
        return NO;
    }

    __block BOOL succeeded = NO;

    dispatch_sync(_sessionQueue, ^{
        if (self->_viewfinderActive) {
            return;
        }

        self->_viewfinderWidth = width;
        self->_viewfinderHeight = height;
        self->_viewfinderFormat = format;

        [self->_session beginConfiguration];
        [self applySessionPresetCovering:width height:height];

        if (!self->_videoOutput) {
            self->_videoOutput = [[AVCaptureVideoDataOutput alloc] init];
            self->_videoOutput.videoSettings = @{
                (id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)
            };
            self->_videoOutput.alwaysDiscardsLateVideoFrames = YES;
            [self->_videoOutput setSampleBufferDelegate:self queue:self->_videoQueue];
        }

        if (![self->_session.outputs containsObject:self->_videoOutput]) {
            if (![self->_session canAddOutput:self->_videoOutput]) {
                [self->_session commitConfiguration];
                return;
            }

            [self->_session addOutput:self->_videoOutput];
        }

        [self orientConnection:[self->_videoOutput connectionWithMediaType:AVMediaTypeVideo]];
        [self->_session commitConfiguration];

        if (!self->_session.running) {
            [self->_session startRunning];
        }

        self->_viewfinderActive = YES;
        succeeded = YES;
    });

    return succeeded;
}

- (void)stopViewfinder {
    // Must not block the caller: stop_viewfinder_feed()/release() run inside an
    // ECam HLE dispatch while the emulator kernel lock is held. AVCaptureSession
    // -stopRunning is a blocking call that internally spins a run-loop condition
    // and can wait on the main thread; if we block here the kernel lock is held
    // across it, stalling the UI/input and timing threads (which both need the
    // lock) and tripping the iOS watchdog. Frame delivery is already gated by the
    // owner's callbacks, which the caller clears before invoking this, so tearing
    // the session down asynchronously delivers no stray frames.
    dispatch_async(_sessionQueue, ^{
        if (!self->_viewfinderActive) {
            return;
        }

        [self->_session beginConfiguration];
        if (self->_videoOutput && [self->_session.outputs containsObject:self->_videoOutput]) {
            [self->_session removeOutput:self->_videoOutput];
        }
        [self->_session commitConfiguration];

        self->_viewfinderActive = NO;

        if (!self->_capturePending && self->_session.running) {
            [self->_session stopRunning];
        }
    });
}

- (BOOL)captureWithResolutionIndex:(int)resolutionIndex
                            format:(eka2l1::drivers::camera::frame_format)format
                   flashModeDriver:(int)flashModeDriver {
    if (!_device) {
        return NO;
    }

    if ((resolutionIndex < 0) || (resolutionIndex >= static_cast<int>(_imageSizes.size()))) {
        LOG_ERROR(eka2l1::DRIVER_CAM, "Capture resolution index {} out of range!", resolutionIndex);
        return NO;
    }

    _captureFormat = format;
    _captureWidth = _imageSizes[resolutionIndex].x;
    _captureHeight = _imageSizes[resolutionIndex].y;
    _captureFlashModeDriver = flashModeDriver;
    _capturePending = YES;

    dispatch_async(_sessionQueue, ^{
        if (!self->_session.running) {
            [self->_session startRunning];
        }

        // FLASH_MODE_VIDEO_LIGHT maps to the torch, like the Android backend
        // does when a capture is requested with it.
        if (self->_device.hasTorch && [self->_device lockForConfiguration:nil]) {
            self->_device.torchMode =
                (self->_captureFlashModeDriver == eka2l1::drivers::camera::FLASH_MODE_VIDEO_LIGHT)
                ? AVCaptureTorchModeOn : AVCaptureTorchModeOff;
            [self->_device unlockForConfiguration];
        }

        AVCapturePhotoSettings *settings = [AVCapturePhotoSettings
            photoSettingsWithFormat:@{ AVVideoCodecKey: AVVideoCodecTypeJPEG }];

        if (self->_device.hasFlash) {
            AVCaptureFlashMode flash_mode = AVCaptureFlashModeOff;
            switch (self->_captureFlashModeDriver) {
            case eka2l1::drivers::camera::FLASH_MODE_AUTO:
                flash_mode = AVCaptureFlashModeAuto;
                break;
            case eka2l1::drivers::camera::FLASH_MODE_FORCED:
                flash_mode = AVCaptureFlashModeOn;
                break;
            default:
                break;
            }

            if ([self->_photoOutput.supportedFlashModes containsObject:@(flash_mode)]) {
                settings.flashMode = flash_mode;
            }
        }

        [self orientConnection:[self->_photoOutput connectionWithMediaType:AVMediaTypeVideo]];
        [self->_photoOutput capturePhotoWithSettings:settings delegate:self];
    });

    return YES;
}

- (void)shutdown {
    // Asynchronous for the same reason as -stopViewfinder: the owning
    // instance_ios is torn down from an ECam HLE dispatch holding the kernel
    // lock, and a blocking -stopRunning here would deadlock the UI/input and
    // timing threads. The block retains self, so the session is stopped and
    // released after this returns.
    dispatch_async(_sessionQueue, ^{
        if (self->_session.running) {
            [self->_session stopRunning];
        }
    });
}

// MARK: Viewfinder frames

- (void)captureOutput:(AVCaptureOutput *)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection *)connection {
    instance_ios *owner = nullptr;

    {
        const std::lock_guard<std::mutex> guard(_ownerLock);
        owner = _owner;
        if (!owner || !owner->wants_new_frame()) {
            return;
        }
    }

    CVImageBufferRef pixel_buffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!pixel_buffer) {
        return;
    }

    CVPixelBufferLockBaseAddress(pixel_buffer, kCVPixelBufferLock_ReadOnly);

    const std::uint8_t *base = static_cast<const std::uint8_t *>(
        CVPixelBufferGetBaseAddress(pixel_buffer));
    const std::size_t stride = CVPixelBufferGetBytesPerRow(pixel_buffer);
    const int source_width = static_cast<int>(CVPixelBufferGetWidth(pixel_buffer));
    const int source_height = static_cast<int>(CVPixelBufferGetHeight(pixel_buffer));

    std::vector<std::uint8_t> converted;
    bool converted_ok = false;

    if ((source_width == _viewfinderWidth) && (source_height == _viewfinderHeight)) {
        converted_ok = eka2l1::drivers::camera::ios_convert_bgra_to_guest(base, stride,
            _viewfinderWidth, _viewfinderHeight, _viewfinderFormat, converted);
    } else {
        CGImageRef source_image = eka2l1::drivers::camera::ios_create_cgimage_from_bgra(base,
            stride, source_width, source_height);

        if (source_image) {
            std::vector<std::uint8_t> scaled;
            if (eka2l1::drivers::camera::ios_render_cgimage_to_bgra(source_image, _viewfinderWidth,
                _viewfinderHeight, scaled)) {
                converted_ok = eka2l1::drivers::camera::ios_convert_bgra_to_guest(scaled.data(),
                    static_cast<std::size_t>(_viewfinderWidth) * 4, _viewfinderWidth,
                    _viewfinderHeight, _viewfinderFormat, converted);
            }

            CGImageRelease(source_image);
        }
    }

    CVPixelBufferUnlockBaseAddress(pixel_buffer, kCVPixelBufferLock_ReadOnly);

    if (!converted_ok) {
        return;
    }

    const std::lock_guard<std::mutex> guard(_ownerLock);
    if (_owner) {
        _owner->deliver_viewfinder_frame(converted.data(), converted.size(), 0);
    }
}

// MARK: Still capture

- (void)deliverCaptureBytes:(const void *)bytes size:(std::size_t)size error:(int)error {
    _capturePending = NO;

    {
        const std::lock_guard<std::mutex> guard(_ownerLock);
        if (_owner) {
            _owner->deliver_captured_image(bytes, size, error);
        }
    }

    dispatch_async(_sessionQueue, ^{
        if (!self->_viewfinderActive && !self->_capturePending && self->_session.running) {
            [self->_session stopRunning];
        }
    });
}

- (void)captureOutput:(AVCapturePhotoOutput *)output
    didFinishProcessingPhoto:(AVCapturePhoto *)photo
                       error:(NSError *)error {
    NSData *jpeg_data = [photo fileDataRepresentation];

    if (error || !jpeg_data) {
        LOG_ERROR(eka2l1::DRIVER_CAM, "Image capture failed: {}",
            error ? error.localizedDescription.UTF8String : "no data");
        [self deliverCaptureBytes:nullptr size:0 error:-1];
        return;
    }

    CGImageSourceRef source = CGImageSourceCreateWithData((__bridge CFDataRef)jpeg_data, nullptr);
    if (!source) {
        [self deliverCaptureBytes:nullptr size:0 error:-1];
        return;
    }

    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    CFRelease(source);

    if (!image) {
        [self deliverCaptureBytes:nullptr size:0 error:-1];
        return;
    }

    const int image_width = static_cast<int>(CGImageGetWidth(image));
    const int image_height = static_cast<int>(CGImageGetHeight(image));

    const eka2l1::drivers::camera::frame_format format = _captureFormat;

    if ((format == eka2l1::drivers::camera::FRAME_FORMAT_JPEG) ||
        (format == eka2l1::drivers::camera::FRAME_FORMAT_EXIF)) {
        if ((image_width == _captureWidth) && (image_height == _captureHeight)) {
            CGImageRelease(image);
            [self deliverCaptureBytes:jpeg_data.bytes size:jpeg_data.length error:0];
            return;
        }

        // Rescale, then re-encode — same behavior as the Android backend when
        // the sensor output size differs from the requested one.
        std::vector<std::uint8_t> scaled;
        bool ok = eka2l1::drivers::camera::ios_render_cgimage_to_bgra(image, _captureWidth,
            _captureHeight, scaled);
        CGImageRelease(image);

        CGImageRef scaled_image = nullptr;
        if (ok) {
            scaled_image = eka2l1::drivers::camera::ios_create_cgimage_from_bgra(scaled.data(),
                static_cast<std::size_t>(_captureWidth) * 4, _captureWidth, _captureHeight);
        }

        if (!scaled_image) {
            [self deliverCaptureBytes:nullptr size:0 error:-1];
            return;
        }

        NSMutableData *encoded = [NSMutableData data];
        CGImageDestinationRef destination = CGImageDestinationCreateWithData(
            (__bridge CFMutableDataRef)encoded, CFSTR("public.jpeg"), 1, nullptr);

        BOOL encode_ok = NO;
        if (destination) {
            NSDictionary *properties = @{
                (id)kCGImageDestinationLossyCompressionQuality: @(0.5)
            };
            CGImageDestinationAddImage(destination, scaled_image,
                (__bridge CFDictionaryRef)properties);
            encode_ok = CGImageDestinationFinalize(destination);
            CFRelease(destination);
        }

        CGImageRelease(scaled_image);

        if (encode_ok) {
            [self deliverCaptureBytes:encoded.bytes size:encoded.length error:0];
        } else {
            [self deliverCaptureBytes:nullptr size:0 error:-1];
        }

        return;
    }

    std::vector<std::uint8_t> bgra;
    const bool render_ok = eka2l1::drivers::camera::ios_render_cgimage_to_bgra(image, _captureWidth,
        _captureHeight, bgra);
    CGImageRelease(image);

    std::vector<std::uint8_t> converted;
    if (render_ok && eka2l1::drivers::camera::ios_convert_bgra_to_guest(bgra.data(),
        static_cast<std::size_t>(_captureWidth) * 4, _captureWidth, _captureHeight,
        format, converted)) {
        [self deliverCaptureBytes:converted.data() size:converted.size() error:0];
    } else {
        [self deliverCaptureBytes:nullptr size:0 error:-1];
    }
}

@end

namespace eka2l1::drivers::camera {
    static EKACameraDevice *device_of(void *holder) {
        return (__bridge EKACameraDevice *)holder;
    }

    std::uint32_t collection_ios::count() const {
        std::uint32_t result = 0;

        // Like Android: expose at most one back and one front camera.
        if ([EKACameraDevice hasCameraAtIndex:0]) {
            result++;
        }

        if ([EKACameraDevice hasCameraAtIndex:1]) {
            result++;
        }

        return result;
    }

    std::unique_ptr<instance> collection_ios::make_camera(const std::uint32_t camera_index) {
        if (![EKACameraDevice ensureCameraPermission]) {
            LOG_ERROR(DRIVER_CAM, "Camera permission denied, refusing camera instance!");
            return nullptr;
        }

        EKACameraDevice *device = [[EKACameraDevice alloc] initWithIndex:static_cast<int>(camera_index)];
        if (!device || ![device valid]) {
            LOG_ERROR(DRIVER_CAM, "Unable to open capture device for camera index {}", camera_index);
            return nullptr;
        }

        auto result = std::make_unique<instance_ios>(this, static_cast<int>(camera_index),
            (__bridge_retained void *)device);
        [device setOwner:result.get()];

        return result;
    }

    instance_ios::instance_ios(collection_ios *collection, const int index, void *device_holder)
        : collection_(collection)
        , index_(index)
        , device_holder_(device_holder)
        , active_capture_img_callback_(nullptr)
        , active_frame_viewfinder_callback_(nullptr)
        , wants_new_frame_callback_(nullptr)
        , stub_optical_zoom_(0)
        , stub_exposure_(EXPOSURE_MODE_AUTO)
        , stub_digital_zoom_(1)
        , stub_contrast_(0)
        , stub_brightness_(0)
        , stub_white_balance_(0)
        , flash_mode_(FLASH_MODE_OFF) {
    }

    instance_ios::~instance_ios() {
        release();

        EKACameraDevice *device = (__bridge_transfer EKACameraDevice *)device_holder_;
        [device clearOwner];
        [device shutdown];
        device = nil;

        device_holder_ = nullptr;
    }

    bool instance_ios::set_parameter(const parameter_key key, const std::uint32_t value) {
        switch (key) {
        case PARAMETER_KEY_OPTICAL_ZOOM:
            stub_optical_zoom_ = value;
            return true;

        case PARAMETER_KEY_FLASH:
            flash_mode_ = value;
            return true;

        case PARAMETER_KEY_EXPOSURE:
            stub_exposure_ = value;
            return true;

        case PARAMETER_KEY_DIGITAL_ZOOM:
            stub_digital_zoom_ = value;
            return true;

        case PARAMETER_KEY_CONTRAST:
            stub_contrast_ = value;
            return true;

        case PARAMETER_KEY_BRIGHTNESS:
            stub_brightness_ = value;
            return true;

        case PARAMETER_KEY_WHITE_BALANCE:
            stub_white_balance_ = value;
            return true;

        default:
            LOG_WARN(DRIVER_CAM, "Unsupported parameter key {} to set value", static_cast<int>(key));
            break;
        }

        return false;
    }

    bool instance_ios::get_parameter(const parameter_key key, std::uint32_t &value) {
        switch (key) {
        case PARAMETER_KEY_OPTICAL_ZOOM:
            value = stub_optical_zoom_;
            break;

        case PARAMETER_KEY_FLASH:
            value = flash_mode_;
            break;

        case PARAMETER_KEY_EXPOSURE:
            value = stub_exposure_;
            break;

        case PARAMETER_KEY_DIGITAL_ZOOM:
            value = stub_digital_zoom_;
            break;

        case PARAMETER_KEY_CONTRAST:
            value = stub_contrast_;
            break;

        case PARAMETER_KEY_BRIGHTNESS:
            value = stub_brightness_;
            break;

        case PARAMETER_KEY_WHITE_BALANCE:
            value = stub_white_balance_;
            break;

        default:
            LOG_WARN(DRIVER_CAM, "Unsupported parameter key {} to get value", static_cast<int>(key));
            return false;
        }

        return true;
    }

    std::vector<frame_format> instance_ios::supported_frame_formats() {
        return std::vector<frame_format>(std::begin(IOS_SUPPORTED_FORMATS), std::end(IOS_SUPPORTED_FORMATS));
    }

    std::vector<eka2l1::vec2> instance_ios::supported_output_image_sizes(const frame_format frame_format) {
        return [device_of(device_holder_) outputImageSizes];
    }

    bool instance_ios::reserve() {
        const std::lock_guard<std::mutex> guard(collection_->reserve_lock_);

        if (collection_->current_reserved_[index_] != nullptr) {
            LOG_ERROR(DRIVER_CAM, "Another camera instance is currently reserved the camera for operations!");
            return false;
        }

        collection_->current_reserved_[index_] = this;
        return true;
    }

    void instance_ios::release() {
        stop_viewfinder_feed_impl(false);

        const std::lock_guard<std::mutex> guard(collection_->reserve_lock_);

        if (collection_->current_reserved_[index_] == this) {
            collection_->current_reserved_[index_] = nullptr;
        }
    }

    info instance_ios::get_info() {
        EKACameraDevice *device = device_of(device_holder_);

        info result;
        result.camera_direction_ = [device facingFront] ? DIRECTION_FRONT : DIRECTION_BACK;
        result.num_image_sizes_supported_ = static_cast<std::int32_t>([device outputImageSizes].size());
        result.flash_modes_supported_ = FLASH_MODE_OFF | FLASH_MODE_AUTO | FLASH_MODE_FORCED | FLASH_MODE_VIDEO_LIGHT;
        result.options_supported_ = CAPTURE_OPTION_ALL;
        result.supported_image_formats_ = 0;

        for (const frame_format format: IOS_SUPPORTED_FORMATS) {
            result.supported_image_formats_ |= static_cast<std::uint32_t>(format);
        }

        return result;
    }

    void instance_ios::receive_viewfinder_feed(const eka2l1::vec2 &size, const frame_format format,
        camera_wants_new_frame_callback new_frame_needed_callback,
        camera_capture_image_done_callback new_frame_come_callback) {
        {
            const std::lock_guard<std::mutex> guard(collection_->reserve_lock_);
            if (collection_->current_reserved_[index_] != this) {
                LOG_ERROR(DRIVER_CAM, "Camera is not yet reserved to receive viewfinder feed!");
                return;
            }
        }

        if (!new_frame_come_callback || !new_frame_needed_callback) {
            LOG_ERROR(DRIVER_CAM, "One of the viewfinder receive callback are null. The operation is skipped!");
            return;
        }

        if ((size.x <= 0) || (size.y <= 0)) {
            LOG_ERROR(DRIVER_CAM, "Invalid viewfinder size {}x{}!", size.x, size.y);
            return;
        }

        if (!ios_is_supported_format(format)) {
            LOG_ERROR(DRIVER_CAM, "Viewfinder format {} is not supported!", static_cast<int>(format));
            return;
        }

        {
            const std::lock_guard<std::mutex> guard(callback_lock_);

            if (active_frame_viewfinder_callback_) {
                LOG_ERROR(DRIVER_CAM, "Another operation is active on this camera!");
                return;
            }

            active_frame_viewfinder_callback_ = new_frame_come_callback;
            wants_new_frame_callback_ = new_frame_needed_callback;
        }

        if (![device_of(device_holder_) startViewfinderWithWidth:size.x height:size.y format:format]) {
            LOG_ERROR(DRIVER_CAM, "Unable to start viewfinder feed!");

            camera_capture_image_done_callback failed_callback;

            {
                const std::lock_guard<std::mutex> guard(callback_lock_);
                failed_callback = active_frame_viewfinder_callback_;
                active_frame_viewfinder_callback_ = nullptr;
                wants_new_frame_callback_ = nullptr;
            }

            if (failed_callback) {
                failed_callback(nullptr, 0, -1);
            }
        }
    }

    void instance_ios::stop_viewfinder_feed() {
        {
            const std::lock_guard<std::mutex> guard(collection_->reserve_lock_);
            if (collection_->current_reserved_[index_] != this) {
                LOG_ERROR(DRIVER_CAM, "Camera is not yet reserved to stop viewfinder feed!");
                return;
            }
        }

        stop_viewfinder_feed_impl(true);
    }

    void instance_ios::stop_viewfinder_feed_impl(const bool log_if_inactive) {
        {
            const std::lock_guard<std::mutex> guard(callback_lock_);

            if (!active_frame_viewfinder_callback_ && !log_if_inactive) {
                // Nothing running; release() calls this unconditionally.
                return;
            }

            active_frame_viewfinder_callback_ = nullptr;
            wants_new_frame_callback_ = nullptr;
        }

        [device_of(device_holder_) stopViewfinder];
    }

    void instance_ios::capture_image(const std::uint32_t resolution_index, const frame_format format,
        camera_capture_image_done_callback callback) {
        if (!callback) {
            LOG_ERROR(DRIVER_CAM, "No capture image callback provided. Skipping image capture!");
            return;
        }

        {
            const std::lock_guard<std::mutex> guard(collection_->reserve_lock_);
            if (collection_->current_reserved_[index_] != this) {
                LOG_ERROR(DRIVER_CAM, "Camera is not yet reserved to capture image!");
                return;
            }
        }

        if (!ios_is_supported_format(format)) {
            LOG_ERROR(DRIVER_CAM, "Image capture format {} is not supported!", static_cast<int>(format));
            callback(nullptr, 0, -1);
            return;
        }

        {
            const std::lock_guard<std::mutex> guard(callback_lock_);

            if (active_capture_img_callback_) {
                LOG_ERROR(DRIVER_CAM, "Another operation is active on this camera!");
                return;
            }

            active_capture_img_callback_ = callback;
        }

        std::uint32_t flash_mode = FLASH_MODE_OFF;
        get_parameter(PARAMETER_KEY_FLASH, flash_mode);

        if (![device_of(device_holder_) captureWithResolutionIndex:static_cast<int>(resolution_index)
                                                            format:format
                                                   flashModeDriver:static_cast<int>(flash_mode)]) {
            LOG_ERROR(DRIVER_CAM, "Unable to request image capture!");

            camera_capture_image_done_callback failed_callback;

            {
                const std::lock_guard<std::mutex> guard(callback_lock_);
                failed_callback = active_capture_img_callback_;
                active_capture_img_callback_ = nullptr;
            }

            if (failed_callback) {
                failed_callback(nullptr, 0, -1);
            }
        }
    }

    // Copy-then-invoke: never call the guest completion while holding
    // callback_lock_ — it takes the kernel lock, and a kernel-locked guest
    // thread may be waiting on callback_lock_ in stop_viewfinder_feed().
    void instance_ios::deliver_viewfinder_frame(const void *bytes, const std::size_t size, const int error) {
        camera_capture_image_done_callback callback;

        {
            const std::lock_guard<std::mutex> guard(callback_lock_);
            callback = active_frame_viewfinder_callback_;
        }

        if (callback) {
            callback(bytes, size, error);
        }
    }

    void instance_ios::deliver_captured_image(const void *bytes, const std::size_t size, const int error) {
        camera_capture_image_done_callback callback;

        {
            const std::lock_guard<std::mutex> guard(callback_lock_);
            callback = active_capture_img_callback_;
            active_capture_img_callback_ = nullptr;
        }

        if (callback) {
            callback(bytes, size, error);
        }
    }

    bool instance_ios::wants_new_frame() {
        const std::lock_guard<std::mutex> guard(callback_lock_);

        if (wants_new_frame_callback_) {
            return wants_new_frame_callback_();
        }

        return false;
    }
}

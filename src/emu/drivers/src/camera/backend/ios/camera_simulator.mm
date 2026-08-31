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

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>

#include <drivers/camera/backend/ios/camera_pixel_ios.h>
#include <drivers/camera/backend/ios/camera_simulator.h>
#include <drivers/camera/camera_collection.h>

#include <common/log.h>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <vector>

// Device builds carry none of this: nothing references collection_simulator
// there (see get_collection()), so compile the whole backend out rather than
// ship a test fixture in the shipping binary.
#if TARGET_OS_SIMULATOR

namespace eka2l1::drivers::camera {
    // 0 = back, 1 = front, the index mapping the Android and iOS backends use.
    static constexpr std::uint32_t SIMULATED_CAMERA_COUNT = 2;
    static constexpr int VIEWFINDER_FPS = 15;

    // A plausible Symbian-era pair: 5MP main sensor, VGA front one.
    static const eka2l1::vec2 BACK_MAX_SIZE(2592, 1944);
    static const eka2l1::vec2 FRONT_MAX_SIZE(640, 480);

    static std::vector<eka2l1::vec2> image_size_ladder(const bool front_facing) {
        // Descending concrete sizes clamped to the sensor maximum, the shape
        // instance_ios builds from AVCaptureDevice's photo dimensions.
        static const eka2l1::vec2 CANDIDATES[] = {
            eka2l1::vec2(2592, 1944), eka2l1::vec2(2048, 1536), eka2l1::vec2(1600, 1200),
            eka2l1::vec2(1280, 960), eka2l1::vec2(1024, 768), eka2l1::vec2(640, 480),
            eka2l1::vec2(320, 240), eka2l1::vec2(160, 120)
        };

        const eka2l1::vec2 max_size = front_facing ? FRONT_MAX_SIZE : BACK_MAX_SIZE;
        std::vector<eka2l1::vec2> result;

        for (const eka2l1::vec2 &candidate: CANDIDATES) {
            if ((candidate.x <= max_size.x) && (candidate.y <= max_size.y)) {
                result.push_back(candidate);
            }
        }

        if (result.empty()) {
            result.push_back(max_size);
        }

        return result;
    }

    // Paint a frame that makes the usual pixel-path defects visible at a glance:
    // colour bars catch a wrong channel order, the corner marker catches a flip,
    // the grid catches a wrong scanline stride, the grey ramp catches truncated
    // colour depth, and the sweeping bar shows frames are actually advancing.
    static void synthesize_test_pattern_bgra(const int width, const int height,
        const std::uint32_t frame_index, const bool front_facing, std::vector<std::uint8_t> &out) {
        struct rgb {
            std::uint8_t r, g, b;
        };

        static const rgb BARS[] = {
            { 255, 255, 255 }, { 255, 255, 0 }, { 0, 255, 255 }, { 0, 255, 0 },
            { 255, 0, 255 }, { 255, 0, 0 }, { 0, 0, 255 }, { 0, 0, 0 }
        };
        static const int BAR_COUNT = static_cast<int>(sizeof(BARS) / sizeof(BARS[0]));

        out.resize(static_cast<std::size_t>(width) * 4 * height);

        const int marker_size = std::max(8, std::min(width, height) / 8);
        const int ramp_top = height - std::max(1, height / 8);
        const int sweep_x = static_cast<int>((frame_index * 4) % static_cast<std::uint32_t>(std::max(1, width)));

        for (int y = 0; y < height; y++) {
            std::uint8_t *row = out.data() + static_cast<std::size_t>(y) * width * 4;

            for (int x = 0; x < width; x++) {
                // The front camera mirrors the bar order, so picking the wrong
                // camera index is visible too.
                const int bar_x = front_facing ? (width - 1 - x) : x;
                rgb color = BARS[std::min(BAR_COUNT - 1, bar_x * BAR_COUNT / std::max(1, width))];

                if (y >= ramp_top) {
                    const std::uint8_t level = static_cast<std::uint8_t>(x * 255 / std::max(1, width - 1));
                    color = { level, level, level };
                }

                if (((x % 32) == 0) || ((y % 32) == 0)) {
                    color = { 64, 64, 64 };
                }

                // Origin marker in the top-left of an upright frame: red on the
                // back camera, blue on the front one.
                if ((x < marker_size) && (y < marker_size)) {
                    color = front_facing ? rgb{ 0, 0, 255 } : rgb{ 255, 0, 0 };
                }

                if ((x >= sweep_x) && (x < sweep_x + 4)) {
                    color = { 255, 128, 0 };
                }

                row[x * 4 + 0] = color.b;
                row[x * 4 + 1] = color.g;
                row[x * 4 + 2] = color.r;
                row[x * 4 + 3] = 0xFF;
            }
        }
    }

    static bool encode_pattern(const int width, const int height, const std::uint32_t frame_index,
        const bool front_facing, const frame_format format, std::vector<std::uint8_t> &out) {
        // The pattern stands in for a raw sensor readout, in the landscape shape
        // and with the same orientation a built-in camera hands over, and then
        // goes through the exact rotate-and-stretch the device backend applies.
        // Generating straight at the requested size skipped the orientation path
        // altogether, which is what left the simulator blind to frames arriving
        // sideways on hardware.
        const int sensor_width = std::max(width, height);
        const int sensor_height = std::min(width, height);
        const int rotation = ios_frame_rotation_ccw();

        std::vector<std::uint8_t> sensor_bgra;
        synthesize_test_pattern_bgra(sensor_width, sensor_height, frame_index, front_facing,
            sensor_bgra);

        std::vector<std::uint8_t> bgra;

        if ((rotation == 0) && (sensor_width == width) && (sensor_height == height)) {
            bgra = std::move(sensor_bgra);
        } else {
            CGImageRef sensor_image = ios_create_cgimage_from_bgra(sensor_bgra.data(),
                static_cast<std::size_t>(sensor_width) * 4, sensor_width, sensor_height);
            if (!sensor_image) {
                return false;
            }

            const bool rendered = ios_render_cgimage_to_bgra(sensor_image, width, height,
                rotation, bgra);
            CGImageRelease(sensor_image);

            if (!rendered) {
                return false;
            }
        }

        if ((format == FRAME_FORMAT_JPEG) || (format == FRAME_FORMAT_EXIF)) {
            return ios_encode_bgra_to_jpeg(bgra.data(), width, height, out);
        }

        return ios_convert_bgra_to_guest(bgra.data(), static_cast<std::size_t>(width) * 4,
            width, height, format, out);
    }

    class instance_simulator : public instance {
    private:
        collection_simulator *collection_;
        int index_;
        bool front_facing_;

        // Guards the callbacks and the viewfinder parameters. Callbacks are
        // copied out and invoked with the lock released: a guest completion
        // takes the kernel lock, and a guest thread holding the kernel lock may
        // concurrently call stop_viewfinder_feed(). Same ABBA shape instance_ios
        // avoids.
        std::mutex callback_lock_;
        camera_capture_image_done_callback frame_callback_;
        camera_wants_new_frame_callback wants_frame_callback_;

        eka2l1::vec2 viewfinder_size_;
        frame_format viewfinder_format_;
        std::uint32_t frame_index_;

        dispatch_queue_t queue_;
        dispatch_source_t timer_;

        std::uint32_t optical_zoom_;
        std::uint32_t digital_zoom_;
        std::uint32_t contrast_;
        std::uint32_t brightness_;
        std::uint32_t white_balance_;
        std::uint32_t exposure_;
        std::uint32_t flash_mode_;

        bool reserved_;

        void stop_timer() {
            if (timer_) {
                dispatch_source_cancel(timer_);
                timer_ = nullptr;
            }
        }

        void tick() {
            camera_capture_image_done_callback frame_callback;
            camera_wants_new_frame_callback wants_frame_callback;
            eka2l1::vec2 size;
            frame_format format;
            std::uint32_t index;

            {
                const std::lock_guard<std::mutex> guard(callback_lock_);
                if (!frame_callback_ || !wants_frame_callback_) {
                    return;
                }

                frame_callback = frame_callback_;
                wants_frame_callback = wants_frame_callback_;
                size = viewfinder_size_;
                format = viewfinder_format_;
                index = frame_index_++;
            }

            if (!wants_frame_callback()) {
                return;
            }

            std::vector<std::uint8_t> encoded;
            if (!encode_pattern(size.x, size.y, index, front_facing_, format, encoded)) {
                return;
            }

            frame_callback(encoded.data(), encoded.size(), 0);
        }

    public:
        explicit instance_simulator(collection_simulator *collection, const int index)
            : collection_(collection)
            , index_(index)
            , front_facing_(index == 1)
            , viewfinder_size_(0, 0)
            , viewfinder_format_(FRAME_FORMAT_ARGB8888)
            , frame_index_(0)
            , timer_(nullptr)
            , optical_zoom_(0)
            , digital_zoom_(1)
            , contrast_(0)
            , brightness_(0)
            , white_balance_(0)
            , exposure_(EXPOSURE_MODE_AUTO)
            , flash_mode_(FLASH_MODE_OFF)
            , reserved_(false) {
            queue_ = dispatch_queue_create("com.eka2l1.camera.simulator", DISPATCH_QUEUE_SERIAL);
        }

        ~instance_simulator() override {
            release();
        }

        bool set_parameter(const parameter_key key, const std::uint32_t value) override {
            switch (key) {
            case PARAMETER_KEY_OPTICAL_ZOOM:
                optical_zoom_ = value;
                break;

            case PARAMETER_KEY_DIGITAL_ZOOM:
                digital_zoom_ = value;
                break;

            case PARAMETER_KEY_CONTRAST:
                contrast_ = value;
                break;

            case PARAMETER_KEY_BRIGHTNESS:
                brightness_ = value;
                break;

            case PARAMETER_KEY_FLASH:
                flash_mode_ = value;
                break;

            case PARAMETER_KEY_EXPOSURE:
                exposure_ = value;
                break;

            case PARAMETER_KEY_WHITE_BALANCE:
                white_balance_ = value;
                break;

            default:
                LOG_WARN(DRIVER_CAM, "Unsupported parameter key {} to set value", static_cast<int>(key));
                return false;
            }

            return true;
        }

        bool get_parameter(const parameter_key key, std::uint32_t &value) override {
            switch (key) {
            case PARAMETER_KEY_OPTICAL_ZOOM:
                value = optical_zoom_;
                break;

            case PARAMETER_KEY_DIGITAL_ZOOM:
                value = digital_zoom_;
                break;

            case PARAMETER_KEY_CONTRAST:
                value = contrast_;
                break;

            case PARAMETER_KEY_BRIGHTNESS:
                value = brightness_;
                break;

            case PARAMETER_KEY_FLASH:
                value = flash_mode_;
                break;

            case PARAMETER_KEY_EXPOSURE:
                value = exposure_;
                break;

            case PARAMETER_KEY_WHITE_BALANCE:
                value = white_balance_;
                break;

            default:
                LOG_WARN(DRIVER_CAM, "Unsupported parameter key {} to get value", static_cast<int>(key));
                return false;
            }

            return true;
        }

        std::vector<frame_format> supported_frame_formats() override {
            return std::vector<frame_format>(std::begin(IOS_SUPPORTED_FORMATS),
                std::end(IOS_SUPPORTED_FORMATS));
        }

        std::vector<eka2l1::vec2> supported_output_image_sizes(const frame_format) override {
            return image_size_ladder(front_facing_);
        }

        bool reserve() override {
            if (reserved_) {
                LOG_ERROR(DRIVER_CAM, "Another camera instance is currently reserved the camera for operations!");
                return false;
            }

            reserved_ = true;
            return true;
        }

        void release() override {
            stop_viewfinder_feed();
            reserved_ = false;
        }

        info get_info() override {
            info result;
            result.camera_direction_ = front_facing_ ? DIRECTION_FRONT : DIRECTION_BACK;
            result.num_image_sizes_supported_ = static_cast<std::int32_t>(image_size_ladder(front_facing_).size());
            result.flash_modes_supported_ = FLASH_MODE_OFF | FLASH_MODE_AUTO | FLASH_MODE_FORCED | FLASH_MODE_VIDEO_LIGHT;
            result.options_supported_ = CAPTURE_OPTION_ALL;
            result.supported_image_formats_ = 0;

            for (const frame_format format: IOS_SUPPORTED_FORMATS) {
                result.supported_image_formats_ |= static_cast<std::uint32_t>(format);
            }

            return result;
        }

        void capture_image(const std::uint32_t resolution_index, const frame_format format,
            camera_capture_image_done_callback callback) override {
            if (!callback) {
                LOG_ERROR(DRIVER_CAM, "Capture done callback is null. The operation is skipped!");
                return;
            }

            if (!ios_is_supported_format(format)) {
                LOG_ERROR(DRIVER_CAM, "Capture format {} is not supported!", static_cast<int>(format));
                callback(nullptr, 0, -1);
                return;
            }

            const std::vector<eka2l1::vec2> sizes = image_size_ladder(front_facing_);
            if (resolution_index >= sizes.size()) {
                LOG_ERROR(DRIVER_CAM, "Capture resolution index {} out of range!", resolution_index);
                callback(nullptr, 0, -1);
                return;
            }

            const eka2l1::vec2 size = sizes[resolution_index];
            const bool front_facing = front_facing_;
            const std::uint32_t index = frame_index_;

            // Asynchronous like the real capture: the guest arms the request and
            // gets completed later, off the calling thread.
            dispatch_async(queue_, ^{
                std::vector<std::uint8_t> encoded;
                if (encode_pattern(size.x, size.y, index, front_facing, format, encoded)) {
                    callback(encoded.data(), encoded.size(), 0);
                } else {
                    callback(nullptr, 0, -1);
                }
            });
        }

        void receive_viewfinder_feed(const eka2l1::vec2 &size, const frame_format format,
            camera_wants_new_frame_callback new_frame_needed_callback,
            camera_capture_image_done_callback new_frame_come_callback) override {
            if (!reserved_) {
                LOG_ERROR(DRIVER_CAM, "Camera is not yet reserved to receive viewfinder feed!");
                return;
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
                viewfinder_size_ = size;
                viewfinder_format_ = format;
                frame_callback_ = new_frame_come_callback;
                wants_frame_callback_ = new_frame_needed_callback;
            }

            if (timer_) {
                return;
            }

            timer_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue_);
            if (!timer_) {
                LOG_ERROR(DRIVER_CAM, "Unable to create the simulated viewfinder timer!");
                return;
            }

            const std::uint64_t interval = NSEC_PER_SEC / VIEWFINDER_FPS;
            dispatch_source_set_timer(timer_, dispatch_time(DISPATCH_TIME_NOW, 0), interval,
                interval / 10);
            dispatch_source_set_event_handler(timer_, ^{
                this->tick();
            });
            dispatch_resume(timer_);
        }

        void stop_viewfinder_feed() override {
            {
                const std::lock_guard<std::mutex> guard(callback_lock_);
                frame_callback_ = nullptr;
                wants_frame_callback_ = nullptr;
            }

            // The handler block captures `this`, so the source must be gone
            // before this instance is. Cancelling is synchronous enough here:
            // an in-flight tick() has already copied the (now cleared)
            // callbacks and delivers nothing.
            stop_timer();
        }
    };

    std::uint32_t collection_simulator::count() const {
        return SIMULATED_CAMERA_COUNT;
    }

    std::unique_ptr<instance> collection_simulator::make_camera(const std::uint32_t camera_index) {
        if (camera_index >= SIMULATED_CAMERA_COUNT) {
            LOG_ERROR(DRIVER_CAM, "Simulated camera index {} out of range!", camera_index);
            return nullptr;
        }

        return std::make_unique<instance_simulator>(this, static_cast<int>(camera_index));
    }
}

#endif

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

#include <drivers/camera/backend/qt/camera_qt.h>
#include <drivers/camera/camera_pixel.h>

#include <common/log.h>

#include <QBuffer>
#include <QByteArray>
#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QCoreApplication>
#include <QImage>
#include <QImageCapture>
#include <QList>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMetaObject>
#include <QSize>
#include <QThread>
#include <QTimer>
#include <QTransform>
#include <QVideoFrame>
#include <QVideoSink>

#include <algorithm>
#include <mutex>
#include <utility>

// convert_bgra_to_guest() reads B,G,R,A from consecutive bytes, which is what
// QImage::Format_ARGB32 (a quint32 0xAARRGGBB) lays out on a little-endian host
// only.
static_assert(Q_BYTE_ORDER == Q_LITTLE_ENDIAN,
    "The Qt camera backend assumes QImage::Format_ARGB32 is B,G,R,A in memory");

namespace eka2l1::drivers::camera {
    namespace {
        // A still capture that never reports back would leave the guest's
        // request outstanding forever, so give the device a deadline to become
        // ready.
        constexpr int CAPTURE_READY_TIMEOUT_MS = 5000;

        constexpr int JPEG_QUALITY = 80;

        // Releasing and re-reserving the camera in quick succession makes Qt
        // reopen the device, which fails while the old handle is still going
        // down. Let a released camera settle first, and check again on the way
        // out: a guest that comes straight back never loses the device.
        constexpr int CAMERA_IDLE_STOP_DELAY_MS = 750;

        // One event loop, shared by every camera, owns all the Qt Multimedia
        // objects: QCamera and friends deliver through queued signals, so they
        // need a thread that runs one. It must not be the GUI thread -- a frame
        // handler completes the guest request under the emulator kernel lock,
        // and a guest thread can hold that lock while waiting on the GUI thread.
        //
        // Nothing on the guest path ever joins this thread. Teardown posts work
        // and returns, because the guest thread doing the teardown may be
        // holding the very kernel lock an in-flight delivery is waiting for.
        QThread *shared_camera_thread() {
            static QThread *thread = []() -> QThread * {
                QThread *created = new QThread();
                created->setObjectName(QStringLiteral("EKA2L1 camera"));
                created->start();

                if (QCoreApplication *app = QCoreApplication::instance()) {
                    // Direct connection: QThread::quit() is thread-safe, while
                    // the QThread object itself lives on whichever guest thread
                    // first opened a camera and has no event loop of its own to
                    // deliver a queued call to.
                    QObject::connect(app, &QCoreApplication::aboutToQuit, app, [created]() {
                        created->quit();
                        created->wait(1000);
                    }, Qt::DirectConnection);
                }

                return created;
            }();

            return thread;
        }

        qint64 pixel_count(const QSize &size) {
            return static_cast<qint64>(size.width()) * size.height();
        }

        // "No camera" is the hardest thing to tell apart from a broken Qt
        // Multimedia backend, so say once what the host actually offers.
        void log_video_inputs_once(const QList<QCameraDevice> &devices) {
            static std::once_flag logged;

            std::call_once(logged, [&devices]() {
                if (devices.isEmpty()) {
                    LOG_INFO(DRIVER_CAM, "Qt Multimedia reports no video input on this host");
                    return;
                }

                for (qsizetype i = 0; i < devices.size(); i++) {
                    LOG_INFO(DRIVER_CAM, "Camera {}: {}", i,
                        devices[i].description().toStdString());
                }
            });
        }

        // Index 0 is the device the host itself defaults to. A desktop has no
        // fixed back/front pair like a phone does, and a guest that only ever
        // opens camera 0 should get the input the user actually has.
        QList<QCameraDevice> ordered_video_inputs() {
            if (!QCoreApplication::instance()) {
                return {};
            }

            const QList<QCameraDevice> inputs = QMediaDevices::videoInputs();
            const QCameraDevice preferred = QMediaDevices::defaultVideoInput();

            QList<QCameraDevice> ordered;
            if (!preferred.isNull() && inputs.contains(preferred)) {
                ordered.append(preferred);
            }

            for (const QCameraDevice &device: inputs) {
                if (!ordered.contains(device)) {
                    ordered.append(device);
                }
            }

            log_video_inputs_once(ordered);

            return ordered;
        }

        // Advertise a ladder of Symbian-era capture sizes clamped to what the
        // device can actually produce, largest first -- the same shape the
        // Android and iOS backends expose.
        std::vector<eka2l1::vec2> build_image_size_ladder(const QCameraDevice &device) {
            QSize max_size(0, 0);

            const auto consider = [&max_size](const QSize &candidate) {
                if (pixel_count(candidate) > pixel_count(max_size)) {
                    max_size = candidate;
                }
            };

            for (const QSize &size: device.photoResolutions()) {
                consider(size);
            }

            for (const QCameraFormat &format: device.videoFormats()) {
                consider(format.resolution());
            }

            if (max_size.isEmpty()) {
                max_size = QSize(640, 480);
            }

            static const eka2l1::vec2 CANDIDATES[] = {
                eka2l1::vec2(2592, 1944), eka2l1::vec2(2048, 1536), eka2l1::vec2(1600, 1200),
                eka2l1::vec2(1280, 960), eka2l1::vec2(1024, 768), eka2l1::vec2(640, 480),
                eka2l1::vec2(320, 240), eka2l1::vec2(160, 120)
            };

            std::vector<eka2l1::vec2> sizes;
            sizes.push_back(eka2l1::vec2(max_size.width(), max_size.height()));

            for (const eka2l1::vec2 &candidate: CANDIDATES) {
                if ((candidate.x <= max_size.width()) && (candidate.y <= max_size.height()) &&
                    !((candidate.x == max_size.width()) && (candidate.y == max_size.height()))) {
                    sizes.push_back(candidate);
                }
            }

            return sizes;
        }

        bool encode_jpeg(const QImage &image, std::vector<std::uint8_t> &out) {
            QByteArray encoded;
            QBuffer buffer(&encoded);

            if (!buffer.open(QIODevice::WriteOnly)) {
                return false;
            }

            const bool saved = image.save(&buffer, "JPEG", JPEG_QUALITY);
            buffer.close();

            if (!saved) {
                return false;
            }

            const std::uint8_t *base = reinterpret_cast<const std::uint8_t *>(encoded.constData());
            out.assign(base, base + encoded.size());

            return true;
        }

        // Rotate a frame into the guest's picture, then fill the requested
        // extent edge to edge. Stretching rather than letterboxing is
        // deliberate: a guest scales the frame over its own window and the two
        // stretches largely cancel, the way they do on hardware whose sensor
        // buffer has a fixed shape whatever the screen is.
        bool image_to_guest(QImage image, const int dest_width, const int dest_height,
            const int rotation_ccw_deg, const frame_format format, std::vector<std::uint8_t> &out) {
            if (image.isNull() || (dest_width <= 0) || (dest_height <= 0)) {
                return false;
            }

            const int rotation = ((rotation_ccw_deg % 360) + 360) % 360;

            if (rotation != 0) {
                // A QTransform angle turns the picture clockwise once the y-down
                // image space is accounted for, so counter-clockwise is negative.
                image = image.transformed(QTransform().rotate(-rotation), Qt::FastTransformation);
            }

            if ((image.width() != dest_width) || (image.height() != dest_height)) {
                image = image.scaled(dest_width, dest_height, Qt::IgnoreAspectRatio,
                    Qt::SmoothTransformation);
            }

            if (image.isNull()) {
                return false;
            }

            if ((format == FRAME_FORMAT_JPEG) || (format == FRAME_FORMAT_EXIF)) {
                return encode_jpeg(image, out);
            }

            if (image.format() != QImage::Format_ARGB32) {
                image = image.convertToFormat(QImage::Format_ARGB32);

                if (image.isNull()) {
                    return false;
                }
            }

            return convert_bgra_to_guest(image.constBits(),
                static_cast<std::size_t>(image.bytesPerLine()), dest_width, dest_height, format, out);
        }
    }

    // No Q_OBJECT macro: every connection made here binds a lambda to this
    // object as context, so nothing in the class needs moc.
    struct session_qt : public QObject {
        QCameraDevice device_;
        std::vector<eka2l1::vec2> image_sizes_;

        // Created on, and only touched by, the camera thread.
        QMediaCaptureSession *capture_session_ = nullptr;
        QCamera *camera_ = nullptr;
        QVideoSink *sink_ = nullptr;
        QImageCapture *image_capture_ = nullptr;

        // Viewfinder state is read by handle_frame(), which runs on whichever
        // thread Qt produced the frame on -- see the connection below.
        std::mutex viewfinder_lock_;
        bool viewfinder_active_ = false;
        eka2l1::vec2 viewfinder_size_;
        frame_format viewfinder_format_ = FRAME_FORMAT_FBSBMP_COLOR64K;

        bool capture_active_ = false;
        bool capture_waiting_ready_ = false;
        eka2l1::vec2 capture_size_;
        frame_format capture_format_ = FRAME_FORMAT_JPEG;

        // Guest-armed callbacks, the one piece of state both threads touch.
        // Copy under the lock and invoke outside it: a completion takes the
        // emulator kernel lock, and a guest thread that holds that lock may be
        // clearing these at the same moment.
        std::mutex callback_lock_;
        camera_capture_image_done_callback viewfinder_callback_;
        camera_capture_image_done_callback capture_callback_;
        camera_wants_new_frame_callback wants_new_frame_callback_;

        explicit session_qt(const QCameraDevice &device)
            : device_(device)
            , image_sizes_(build_image_size_ladder(device)) {
        }

        ~session_qt() override {
            // Members are torn down after this body runs, and the Qt children
            // only after that, so stop the camera here: a viewfinder frame is
            // handled straight from the thread Qt produced it on, and stopping
            // is what waits for one already in flight. Without it a late frame
            // could reach a half-destroyed session.
            if (camera_) {
                camera_->stop();
            }

            disconnect();
        }

        bool facing_front() const {
            return device_.position() == QCameraDevice::FrontFace;
        }

        // MARK: guest thread

        bool arm_viewfinder(const eka2l1::vec2 &size, const frame_format format,
            camera_wants_new_frame_callback wants, camera_capture_image_done_callback done) {
            {
                const std::lock_guard<std::mutex> guard(callback_lock_);

                if (viewfinder_callback_) {
                    return false;
                }

                viewfinder_callback_ = std::move(done);
                wants_new_frame_callback_ = std::move(wants);
            }

            QMetaObject::invokeMethod(this, [this, size, format]() {
                start_viewfinder(size, format);
            }, Qt::QueuedConnection);

            return true;
        }

        void disarm_viewfinder() {
            {
                const std::lock_guard<std::mutex> guard(callback_lock_);
                viewfinder_callback_ = nullptr;
                wants_new_frame_callback_ = nullptr;
            }

            const std::lock_guard<std::mutex> guard(viewfinder_lock_);
            viewfinder_active_ = false;
        }

        // The guest is done with the camera, not just with this viewfinder run.
        // Only here is the capture device actually let go: a guest stops and
        // restarts the viewfinder around a still, and every stop/start of the
        // device is a chance for Qt to fail the reopen.
        void release_camera() {
            QMetaObject::invokeMethod(this, [this]() {
                stop_camera_if_idle();
            }, Qt::QueuedConnection);
        }

        bool arm_capture(const eka2l1::vec2 &size, const frame_format format,
            camera_capture_image_done_callback done) {
            {
                const std::lock_guard<std::mutex> guard(callback_lock_);

                if (capture_callback_) {
                    return false;
                }

                capture_callback_ = std::move(done);
            }

            QMetaObject::invokeMethod(this, [this, size, format]() {
                request_capture(size, format);
            }, Qt::QueuedConnection);

            return true;
        }

        void clear_callbacks() {
            const std::lock_guard<std::mutex> guard(callback_lock_);
            viewfinder_callback_ = nullptr;
            wants_new_frame_callback_ = nullptr;
            capture_callback_ = nullptr;
        }

        // MARK: camera thread

        void build_qt_objects() {
            capture_session_ = new QMediaCaptureSession(this);
            camera_ = new QCamera(device_, this);
            sink_ = new QVideoSink(this);
            image_capture_ = new QImageCapture(this);

            capture_session_->setCamera(camera_);
            capture_session_->setVideoSink(sink_);
            capture_session_->setImageCapture(image_capture_);

            // Direct: convert on the thread that produced the frame. A queued
            // connection would let frames pile up behind a conversion slower
            // than the capture rate, and the guest wants the newest frame, not
            // a backlog.
            connect(sink_, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
                handle_frame(frame);
            }, Qt::DirectConnection);

            connect(camera_, &QCamera::errorOccurred, this,
                [this](QCamera::Error error, const QString &message) {
                if (error == QCamera::NoError) {
                    return;
                }

                LOG_ERROR(DRIVER_CAM, "Camera error: {}", message.toStdString());
                fail_active_requests();
            });

            connect(image_capture_, &QImageCapture::imageCaptured, this,
                [this](int, const QImage &preview) {
                handle_captured_image(preview);
            });

            connect(image_capture_, &QImageCapture::errorOccurred, this,
                [this](int, QImageCapture::Error, const QString &message) {
                LOG_ERROR(DRIVER_CAM, "Image capture failed: {}", message.toStdString());
                finish_capture(nullptr, 0, -1);
            });

            connect(image_capture_, &QImageCapture::readyForCaptureChanged, this,
                [this](bool ready) {
                if (ready && capture_waiting_ready_) {
                    capture_waiting_ready_ = false;
                    issue_capture();
                }
            });
        }

        void start_viewfinder(const eka2l1::vec2 &size, const frame_format format) {
            {
                const std::lock_guard<std::mutex> guard(viewfinder_lock_);
                viewfinder_size_ = size;
                viewfinder_format_ = format;
                viewfinder_active_ = true;
            }

            if (!camera_->isActive()) {
                apply_format_covering(size);
                camera_->start();
            }
        }

        void request_capture(const eka2l1::vec2 &size, const frame_format format) {
            capture_size_ = size;
            capture_format_ = format;
            capture_active_ = true;

            if (!camera_->isActive()) {
                apply_format_covering(size);
                camera_->start();
            }

            if (image_capture_->isReadyForCapture()) {
                issue_capture();
                return;
            }

            capture_waiting_ready_ = true;

            QTimer::singleShot(CAPTURE_READY_TIMEOUT_MS, this, [this]() {
                if (capture_waiting_ready_) {
                    capture_waiting_ready_ = false;
                    LOG_ERROR(DRIVER_CAM, "Camera never became ready to capture an image!");
                    finish_capture(nullptr, 0, -1);
                }
            });
        }

        void issue_capture() {
            if (image_capture_->capture() < 0) {
                LOG_ERROR(DRIVER_CAM, "Unable to request image capture!");
                finish_capture(nullptr, 0, -1);
            }
        }

        // Pick the smallest camera format that still covers what the guest
        // asked for, so the conversion below scales down rather than up.
        void apply_format_covering(const eka2l1::vec2 &size) {
            // Qt reopens the capture device to change format, and the reopen
            // fails ("Camera is in use") while the camera is running. A frame
            // is rescaled to the guest's size anyway, so a running camera keeps
            // whatever format it started with.
            if (camera_->isActive()) {
                return;
            }

            const QList<QCameraFormat> formats = device_.videoFormats();
            if (formats.isEmpty()) {
                return;
            }

            // Rotating a frame into the guest's picture can swap the requested
            // edges, so match the long edge against the long one.
            const int long_edge = std::max(size.x, size.y);
            const int short_edge = std::min(size.x, size.y);

            QCameraFormat chosen;
            QCameraFormat largest;

            for (const QCameraFormat &format: formats) {
                const QSize resolution = format.resolution();

                if (largest.isNull() || (pixel_count(resolution) > pixel_count(largest.resolution()))) {
                    largest = format;
                }

                if ((std::max(resolution.width(), resolution.height()) < long_edge) ||
                    (std::min(resolution.width(), resolution.height()) < short_edge)) {
                    continue;
                }

                if (chosen.isNull() || (pixel_count(resolution) < pixel_count(chosen.resolution()))) {
                    chosen = format;
                }
            }

            if (chosen.isNull()) {
                chosen = largest;
            }

            if (!chosen.isNull() && (chosen != camera_->cameraFormat())) {
                camera_->setCameraFormat(chosen);
            }
        }

        bool viewfinder_running() {
            const std::lock_guard<std::mutex> guard(viewfinder_lock_);
            return viewfinder_active_;
        }

        void stop_camera_if_idle() {
            QTimer::singleShot(CAMERA_IDLE_STOP_DELAY_MS, this, [this]() {
                if (!viewfinder_running() && !capture_active_ && camera_ && camera_->isActive()) {
                    camera_->stop();
                }
            });
        }

        void handle_frame(const QVideoFrame &frame) {
            eka2l1::vec2 size;
            frame_format format;

            {
                const std::lock_guard<std::mutex> guard(viewfinder_lock_);

                if (!viewfinder_active_) {
                    return;
                }

                size = viewfinder_size_;
                format = viewfinder_format_;
            }

            if (!frame.isValid() || !wants_new_frame()) {
                return;
            }

            std::vector<std::uint8_t> converted;
            if (!image_to_guest(frame.toImage(), size.x, size.y, frame_rotation(), format, converted)) {
                return;
            }

            deliver_viewfinder_frame(converted.data(), converted.size(), 0);
        }

        void handle_captured_image(const QImage &image) {
            if (!capture_active_) {
                return;
            }

            std::vector<std::uint8_t> converted;
            if (image_to_guest(image, capture_size_.x, capture_size_.y, frame_rotation(),
                capture_format_, converted)) {
                finish_capture(converted.data(), converted.size(), 0);
            } else {
                LOG_ERROR(DRIVER_CAM, "Unable to convert the captured image for the guest!");
                finish_capture(nullptr, 0, -1);
            }
        }

        void finish_capture(const void *bytes, const std::size_t size, const int error) {
            capture_active_ = false;
            capture_waiting_ready_ = false;

            deliver_captured_image(bytes, size, error);
        }

        void fail_active_requests() {
            if (capture_active_) {
                finish_capture(nullptr, 0, -1);
            }

            if (viewfinder_running()) {
                deliver_viewfinder_frame(nullptr, 0, -1);
            }
        }

        // MARK: callback plumbing, either thread

        bool wants_new_frame() {
            camera_wants_new_frame_callback callback;

            {
                const std::lock_guard<std::mutex> guard(callback_lock_);
                callback = wants_new_frame_callback_;
            }

            return callback ? callback() : false;
        }

        void deliver_viewfinder_frame(const void *bytes, const std::size_t size, const int error) {
            camera_capture_image_done_callback callback;

            {
                const std::lock_guard<std::mutex> guard(callback_lock_);
                callback = viewfinder_callback_;
            }

            if (callback) {
                callback(bytes, size, error);
            }
        }

        void deliver_captured_image(const void *bytes, const std::size_t size, const int error) {
            camera_capture_image_done_callback callback;

            {
                const std::lock_guard<std::mutex> guard(callback_lock_);
                callback = capture_callback_;
                capture_callback_ = nullptr;
            }

            if (callback) {
                callback(bytes, size, error);
            }
        }
    };

    session_qt *make_session_qt(const std::uint32_t camera_index) {
        if (!QCoreApplication::instance()) {
            LOG_ERROR(DRIVER_CAM, "Qt Multimedia needs a running application to open a camera!");
            return nullptr;
        }

        const QList<QCameraDevice> devices = ordered_video_inputs();
        if (camera_index >= static_cast<std::uint32_t>(devices.size())) {
            LOG_ERROR(DRIVER_CAM, "No host video input with index {}", camera_index);
            return nullptr;
        }

        session_qt *session = new session_qt(devices[static_cast<qsizetype>(camera_index)]);
        session->moveToThread(shared_camera_thread());

        QMetaObject::invokeMethod(session, [session]() {
            session->build_qt_objects();
        }, Qt::QueuedConnection);

        return session;
    }

    void destroy_session_qt(session_qt *session) {
        if (!session) {
            return;
        }

        // Clear first: a delivery that is already on its way then finds nothing
        // to complete instead of reaching a guest object being torn down.
        session->clear_callbacks();

        // Never joined from here. deleteLater runs on the camera thread, and
        // destroying the QCamera there stops it; blocking on that from a guest
        // thread would hold the kernel lock an in-flight delivery may want.
        session->deleteLater();
    }

    std::uint32_t collection_qt::count() const {
        return static_cast<std::uint32_t>(ordered_video_inputs().size());
    }

    std::unique_ptr<instance> collection_qt::make_camera(const std::uint32_t camera_index) {
        session_qt *session = make_session_qt(camera_index);
        if (!session) {
            return nullptr;
        }

        return std::make_unique<instance_qt>(this, static_cast<int>(camera_index), session);
    }

    instance_qt::instance_qt(collection_qt *collection, const int index, session_qt *session)
        : collection_(collection)
        , index_(index)
        , session_(session)
        , stub_optical_zoom_(0)
        , stub_exposure_(EXPOSURE_MODE_AUTO)
        , stub_digital_zoom_(1)
        , stub_contrast_(0)
        , stub_brightness_(0)
        , stub_white_balance_(0)
        , flash_mode_(FLASH_MODE_OFF) {
    }

    instance_qt::~instance_qt() {
        release();
    }

    bool instance_qt::set_parameter(const parameter_key key, const std::uint32_t value) {
        // A host webcam exposes none of these knobs through Qt Multimedia, but a
        // guest that is told a parameter is supported and then gets a failure
        // back tends to leave, so remember what it set and hand the same value
        // back -- the same contract the iOS backend keeps.
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

    bool instance_qt::get_parameter(const parameter_key key, std::uint32_t &value) {
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

    std::vector<frame_format> instance_qt::supported_frame_formats() {
        return std::vector<frame_format>(std::begin(SUPPORTED_FRAME_FORMATS),
            std::end(SUPPORTED_FRAME_FORMATS));
    }

    std::vector<eka2l1::vec2> instance_qt::supported_output_image_sizes(const frame_format frame_format) {
        return session_ ? session_->image_sizes_ : std::vector<eka2l1::vec2>();
    }

    bool instance_qt::reserve() {
        const std::lock_guard<std::mutex> guard(collection_->reserve_lock_);

        if (collection_->current_reserved_[index_] != nullptr) {
            LOG_ERROR(DRIVER_CAM, "Another camera instance is currently reserved the camera for operations!");
            return false;
        }

        collection_->current_reserved_[index_] = this;
        return true;
    }

    void instance_qt::release() {
        if (session_) {
            session_->disarm_viewfinder();
            session_->release_camera();
        }

        const std::lock_guard<std::mutex> guard(collection_->reserve_lock_);

        if (collection_->current_reserved_[index_] == this) {
            collection_->current_reserved_[index_] = nullptr;
        }
    }

    info instance_qt::get_info() {
        info result{};
        result.camera_direction_ = (session_ && session_->facing_front()) ? DIRECTION_FRONT : DIRECTION_BACK;
        result.num_image_sizes_supported_ = session_
            ? static_cast<std::int32_t>(session_->image_sizes_.size()) : 0;
        result.flash_modes_supported_ = FLASH_MODE_OFF | FLASH_MODE_AUTO | FLASH_MODE_FORCED | FLASH_MODE_VIDEO_LIGHT;
        result.options_supported_ = CAPTURE_OPTION_ALL;
        result.supported_image_formats_ = 0;

        for (const frame_format format: SUPPORTED_FRAME_FORMATS) {
            result.supported_image_formats_ |= static_cast<std::uint32_t>(format);
        }

        return result;
    }

    void instance_qt::receive_viewfinder_feed(const eka2l1::vec2 &size, const frame_format format,
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

        if (!is_supported_frame_format(format)) {
            LOG_ERROR(DRIVER_CAM, "Viewfinder format {} is not supported!", static_cast<int>(format));
            return;
        }

        if (!session_ || !session_->arm_viewfinder(size, format, new_frame_needed_callback,
            new_frame_come_callback)) {
            LOG_ERROR(DRIVER_CAM, "Another operation is active on this camera!");
        }
    }

    void instance_qt::stop_viewfinder_feed() {
        {
            const std::lock_guard<std::mutex> guard(collection_->reserve_lock_);
            if (collection_->current_reserved_[index_] != this) {
                LOG_ERROR(DRIVER_CAM, "Camera is not yet reserved to stop viewfinder feed!");
                return;
            }
        }

        if (session_) {
            session_->disarm_viewfinder();
        }
    }

    void instance_qt::capture_image(const std::uint32_t resolution_index, const frame_format format,
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

        if (!is_supported_frame_format(format)) {
            LOG_ERROR(DRIVER_CAM, "Image capture format {} is not supported!", static_cast<int>(format));
            callback(nullptr, 0, -1);
            return;
        }

        if (!session_ || (resolution_index >= session_->image_sizes_.size())) {
            LOG_ERROR(DRIVER_CAM, "Capture resolution index {} out of range!", resolution_index);
            callback(nullptr, 0, -1);
            return;
        }

        if (!session_->arm_capture(session_->image_sizes_[resolution_index], format, callback)) {
            LOG_ERROR(DRIVER_CAM, "Another operation is active on this camera!");
        }
    }
}

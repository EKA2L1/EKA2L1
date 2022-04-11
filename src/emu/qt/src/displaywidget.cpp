/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <QWindow>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMessageBox>
#include <QGuiApplication>

#include <common/algorithm.h>
#include <common/log.h>
#include <qt/displaywidget.h>

#if !(defined(WIN32) || defined(__APPLE__) || defined(__HAIKU__))
#include <qpa/qplatformnativeinterface.h>
#endif

static eka2l1::drivers::window_system_type get_window_system_type() {
    // Determine WSI type based on Qt platform.
    QString platform_name = QGuiApplication::platformName();
    if (platform_name == QStringLiteral("windows"))
        return eka2l1::drivers::window_system_type::windows;
    else if (platform_name == QStringLiteral("cocoa"))
        return eka2l1::drivers::window_system_type::macOS;
    else if (platform_name == QStringLiteral("xcb"))
        return eka2l1::drivers::window_system_type::x11;
    else if (platform_name == QStringLiteral("wayland"))
        return eka2l1::drivers::window_system_type::wayland;
    else if (platform_name == QStringLiteral("haiku"))
        return eka2l1::drivers::window_system_type::haiku;

    QMessageBox::critical( nullptr, QStringLiteral("Error"), QString::asprintf("Unknown Qt platform: %s", platform_name.toStdString().c_str()));
    return eka2l1::drivers::window_system_type::headless;
}

display_widget::display_widget(QWidget *parent)
    : QWidget(parent)
    , userdata_(nullptr)
{
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_AcceptTouchEvents);

    setMouseTracking(false);

    windowHandle()->setSurfaceType(QWindow::OpenGLSurface);
    windowHandle()->create();

    reset_active_pointers();
}

display_widget::~display_widget() {
}

eka2l1::drivers::window_system_info display_widget::get_window_system_info() {
    eka2l1::drivers::window_system_info wsi;
    wsi.type = get_window_system_type();

    QWindow *window = windowHandle();

    // Our Win32 Qt external doesn't have the private API.
    #if defined(WIN32) || defined(__APPLE__) || defined(__HAIKU__)
    wsi.render_window = window ? reinterpret_cast<void*>(window->winId()) : nullptr;
    wsi.render_surface = wsi.render_window;
    #else
    QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
    wsi.display_connection = pni->nativeResourceForWindow("display", window);
    if (wsi.type == eka2l1::drivers::window_system_type::wayland)
        wsi.render_window = window ? pni->nativeResourceForWindow("surface", window) : nullptr;
    else
        wsi.render_window = window ? reinterpret_cast<void*>(window->winId()) : nullptr;

    wsi.render_surface = wsi.render_window;
    #endif
    wsi.render_surface_scale = window ? static_cast<float>(window->devicePixelRatio()) : 1.0f;

    return wsi;
}

void display_widget::init(std::string title, eka2l1::vec2 size, const std::uint32_t flags) {
    resize(size.x, size.y);
    change_title(title);
}

void display_widget::poll_events() {
    // Not to our business
}

void display_widget::shutdown() {
    // todo
}

void display_widget::set_fullscreen(const bool is_fullscreen) {
    if (is_fullscreen) {
        showFullScreen();
    } else {
        showNormal();
    }
}

bool display_widget::should_quit() {
    return false;
}

void display_widget::change_title(std::string title) {
    setWindowTitle(QString::fromUtf8(title.c_str()));
}

eka2l1::vec2 display_widget::window_size() {
    return eka2l1::vec2(width(), height());
}

eka2l1::vec2 display_widget::window_fb_size() {
    const qreal pixel_ratio = devicePixelRatioF();
    return eka2l1::vec2(width() * pixel_ratio, height() * pixel_ratio);
}

eka2l1::vec2d display_widget::get_mouse_pos() {
    return eka2l1::vec2d{ 0, 0 };
}

bool display_widget::get_mouse_button_hold(const int mouse_btt) {
    return false;
}

void display_widget::set_userdata(void *userdata) {
    userdata_ = userdata;
}

void *display_widget::get_userdata() {
    return userdata_;
}

bool display_widget::set_cursor(eka2l1::drivers::cursor *cur) {
    return true;
}

void display_widget::cursor_visiblity(const bool visi) {
}

bool display_widget::cursor_visiblity() {
    return true;
}

void display_widget::keyPressEvent(QKeyEvent *event) {
    if (event->isAutoRepeat()) {
        return;
    }

    if (button_pressed) {
        button_pressed(userdata_, event->key());
    }
}

void display_widget::keyReleaseEvent(QKeyEvent *event) {
    if (event->isAutoRepeat()) {
        return;
    }

    if (button_released) {
        button_released(userdata_, event->key());
    }
}

void display_widget::mousePressEvent(QMouseEvent *event) {
    const qreal pixel_ratio = devicePixelRatioF();

    if (raw_mouse_event) {
        const int button = eka2l1::common::find_most_significant_bit_one(event->button());
        raw_mouse_event(userdata_, eka2l1::vec3(event->pos().x() * pixel_ratio, event->pos().y() * pixel_ratio, 0), button, 0, 0);
    }
}

void display_widget::mouseReleaseEvent(QMouseEvent *event) {
    if (raw_mouse_event) {
        const qreal pixel_ratio = devicePixelRatioF();

        const int button = eka2l1::common::find_most_significant_bit_one(event->button());
        raw_mouse_event(userdata_, eka2l1::vec3(event->pos().x() * pixel_ratio, event->pos().y() * pixel_ratio, 0), button, 2, 0);
    }
}

void display_widget::mouseMoveEvent(QMouseEvent *event) {
    // Even with mouse tracking disabled, they still keep coming
    if (event->buttons() == Qt::NoButton) {
        return;
    }

    const qreal pixel_ratio = devicePixelRatioF();

    if (raw_mouse_event) {
        const int button = eka2l1::common::find_most_significant_bit_one(event->buttons());
        raw_mouse_event(userdata_, eka2l1::vec3(event->pos().x() * pixel_ratio, event->pos().y() * pixel_ratio, 0), button, 1, 0);
    }
}

void display_widget::reset_active_pointers() {
    std::fill(active_pointers_.begin(), active_pointers_.end(), 0);
}

bool display_widget::event(QEvent *event) {
    switch (event->type()) {
    case QEvent::TouchBegin: {
        QTouchEvent *touch_event = reinterpret_cast<QTouchEvent*>(event);
        const QList<QTouchEvent::TouchPoint> &points = touch_event->touchPoints();

        const qreal pixel_ratio = devicePixelRatioF();

        for (std::size_t i = 0; i < active_pointers_.size(); i++) {
            if (active_pointers_[i] == 0) {
                active_pointers_[i] = points[i].id() + 1;
                raw_mouse_event(userdata_, eka2l1::vec3(static_cast<int>(points[i].pos().x() * pixel_ratio),
                    static_cast<int>(points[i].pos().y() * pixel_ratio),
                    static_cast<int>(points[i].pressure() * eka2l1::PRESSURE_MAX_NUM)),
                    0, 0, static_cast<int>(i));
            }
        }

        return true;
    }

    case QEvent::TouchUpdate: {
        QTouchEvent *touch_event = reinterpret_cast<QTouchEvent*>(event);
        const QList<QTouchEvent::TouchPoint> &points = touch_event->touchPoints();

        const qreal pixel_ratio = devicePixelRatioF();

        for (std::size_t i = 0; i < points.size(); i++) {
            bool found = false;
            for (std::size_t j = 0; j < active_pointers_.size(); j++) {
                if ((points[i].id() + 1) == active_pointers_[j]) {
                    raw_mouse_event(userdata_, eka2l1::vec3(static_cast<int>(points[i].pos().x() * pixel_ratio),
                        static_cast<int>(points[i].pos().y() * pixel_ratio),
                        static_cast<int>(points[i].pressure() * eka2l1::PRESSURE_MAX_NUM)),
                        0, 1, static_cast<int>(j));

                    found = true;
                    break;
                }
            }

            if (!found) {
                // Start a new one somewhere
                for (std::size_t j = 0; j < active_pointers_.size(); j++) {
                    if (active_pointers_[j] == 0) {
                        active_pointers_[j] = points[i].id() + 1;
                        raw_mouse_event(userdata_, eka2l1::vec3(static_cast<int>(points[i].pos().x() * pixel_ratio),
                            static_cast<int>(points[i].pos().y() * pixel_ratio),
                            static_cast<int>(points[i].pressure() * eka2l1::PRESSURE_MAX_NUM)),
                            0, 0, static_cast<int>(j));
                    }
                }
            }
        }

        for (std::size_t i = 0; i < active_pointers_.size(); i++) {
            bool found = false;
            for (std::size_t j = 0; j < points.size(); j++) {
                if (active_pointers_[i] == (points[j].id() + 1)) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                raw_mouse_event(userdata_, eka2l1::vec3(0, 0, 0), 0, 2, static_cast<int>(i));
                active_pointers_[i] = 0;
            }
        }

        return true;
    }

    case QEvent::TouchEnd:
    case QEvent::TouchCancel: {
        for (std::size_t i = 0; i < active_pointers_.size(); i++) {
            if (active_pointers_[i] != 0) {
                raw_mouse_event(userdata_, eka2l1::vec3(0, 0, 0), 0, 2, static_cast<int>(i));
                active_pointers_[i] = 0;
            }
        }

        return true;
    }

    default:
        break;
    }

    return QWidget::event(event);
}

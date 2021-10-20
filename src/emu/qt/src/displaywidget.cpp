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

#include <common/algorithm.h>
#include <common/log.h>
#include <qt/displaywidget.h>

#include <QWindow>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMessageBox>
#include <QGuiApplication>

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

    setMouseTracking(false);

    windowHandle()->setSurfaceType(QWindow::OpenGLSurface);
    windowHandle()->create();
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
        raw_mouse_event(userdata_, eka2l1::vec2(event->pos().x() * pixel_ratio, event->pos().y() * pixel_ratio), button, 0);
    }

    if (touch_pressed && (event->button() == Qt::LeftButton)) {
        touch_pressed(userdata_, eka2l1::vec2(event->pos().x() * pixel_ratio, event->pos().y() * pixel_ratio));
    }
}

void display_widget::mouseReleaseEvent(QMouseEvent *event) {
    if (raw_mouse_event) {
        const qreal pixel_ratio = devicePixelRatioF();

        const int button = eka2l1::common::find_most_significant_bit_one(event->button());
        raw_mouse_event(userdata_, eka2l1::vec2(event->pos().x() * pixel_ratio, event->pos().y() * pixel_ratio), button, 2);
    }

    if (touch_released && (event->button() == Qt::LeftButton)) {
        touch_released(userdata_);
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
        raw_mouse_event(userdata_, eka2l1::vec2(event->pos().x() * pixel_ratio, event->pos().y() * pixel_ratio), button, 1);
    }

    if (touch_move && (event->buttons() & Qt::LeftButton)) {
        touch_move(userdata_, eka2l1::vec2(event->pos().x() * pixel_ratio, event->pos().y() * pixel_ratio));
    }
}

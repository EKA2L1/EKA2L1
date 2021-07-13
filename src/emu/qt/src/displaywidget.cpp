#include <qt/displaywidget.h>
#include <common/log.h>

#include <QWindow>
#include <QtOpenGLWidgets>

display_widget::display_widget(QWidget *parent) :
    QWidget(parent),
    userdata_(nullptr),
    display_context_(nullptr),
    shared_display_context_(nullptr) // For vulkan this may stay null all over
{
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NativeWindow);

    windowHandle()->setSurfaceType(QWindow::OpenGLSurface);
    windowHandle()->create();

    static constexpr std::int32_t TOTAL_TRY = 2;

    display_context_ = new QOpenGLContext;

    QSurfaceFormat::OpenGLContextProfile total_profiles_to_try[TOTAL_TRY] = { QSurfaceFormat::CoreProfile, QSurfaceFormat::CompatibilityProfile };
    int total_minor_to_try[TOTAL_TRY] = { 2, 1 };

    bool success = false;

    for (int i = 0; i < TOTAL_TRY; i++) {
        QSurfaceFormat format_needed;
        format_needed.setProfile(total_profiles_to_try[i]);
        format_needed.setVersion(3, total_minor_to_try[i]);
        format_needed.setSwapBehavior(QSurfaceFormat::DefaultSwapBehavior);
        format_needed.setSwapInterval(0);

        if (display_context_->create()) {
            success = true;
            break;
        }
    }

    if (!success) {
        LOG_ERROR(eka2l1::FRONTEND_UI, "Unable to create required OpenGL context (needed OpenGL 3.1 at minimum!");

        delete display_context_;
        display_context_ = nullptr;
    }
}

display_widget::~display_widget() {    
    if (shared_display_context_)
        delete shared_display_context_;

    if (display_context_)
        delete display_context_;
}

void display_widget::init(std::string title, eka2l1::vec2 size, const std::uint32_t flags) {
    // This is usally called from another thread :D
    // Create shared contexts
    shared_display_context_ = new QOpenGLContext;
    shared_display_context_->setShareContext(display_context_);
    shared_display_context_->setFormat(display_context_->format());

    if (!shared_display_context_->create()) {
        LOG_ERROR(eka2l1::FRONTEND_UI, "Unable to create shared OpenGL context for worker thread!");
    }

    resize(size.x, size.y);
    change_title(title);
}

void display_widget::display_widget::make_current() {
    shared_display_context_->makeCurrent(windowHandle());
}

void display_widget::done_current() {
    shared_display_context_->doneCurrent();
}

void display_widget::swap_buffer() {
    shared_display_context_->swapBuffers(windowHandle());
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

void display_widget::keyPressEvent(QKeyEvent* event) {
    if (button_pressed) {
        button_pressed(userdata_, event->key());
    }
}

void display_widget::keyReleaseEvent(QKeyEvent* event) {
    if (button_released) {
        button_released(userdata_, event->key());
    }
}

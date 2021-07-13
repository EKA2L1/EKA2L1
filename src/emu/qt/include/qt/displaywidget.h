#ifndef DISPLAY_WIDGET_H
#define DISPLAY_WIDGET_H

#include <drivers/graphics/emu_window.h>
#include <QWidget>
#include <QOpenGLContext>

class display_window_widget;

class display_widget : public QWidget, public eka2l1::drivers::emu_window
{
    Q_OBJECT

private:
    void *userdata_;

    QOpenGLContext *display_context_;
    QOpenGLContext *shared_display_context_;

public:
    explicit display_widget(QWidget *parent = nullptr);
    ~display_widget();

    void init(std::string title, eka2l1::vec2 size, const std::uint32_t flags) override;

    void make_current() override;
    void done_current() override;
    void swap_buffer() override;
    void poll_events() override;
    void shutdown() override;
    void set_fullscreen(const bool is_fullscreen) override;

    bool should_quit() override;

    void change_title(std::string title) override;

    eka2l1::vec2 window_size() override;
    eka2l1::vec2 window_fb_size() override;
    eka2l1::vec2d get_mouse_pos() override;

    bool get_mouse_button_hold(const int mouse_btt) override;
    void set_userdata(void *userdata) override;
    void *get_userdata() override;

    bool set_cursor(eka2l1::drivers::cursor *cur) override;
    void cursor_visiblity(const bool visi) override;
    bool cursor_visiblity() override;

    QOpenGLContext *get_parent_context() {
        return display_context_;
    }

    QPaintEngine *paintEngine() const override {
        return nullptr;
    }

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    /*
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;*/
};

#endif // DISPLAY_WIDGET_H

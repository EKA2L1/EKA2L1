#include <qt/winutils.h>
#include <Windows.h>
#include <dwmapi.h>
#include <cstdint>
#include <vector>

#include <QComboBox>
#include <QDialog>
#include <QMenu>
#include <QMainWindow>
#include <QToolBar>
#include <QWidget>
#include <QTimer>

#include <winrt/Windows.UI.ViewManagement.h>

namespace eka2l1::qt {
    bool window_transparent_manager::eventFilter(QObject *watched, QEvent *event) {
        // Because the metaobject of Qt and the runtime type info has not been filled when QEvent::Create is called
        // We can't use dynamic_cast to check the type of the object or use metaObject()->clazzName
        // Further more, I think QMainWindow ignores Create event (it does not invoke it at all)
        // So we rely on the ShowToParent event instead (not the best way but it does the job)
        if (event->type() == QEvent::ShowToParent || event->type() == QEvent::Destroy) {
            if (event->type() == QEvent::ShowToParent) {
                if (!is_object_tracked(watched)) {
                    bool should_track = false;

                    if ((dynamic_cast<QDialog*>(watched)) || (dynamic_cast<QMenu*>(watched)) || (dynamic_cast<QMainWindow*>(watched))
                        || dynamic_cast<QComboBox*>(watched) || dynamic_cast<QToolBar*>(watched)) {
                        should_track = true;
                    }

                    if (should_track) {
                        track_new_object(watched);
                    }
                }
            } else {
                remove_object(watched);
            }
        }

        return false;
    }

    bool window_transparent_manager::is_object_tracked(QObject *object) {
        return objects_.contains(object);
    }

    void window_transparent_manager::track_new_object(QObject *object) {
        objects_.insert(object);

        if (is_theme_set_) {
            set_transparent_blur_effect(object, is_dark_theme_);
        }
    }

    void window_transparent_manager::remove_object(QObject *object) {
        objects_.erase(object);
    }

    void window_transparent_manager::set_dark_theme(bool is_dark_theme) {
        if (!is_theme_set_ || dirtied_ || is_dark_theme_ != is_dark_theme) {
            is_dark_theme_ = is_dark_theme;
            dirtied_ = false;

            if (is_supported_) {
                for (auto object : objects_) {
                    set_transparent_blur_effect(object, is_dark_theme_);

                    if (!is_theme_set_) {
                        auto widget = reinterpret_cast<QWidget*>(object);

                        if (widget->isMaximized()) {
                            widget->showNormal();

                            QTimer::singleShot(0, [widget]() {
                                widget->showMaximized();
                            });
                        } else {
                            widget->showMaximized();

                            QTimer::singleShot(0, [widget]() {
                                widget->showNormal();
                            });
                        }
                    }
                }
            }

            is_theme_set_ = true;
        }
    }

    const bool window_transparent_manager::is_system_dark_mode() const {
        winrt::Windows::UI::ViewManagement::UISettings settings;
        auto color = settings.GetColorValue(winrt::Windows::UI::ViewManagement::UIColorType::Background);

        return color.R == 0 && color.G == 0 && color.B == 0;
    }

    window_transparent_manager::window_transparent_manager(QApplication &application)
        : QObject(nullptr)
        , is_dark_theme_(false)
        , is_theme_set_(false)
        , is_supported_(false)
        , use_acrylic_(false)
        , dirtied_(false) {
        NTSTATUS(WINAPI *RtlGetVersion)(LPOSVERSIONINFOEXW);
        OSVERSIONINFOEXW os_info = { sizeof os_info };

        HMODULE h_module = GetModuleHandleW(L"ntdll.dll");
        if (h_module) {
            RtlGetVersion = reinterpret_cast<decltype(RtlGetVersion)>(GetProcAddress(h_module, "RtlGetVersion"));
            if (RtlGetVersion) {
                if (RtlGetVersion(&os_info) == 0) {
                    is_supported_ = os_info.dwMajorVersion >= 10 && os_info.dwBuildNumber >= 22621;

                    if (is_supported_) {
                        build_number_ = os_info.dwBuildNumber;
                    }
                }
            }
        }

        if (is_supported_) {
            for (auto widget : QApplication::allWidgets()) {
                if ((dynamic_cast<QDialog*>(widget)) || (dynamic_cast<QMenu*>(widget))) {
                    track_new_object(widget);
                }
            }

            application.installEventFilter(this);
        }
    }

    QColor window_transparent_manager::get_accent_color() const {
        winrt::Windows::UI::ViewManagement::UISettings settings;
        auto color = settings.GetColorValue(winrt::Windows::UI::ViewManagement::UIColorType::Accent);

        return QColor(color.R, color.G, color.B);
    }

    void enable_dwm_blur(HWND hwnd, int is_dark_theme, std::uint32_t attribute, std::uint32_t attribute_value) {
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &is_dark_theme, sizeof is_dark_theme);

        COLORREF clr = DWMWA_COLOR_NONE;
        DwmSetWindowAttribute (hwnd, DWMWA_CAPTION_COLOR, &clr, sizeof clr);

        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        DwmSetWindowAttribute(hwnd, attribute, &attribute_value, sizeof attribute_value);
    }

    void window_transparent_manager::set_use_acrylic(bool use_acrylic) {
        if (!is_theme_set_) {
            use_acrylic_ = use_acrylic;
            dirtied_ = true;

            return;
        } else {
            if (use_acrylic_ != use_acrylic) {
                use_acrylic_ = use_acrylic;
                dirtied_ = true;
            }
        }
    }

    void window_transparent_manager::set_transparent_blur_effect(QObject *object, bool is_dark_theme) {
        HWND hwnd = nullptr;

        if (object->isWidgetType()) {
            auto widget = reinterpret_cast<QWidget*>(object);
            bool disable_shadow_drop = false;

            if (auto combo = dynamic_cast<QComboBox*>(object)) {
                widget = reinterpret_cast<QWidget *>(combo->view())->window();
                disable_shadow_drop = true;
            } else if (auto menu = dynamic_cast<QMenu*>(object)) {
                disable_shadow_drop = true;
            }

            hwnd = reinterpret_cast<HWND>(widget->winId());

            widget->setAttribute(Qt::WA_TranslucentBackground);

            if (disable_shadow_drop && ((widget->windowFlags() & Qt::NoDropShadowWindowHint) == 0)) {
                bool was_visible = widget->isVisible();

                widget->setWindowFlags(widget->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);

                if (was_visible) {
                    widget->show();
                }
            }
        }

        enable_dwm_blur(hwnd, is_dark_theme, DWMWA_SYSTEMBACKDROP_TYPE, use_acrylic_ ? DWMSBT_TRANSIENTWINDOW : DWMSBT_TABBEDWINDOW);
    }
}
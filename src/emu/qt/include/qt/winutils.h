#pragma once

#include <QApplication>
#include <QColor>

#include <Windows.h>

#include <set>

namespace eka2l1::qt {
    class window_transparent_manager : public QObject {
    private:
        std::set<QObject*> objects_;

        bool is_dark_theme_;
        bool is_theme_set_;
        bool is_supported_;
        bool use_acrylic_;
        bool dirtied_;

        void track_new_object(QObject *object);
        void remove_object(QObject *object);
        bool is_object_tracked(QObject *object);

        std::uint32_t build_number_;

        void set_transparent_blur_effect(QObject *object, bool is_dark_theme);

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;

    public:
        explicit window_transparent_manager(QApplication &application);

        void set_dark_theme(bool is_dark_theme);
        void set_use_acrylic(bool use_acrylic);

        const bool is_system_dark_mode() const;

        const bool is_dark_theme() const {
            return is_dark_theme_;
        }

        const bool is_enabled() const {
            return is_theme_set_;
        }

        const bool is_supported() const {
            return is_supported_;
        }

        const bool use_acrylic() const {
            return use_acrylic_;
        }

        QColor get_accent_color() const;
    };
}
/*
 * Copyright (c) 2019 EKA2L1 Team
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

#pragma once

#include <common/container.h>
#include <common/vecx.h>

#include <drivers/graphics/common.h>
#include <services/window/classes/config.h>
#include <services/window/common.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace eka2l1 {
    namespace kernel {
        class chunk;
    }

    namespace config {
        struct app_setting;
    }

    class window_server;
    class ntimer;
}

namespace eka2l1::drivers {
    class graphics_driver;
    class graphics_command_builder;
}

namespace eka2l1::epoc {
    const std::uint32_t WORD_PALETTE_ENTRIES_COUNT = 16;

    struct window;
    struct window_group;
    struct screen;

    enum focus_change_property {
        focus_change_target,
        focus_change_name
    };

    using focus_change_callback_handler = std::function<void(void *, window_group *, focus_change_property)>;
    using screen_redraw_callback_handler = std::function<void(void *, screen *, bool)>;
    using screen_mode_change_callback_handler = std::function<void(void *, screen *, const int)>;

    struct screen {
        int number;
        int ui_rotation; ///< Rotation for UI display. So nikita can skip neck day.

        std::uint8_t refresh_rate;

        float display_scale_factor;
        float logic_scale_factor_x;
        float logic_scale_factor_y;
        float requested_ui_scale_factor;

        // The root window, used to traverse window tree
        // Draw order will be child in front of parent, newer in front of older.
        std::unique_ptr<epoc::window> root;
        drivers::handle screen_texture; ///< Server handle to texture of the screen
        drivers::handle dsa_texture;    ///< Texture use for temporary DSA transfer

        epoc::display_mode disp_mode;

        std::uint64_t last_vsync;
        std::uint64_t last_fps_check;
        std::uint64_t last_fps;
        std::uint64_t frame_passed_per_sec;

        epoc::config::screen scr_config; ///< All mode of this screen
        std::uint8_t crr_mode; ///< The current mode being used by the screen.
        std::uint8_t physical_mode; ///< Mode that orientation is normal.

        epoc::window_group *focus; ///< Current window group that is being focused

        screen *next;

        kernel::chunk *screen_buffer_chunk;
        std::mutex screen_mutex;

        // Position of this screen in graphics driver
        // Update in graphics driver thread and read in os thread
        eka2l1::vec2 absolute_pos;

        std::map<std::int32_t, eka2l1::rect> pointer_areas_;
        eka2l1::vec2 pointer_cursor_pos_;

        std::uint32_t flags_ = 0;
        std::int32_t active_dsa_count_ = 0;

        bool sync_screen_buffer = false;

        enum {
            FLAG_NEED_RECALC_VISIBLE = 1 << 0,
            FLAG_ORIENTATION_LOCK = 1 << 1,
            FLAG_AUTO_CLEAR_BACKGROUND = 1 << 2,
            FLAG_SERVER_REDRAW_PENDING = 1 << 3,
            FLAG_CLIENT_REDRAW_PENDING = 1 << 4,
            FLAG_SCREEN_UPSCALE_FACTOR_LOCK = 1 << 5,
            FLAG_IS_SCREENPLAY = 1 << 6
        };

        using focus_change_callback = std::pair<void *, focus_change_callback_handler>;
        using screen_redraw_callback = std::pair<void *, screen_redraw_callback_handler>;
        using screen_mode_change_callback = std::pair<void *, screen_mode_change_callback_handler>;

        common::identity_container<focus_change_callback> focus_callbacks;
        common::identity_container<screen_redraw_callback> screen_redraw_callbacks;
        common::identity_container<screen_mode_change_callback> screen_mode_change_callbacks;

        void fire_focus_change_callbacks(const focus_change_property property);
        void fire_screen_redraw_callbacks(const bool is_dsa);
        void fire_screen_mode_change_callbacks(const int old_mode);

        std::size_t add_focus_change_callback(void *userdata, focus_change_callback_handler handler);
        bool remove_focus_change_callback(const std::size_t cb);

        std::size_t add_screen_redraw_callback(void *userdata, screen_redraw_callback_handler handler);
        bool remove_screen_redraw_callback(const std::size_t cb);

        std::size_t add_screen_mode_change_callback(void *userdata, screen_mode_change_callback_handler handler);
        bool remove_screen_mode_change_callback(const std::size_t cb);

        void vsync(ntimer *timing, std::uint64_t &next_vsync_us);

        explicit screen(const int number, epoc::config::screen &scr_conf);

        void set_rotation(window_server *winserv, drivers::graphics_driver *drv, int rot);
        void set_orientation_lock(drivers::graphics_driver *drv, const bool lock);
        void abort_all_dsas(const std::int32_t reason);
        void recalculate_visible_regions();

        void restore_from_config(drivers::graphics_driver *driver, const eka2l1::config::app_setting &setting);
        void store_to_config(drivers::graphics_driver *driver, eka2l1::config::app_setting &setting);
        void try_change_display_rescale(drivers::graphics_driver *driver, const float scale_factor);

        // ========================= UTILITIES FUNCTIONS ===========================
        epoc::window_group *get_group_chain();

        /**
         * @brief       Get the start pointer to write screen data.
         * @returns     Pointer to the start of the screen data.
         */
        std::uint8_t *screen_buffer_ptr();

        /**
         * \brief Get the size of this screen, in pixels.
         */
        eka2l1::vec2 size() const;

        /**
         * \brief   Retrieve info of current screen mode.
         * \returns Info of current screen mode
         */
        const epoc::config::screen_mode &current_mode() const;

        /**
         * \brief Get the maximum number of modes this screen support.
         */
        const int total_screen_mode() const;

        const epoc::config::screen_mode *mode_info(const int number) const;

        const void get_max_num_colors(int &colors, int &greys) const;

        void sync_screen_buffer_data(drivers::graphics_driver *driver);

        /**
         * \brief Set screen mode.
         */
        void set_screen_mode(window_server *winserv, drivers::graphics_driver *drv, const int mode);
        void set_native_scale_factor(drivers::graphics_driver *driver, const float scale_factor_x,
            const float scale_factor_y);

        /**
         * \brief Resize the screen.
         * 
         * This resize the screen. Which means all pixel will be lose, and redraw will happens, if
         * GPU renderer is enabled.
         * 
         * DSA (Direct Screen Access) will also be lost too, it will not be saved.
         * Note: For now DSA will be lost, yes. But I'm thinking we could alloc a buffer and let user
         * edit pixel there. Well read can kind of be hard though. At the end, upload DSA texture
         * and draw it over all windows.
         * 
         * Does this apply to rotation though? TODO. But most likely we should...
         * 
         * This function also creates the screen bitmap from server side if there is currently none.
         * 
         * WARNING: Don't call this from outside of this struct. Nguy hiểm
         * 
         * \param driver     The graphics driver associated with the screen.
         * \param new_size   Size of the screen.
         */
        void resize(drivers::graphics_driver *driver, const eka2l1::vec2 &new_size);

        void deinit(drivers::graphics_driver *driver);
        bool redraw(drivers::graphics_command_builder &builder, const bool need_bind);

        /**
         * \brief Redraw the screen.
         * \param driver Pointer to the graphics driver
         */
        void redraw(drivers::graphics_driver *driver);

        /**
         * \brief Update the window group focus.
         */
        epoc::window_group *update_focus(window_server *serv, epoc::window_group *closing_group);

        const bool orientation_lock() const {
            return flags_ & FLAG_ORIENTATION_LOCK;
        }

        void orientation_lock(const bool value) {
            if (value) {
                flags_ |= FLAG_ORIENTATION_LOCK;
            } else {
                flags_ &= ~FLAG_ORIENTATION_LOCK;
            }
        }

        const bool need_update_visible_regions() const {
            return flags_ & FLAG_NEED_RECALC_VISIBLE;
        }

        const bool auto_clear_enabled() const {
            return flags_ & FLAG_AUTO_CLEAR_BACKGROUND;
        }

        void set_client_draw_pending() {
            flags_ |= FLAG_CLIENT_REDRAW_PENDING;
        }
        
        void set_is_screenplay_architecture(bool is_screenplay) {
            if (!is_screenplay) {
                flags_ &= ~FLAG_IS_SCREENPLAY;
                return;
            }
            flags_ |= FLAG_IS_SCREENPLAY;
        }

        bool is_screenplay_architecture() const {
            return flags_ & FLAG_IS_SCREENPLAY;
        }

        void need_update_visible_regions(const bool value);

        void ref_dsa_usage();
        void deref_dsa_usage();
    };
}

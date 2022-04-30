/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <kernel/common.h>
#include <kernel/kernel.h>
#include <kernel/timing.h>
#include <services/window/window.h>
#include <system/epoc.h>
#include <system/hal.h>

#include <loader/rom.h>

#include <common/algorithm.h>
#include <common/log.h>

#include <common/common.h>
#include <utils/err.h>

#include <drivers/graphics/graphics.h>
#include <utils/des.h>

#include <config/config.h>

#define REGISTER_HAL_FUNC(op, hal_name, func) \
    funcs.emplace(op, std::bind(&hal_name::func, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))

#define REGISTER_HAL_D(sys, cage, hal_name)                               \
    std::unique_ptr<epoc::hal> hal_com = std::make_unique<hal_name>(sys); \
    sys->add_new_hal(cage, hal_com);

#define REGISTER_HAL(sys, cage, hal_name)      \
    hal_com = std::make_unique<hal_name>(sys); \
    sys->add_new_hal(cage, hal_com);

namespace eka2l1::epoc {
    static constexpr std::uint32_t HAL_CONTRAST_MAX = 100;

    hal::hal(eka2l1::system *sys)
        : sys(sys) {}

    int hal::do_hal(std::uint32_t func, int *a1, int *a2, const std::uint16_t device_num) {
        auto func_pair = funcs.find(func);

        if (func_pair == funcs.end()) {
            return -1;
        }

        return func_pair->second(a1, a2, device_num);
    }

    /**
     * \brief Kernel HAL cagetory. 
     * 
     * Contains HAL of drivers, memory, etc...
     */
    struct kern_hal : public eka2l1::epoc::hal {
        /**
         * \brief Get the size of a page. 
         *
         * \param a1 The pointer to the integer destination, supposed to
         *           contains the page size.
         * \param a2 Unused.
         */
        int page_size(int *a1, int *a2, const std::uint16_t device_num) {
            *a1 = static_cast<int>(sys->get_memory_system()->get_page_size());
            return epoc::error_none;
        }

        int memory_info(int *a1, int *a2, const std::uint16_t device_num) {
            des8 *buf = reinterpret_cast<des8 *>(a1);
            epoc::memory_info_v1 mem_info;

            mem_info.total_ram_in_bytes_ = static_cast<int>(common::MB(256));
            mem_info.rom_is_reprogrammable_ = false;
            mem_info.max_free_ram_in_bytes_ = static_cast<int>(common::MB(256));
            mem_info.free_ram_in_bytes_ = static_cast<int>(common::MB(256));
            mem_info.total_rom_in_bytes_ = sys->get_rom_info()->header.rom_size;

            // This value is appr. the same as rom.
            mem_info.internal_disk_ram_in_bytes_ = mem_info.total_rom_in_bytes_;

            buf->assign(sys->get_kernel_system()->crr_process(), reinterpret_cast<std::uint8_t *>(&mem_info), sizeof(mem_info));

            return epoc::error_none;
        }

        int tick_period(int *a1, int *a2, const std::uint16_t device_num) {
            std::uint32_t *tick_period_in_microsecs = reinterpret_cast<std::uint32_t *>(a1);
            *tick_period_in_microsecs = 1000000 / static_cast<std::uint32_t>(epoc::TICK_TIMER_HZ);

            return epoc::error_none;
        }

        int hardware_floating_point(int *a1, int *a2, const std::uint16_t device_num) {
            // Can handle NEON also
            *a1 = floating_point_type_vfpv3;
            return epoc::error_none;
        }

        explicit kern_hal(eka2l1::system *sys)
            : hal(sys) {
            REGISTER_HAL_FUNC(kernel_hal_memory_info, kern_hal, memory_info);
            REGISTER_HAL_FUNC(kernel_hal_page_size_in_bytes, kern_hal, page_size);
            REGISTER_HAL_FUNC(kernel_hal_tick_period, kern_hal, tick_period);
            REGISTER_HAL_FUNC(kernel_hal_hardware_floating_point, kern_hal, hardware_floating_point);
        }
    };

    struct variant_hal : public eka2l1::epoc::hal {
        int get_variant_info(int *a1, int *a2, const std::uint16_t device_num) {
            epoc::des8 *package = reinterpret_cast<epoc::des8 *>(a1);
            epoc::variant_info_v1 *info_ptr = reinterpret_cast<epoc::variant_info_v1 *>(package->get_pointer(sys->get_kernel_system()->crr_process()));

            loader::rom &rom_info = *(sys->get_rom_info());
            info_ptr->major_ = rom_info.header.major;
            info_ptr->minor_ = rom_info.header.minor;
            info_ptr->build_ = rom_info.header.build;

            info_ptr->processor_clock_in_mhz_ = sys->get_ntimer()->get_clock_frequency_mhz();
            info_ptr->machine_uid_ = 0x70000001;

            return 0;
        }

        variant_hal(eka2l1::system *sys)
            : hal(sys) {
            REGISTER_HAL_FUNC(variant_hal_variant_info, variant_hal, get_variant_info);
        }
    };

    struct display_hal : public hal {
        window_server *winserv_;

        static void get_screen_info_from_scr_object(epoc::screen *scr, epoc::screen_info_v1 &info) {
            info.window_handle_valid_ = false;
            info.screen_address_valid_ = true;
            info.screen_address_ = scr->screen_buffer_chunk->base(nullptr).cast<void>();
            info.screen_size_ = scr->size();
        }

        void get_video_info_from_scr_object(epoc::screen *scr, const epoc::display_mode &mode, epoc::video_info_v1 &info) {
            if (mode != scr->disp_mode) {
                LOG_WARN(SYSTEM, "Trying to get video info with a different display mode {}", static_cast<int>(mode));
            }

            kernel_system *kern = sys->get_kernel_system();

            info.size_in_pixels_ = scr->size();
            info.size_in_twips_ = info.size_in_pixels_ * epoc::get_approximate_pixel_to_twips_mul(kern->get_epoc_version());
            info.is_mono_ = is_display_mode_mono(mode);
            info.bits_per_pixel_ = get_bpp_from_display_mode(mode);
            info.is_pixel_order_rgb_ = (mode >= epoc::display_mode::color4k);

            // TODO: Verify
            info.is_pixel_order_landspace_ = (info.size_in_pixels_.x > info.size_in_pixels_.y);
            info.is_palettelized_ = !info.is_mono_ && (mode < epoc::display_mode::color4k);

            // Intentional
            info.video_address_ = scr->screen_buffer_chunk->base(nullptr).ptr_address();
            info.offset_to_first_pixel_ = sizeof(std::uint16_t) * epoc::WORD_PALETTE_ENTRIES_COUNT;
            info.offset_between_lines_ = ((info.bits_per_pixel_ + 31) / 32) * 4 * info.size_in_pixels_.x;
            info.display_mode_ = 0;
        }

        int current_screen_info(int *a1, int *a2, const std::uint16_t device_num) {
            assert(winserv_);

            epoc::des8 *package = reinterpret_cast<epoc::des8 *>(a1);
            epoc::screen_info_v1 *info_ptr = reinterpret_cast<epoc::screen_info_v1 *>(
                package->get_pointer(sys->get_kernel_system()->crr_process()));

            epoc::screen *scr = winserv_->get_current_focus_screen();

            if (!scr || !info_ptr) {
                return epoc::error_not_found;
            }

            get_screen_info_from_scr_object(scr, *info_ptr);
            return epoc::error_none;
        }

        int current_mode_info(int *a1, int *a2, const std::uint16_t device_num) {
            assert(winserv_);

            epoc::des8 *package = reinterpret_cast<epoc::des8 *>(a1);
            epoc::video_info_v1 *info_ptr = reinterpret_cast<epoc::video_info_v1 *>(
                package->get_pointer(sys->get_kernel_system()->crr_process()));

            epoc::screen *scr = winserv_->get_screen(device_num);

            if (!scr) {
                return epoc::error_not_found;
            }

            get_video_info_from_scr_object(scr, scr->disp_mode, *info_ptr);
            return 0;
        }

        int specified_mode_info(int *a1, int *a2, const std::uint16_t device_num) {
            assert(winserv_);

            epoc::des8 *package = reinterpret_cast<epoc::des8 *>(a2);
            epoc::video_info_v1 *info_ptr = reinterpret_cast<epoc::video_info_v1 *>(
                package->get_pointer(sys->get_kernel_system()->crr_process()));

            epoc::screen *scr = winserv_->get_screen(device_num);

            if (!scr) {
                return epoc::error_not_found;
            }

            get_video_info_from_scr_object(scr, static_cast<epoc::display_mode>(*a1), *info_ptr);
            return 0;
        }

        int color_count(int *a1, int *a2, const std::uint16_t device_num) {
            assert(winserv_);

            epoc::screen *scr = winserv_->get_screen(device_num);

            if (!scr) {
                return epoc::error_not_found;
            }

            *a1 = get_num_colors_from_display_mode(scr->disp_mode);
            return 0;
        }

        int mode(int *a1, int *a2, const std::uint16_t device_num) {
            assert(winserv_);

            epoc::screen *scr = winserv_->get_screen(device_num);

            if (!scr) {
                return epoc::error_not_found;
            }

            *a1 = static_cast<int>(scr->disp_mode);
            return 0;
        }

        int backlight_on(int *a1, int *a2, const std::uint16_t device_num) {
            LOG_TRACE(SYSTEM, "Backlight on stubbed to false");
            *a1 = false;

            return 0;
        }

        explicit display_hal(system *sys)
            : hal(sys)
            , winserv_(nullptr) {
            REGISTER_HAL_FUNC(display_hal_screen_info, display_hal, current_screen_info);
            REGISTER_HAL_FUNC(display_hal_current_mode_info, display_hal, current_mode_info);
            REGISTER_HAL_FUNC(display_hal_specified_mode_info, display_hal, specified_mode_info);
            REGISTER_HAL_FUNC(display_hal_colors, display_hal, color_count);
            REGISTER_HAL_FUNC(display_hal_mode, display_hal, mode);
            REGISTER_HAL_FUNC(display_hal_backlight_on, display_hal, backlight_on);

            winserv_ = reinterpret_cast<window_server *>(sys->get_kernel_system()->get_by_name<service::server>(
                eka2l1::get_winserv_name_by_epocver(sys->get_symbian_version_use())));
        }
    };

    struct digitiser_hal : public hal {
        // Max supported pointer count by CONE
        static constexpr std::uint32_t MAX_POINTER_SUPPORTED = 8;

        window_server *winserv_;

        int get_xy_info(int *a1, int *a2, const std::uint16_t device_num) {
            assert(winserv_);

            epoc::screen *scr = winserv_->get_screen(device_num);

            if (!scr) {
                return epoc::error_not_found;
            }

            epoc::des8 *package = reinterpret_cast<epoc::des8 *>(a1);
            kernel::process *crr = sys->get_kernel_system()->crr_process();

            if (package->get_length() == sizeof(digitiser_info_v1)) {
                digitiser_info_v1 *info = reinterpret_cast<digitiser_info_v1 *>(package->get_pointer(crr));
                info->offset_to_first_usable_ = { 0, 0 };
                info->size_usable_ = scr->size();

                return epoc::error_none;
            }

            return epoc::error_general;
        }

        int get_3d_info(int *a1, int *a2, const std::uint16_t device_num) {
            if (!a1) {
                return epoc::error_argument;
            }

            assert(winserv_);

            epoc::screen *scr = winserv_->get_screen(device_num);
            if (!scr) {
                return epoc::error_not_found;
            }


            epoc::des8 *package = reinterpret_cast<epoc::des8 *>(a1);
            kernel::process *crr = sys->get_kernel_system()->crr_process();

            LOG_TRACE(SYSTEM, "Get 3d info (digistier info) is incomplete!");

            digitister_info_v2 *info = reinterpret_cast<digitister_info_v2 *>(package->get_pointer(crr));
            info->offset_to_first_usable_ = { 0, 0 };
            info->size_usable_ = scr->size();
            info->max_pointers_ = MAX_POINTER_SUPPORTED;
            info->number_of_pointers_ = MAX_POINTER_SUPPORTED;

            return epoc::error_none;
        }

        explicit digitiser_hal(system *sys)
            : hal(sys)
            , winserv_(nullptr) {
            REGISTER_HAL_FUNC(digitiser_hal_hal_xy_info, digitiser_hal, get_xy_info);
            REGISTER_HAL_FUNC(digitiser_hal_3d_info, digitiser_hal, get_3d_info);

            winserv_ = reinterpret_cast<window_server *>(sys->get_kernel_system()->get_by_name<service::server>(
                eka2l1::get_winserv_name_by_epocver(sys->get_symbian_version_use())));
        }
    };

    struct keyboard_hal : public hal {
        int get_info(int *a1, int *a2, const std::uint16_t device_num) {
            epoc::des8 *package = reinterpret_cast<epoc::des8 *>(a1);
            epoc::keyboard_info_v01 *info_ptr = reinterpret_cast<epoc::keyboard_info_v01 *>(
                package->get_pointer(sys->get_kernel_system()->crr_process()));

            if (!info_ptr) {
                return epoc::error_argument;
            }

            LOG_TRACE(SYSTEM, "GetKeyboardInfo stubbed with keypad");

            // Middle, left right softkeys, 4 arrow keys, one call button and end call button
            // 9 number keys. That should be 18
            static constexpr const std::uint32_t MAX_KEYPAD_APP_KEY_NUM = 4;
            static constexpr const std::uint32_t MAX_KEYPAD_KEY_NUM = 18;

            info_ptr->app_keys_ = MAX_KEYPAD_APP_KEY_NUM;
            info_ptr->device_keys_ = MAX_KEYPAD_KEY_NUM;
            info_ptr->type_ = keyboard_type_numpad;

            return 0;
        }

        explicit keyboard_hal(system *sys)
            : hal(sys) {
            REGISTER_HAL_FUNC(keyboard_hal_get_info, keyboard_hal, get_info);
        }
    };

    void init_hal(eka2l1::system *sys) {
        REGISTER_HAL_D(sys, hal_category_kernel, kern_hal);
        REGISTER_HAL(sys, hal_category_variant, variant_hal);
        REGISTER_HAL(sys, hal_category_display, display_hal);
        REGISTER_HAL(sys, hal_category_digister, digitiser_hal);
        REGISTER_HAL(sys, hal_category_keyboard, keyboard_hal);
    }

    int do_hal(eka2l1::system *sys, uint32_t cage, uint32_t func, int *a1, int *a2) {
        hal *hal_com = sys->get_hal(static_cast<std::uint16_t>(cage));

        if (!hal_com) {
            LOG_TRACE(SYSTEM, "HAL cagetory not found or unimplemented: 0x{:x} (for function: 0x{:x})",
                cage, func);

            return epoc::error_not_found;
        }

        int ret = hal_com->do_hal(func, a1, a2, static_cast<std::uint16_t>(cage >> 16));

        if (ret == -1) {
            LOG_WARN(SYSTEM, "Unimplemented HAL function, cagetory: 0x{:x}, function: 0x{:x}",
                cage, func);
        }

        return ret;
    }

    static void fill_machine_info(eka2l1::system *sys, machine_info_v1 &info) {
        kernel_system *kern = sys->get_kernel_system();
        window_server *winserv = reinterpret_cast<window_server *>(kern->get_by_name<service::server>(
            eka2l1::get_winserv_name_by_epocver(kern->get_epoc_version())));

        epoc::screen *crr_screen = winserv->get_current_focus_screen();

        info.backlight_present_ = 1;
        info.clock_speed_mhz_ = 0;
        info.display_id_ = 0;
        info.xy_input_size_pixels_ = crr_screen->size();
        info.display_size_pixels_ = crr_screen->size();
        info.physical_screen_size_ = info.display_size_pixels_ * epoc::get_approximate_pixel_to_twips_mul(kern->get_epoc_version()); // In twips
        info.input_type_ = xy_input_type_pointer;
        info.keyboard_id_ = 0;
        info.keyboard_present_ = 1;
        info.machine_unique_id_ = 0; // TODO: Machine Unique ID here
        info.maximum_display_color_ = get_num_colors_from_display_mode(crr_screen->disp_mode);
        info.offset_to_display_in_pixels_ = eka2l1::point(0, 0);
        info.speed_factor_ = 1;

        info.rom_ver_.major = 1;
        info.rom_ver_.minor = 0;
        info.rom_ver_.build = 0;
    }

    static int do_hal_machine_info(eka2l1::system *sys, void *data) {
        epoc::des8 *buf = reinterpret_cast<epoc::des8 *>(data);
        kernel::process *pr = sys->get_kernel_system()->crr_process();
        machine_info_v1 *info = reinterpret_cast<machine_info_v1 *>(buf->get_pointer_raw(pr));

        if (!info) {
            LOG_ERROR(SYSTEM, "Trying to get machine info but buffer is null!");
            return -1;
        }

        fill_machine_info(sys, *info);
        return 0;
    }

    int do_hal_by_data_num(eka2l1::system *sys, const std::uint32_t data_num, void *data) {
        switch (data_num) {
        case kernel::hal_data_eka1_machine_info:
            return do_hal_machine_info(sys, data);

        case kernel::hal_data_eka1_memory_info: {
            kern_hal *the_hal = reinterpret_cast<kern_hal *>(sys->get_hal(hal_category_kernel));

            if (!the_hal) {
                return -1;
            }

            return the_hal->memory_info(reinterpret_cast<int *>(data), nullptr, 0);
        }

        case kernel::hal_data_eka1_manufacturer_hardware_rev:
            *reinterpret_cast<std::uint32_t *>(data) = 1;
            break;

        case kernel::hal_data_eka1_page_size:
            *reinterpret_cast<std::uint32_t *>(data) = sys->get_memory_system()->get_page_size();
            break;

        case kernel::hal_data_eka1_tick_period:
            *reinterpret_cast<std::uint32_t *>(data) = 1000000 / epoc::TICK_TIMER_HZ;
            break;

        case kernel::hal_data_eka1_display_contrast_max:
            *reinterpret_cast<std::uint32_t *>(data) = HAL_CONTRAST_MAX;
            break;

        case kernel::hal_data_eka1_display_memory_address: {    
            window_server *winserv = reinterpret_cast<window_server *>(sys->get_kernel_system()->get_by_name<service::server>(
                eka2l1::get_winserv_name_by_epocver(sys->get_symbian_version_use())));

            epoc::screen *crr_screen = winserv->get_current_focus_screen();

            *reinterpret_cast<std::uint32_t *>(data) = crr_screen->screen_buffer_chunk->base(nullptr).ptr_address();
            break;
        }

        case kernel::hal_data_eka1_screen_info: {
            display_hal *the_hal = reinterpret_cast<display_hal *>(sys->get_hal(hal_category_display));

            if (!the_hal) {
                return -1;
            }

            return the_hal->current_screen_info(reinterpret_cast<int *>(data), nullptr, 0);
        }

        default:
            LOG_ERROR(SYSTEM, "Unimplemented HAL function handler for key {}", data_num);
            return -1;
        }

        return 0;
    }
}
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

#pragma once

#include <epoc/ptr.h>
#include <common/vecx.h>

#include <functional>
#include <unordered_map>

namespace eka2l1 {
    class system;
}

namespace eka2l1::epoc {
    /**
     * \brief List of opcodes which belong to HAL Kernel category.
     */
    enum kernel_hal_function {
        kernel_hal_memory_info = 0,
        kernel_hal_rom_info = 1,
        kernel_hal_startup_reason = 2,
        kernel_hal_fault_reason = 3,
        kernel_hal_exception_id = 4,
        kernel_hal_exception_info = 5,
        kernel_hal_cpu_info = 6,
        kernel_hal_page_size_in_bytes = 7,
        kernel_hal_tick_period = 8,
        kernel_hal_mem_model_info = 9,
        kernel_hal_fast_counter_freq = 10,
        kernel_hal_ntick_period = 11,
        kernel_hal_hardware_floating_point = 12,
        kernel_hal_get_non_secure_clock_offset = 13,
        kernel_hal_set_non_secure_clock_offset = 14,
        kernel_hal_smp_supported = 15,
        kernel_hal_num_logical_cpus = 16,
        kernel_hal_supervisor_barrier = 17,
        kernel_hal_floating_point_system_id = 18,
        kernel_hal_lock_thread_to_cpu = 19,
        kernel_hal_config_flags = 20,       // Superpage
        kernel_hal_cpu_states = 21,
        kernel_hal_set_number_of_cpus = 22
    };

    enum variant_hal_function {
        variant_hal_variant_info = 0,
        variant_hal_debug_port_set = 1,
        variant_hal_debug_port_get = 2,
        variant_hal_led_mask_set = 3,
        variant_hal_switches = 4,
        variant_hal_custom_restart = 5,
        variant_hal_custom_restart_reason = 6,
        variant_hal_case_state = 7,
        variant_hal_current_number_of_screens = 8,
        variant_hal_persist_startup_mode = 9,
        variant_hal_get_persisted_startup_mode = 10,
        variant_hal_get_maximum_custom_restart_reasons = 11,
        variant_hal_get_maxomum_restart_startup_modes = 12,
        variant_hal_timout_expansion = 13,
        variant_hal_serial_number = 14,
        variant_hal_profiling_default_interrupt_base = 15
    };

    enum display_hal_function {
        display_hal_screen_info = 0,
        display_hal_ws_register_switch_on_screen_handling = 1,
        display_hal_ws_switch_on_screen = 2,
        display_hal_max_display_contrast = 3,
        display_hal_set_display_contrast = 4,
        display_hal_display_contrast = 5,
        display_hal_set_backlight_behavior = 6,
        display_hal_backlight_behavior = 7,
        display_hal_set_backlight_on_time = 8,
        display_hal_backlight_on_time = 9,
        display_hal_set_backlight_on = 10,
        display_hal_backlight_on = 11,
        display_hal_max_display_brightness = 12,
        display_hal_set_display_brightness = 13,
        display_hal_display_brightness = 14,
        display_hal_mode_count = 15,
        display_hal_set_mode = 16,
        display_hal_mode = 17,
        display_hal_set_palette_entry = 18,
        display_hal_palette_entry = 19,
        display_hal_set_state = 20,
        display_hal_state = 21,
        display_hal_colors = 22,
        display_hal_current_mode_info = 23,
        display_hal_specified_mode_info = 24,
        // display_hal_switch_off_screen = 25, - This call can be used in lower version of Symbian ?
        display_hal_block_fill = 25,
        display_hal_block_copy = 26,
        display_hal_secure = 27,
        display_hal_set_secure = 28,
        display_hal_get_display_mem_addr = 29,
        display_hal_get_display_mem_handle = 30,
        display_hal_num_resolutions = 31,
        display_hal_specific_screen_info = 32,
        display_hal_current_screen_info = 33,
        display_hal_set_display_state = 34,
        display_hal_get_state_spinner = 35
    };

    enum digitiser_hal_function {
        digitiser_hal_set_xy_input_calibration = 0,
        digitiser_hal_calibration_points = 1,
        digitiser_hal_save_xy_input_calibration = 2,
        digitiser_hal_restore_xy_input_calibration = 3,
        digitiser_hal_hal_xy_info = 4,
        digitiser_hal_xy_state = 5,
        digitiser_hal_hal_set_xy_state = 6,
        digitiser_hal_3d_pointer = 7,
        digitiser_hal_set_z_range = 8,
        digitiser_hal_set_number_of_pointers = 9,
        digitiser_hal_3d_info = 10,
        digitiser_hal_orientation = 11
    };

    struct memory_info_v1 {
        std::uint32_t total_ram_in_bytes_;
        std::uint32_t total_rom_in_bytes_;
        std::uint32_t max_free_ram_in_bytes_;
        std::uint32_t free_ram_in_bytes_;
        std::uint32_t internal_disk_ram_in_bytes_;
        std::int32_t rom_is_reprogrammable_;
    };

    static_assert(sizeof(memory_info_v1) == 24, "Size of memory info v1 is invalid!");

    #pragma pack(push, 1)
    struct variant_info_v1 {
        std::uint8_t major_;
        std::uint8_t minor_;
        std::uint16_t build_;
        std::uint64_t machine_uid_;
        std::uint32_t led_caps_;
        std::uint32_t processor_clock_in_mhz_;
        std::uint32_t speed_factor_;
    };
    #pragma pack(pop)

    static_assert(sizeof(variant_info_v1) == 24, "Size of variant info v1 is invalid!");

    class video_info_v1 {
    public:
        vec2 size_in_pixels_;
        vec2 size_in_twips_;
        std::int32_t is_mono_;
        std::int32_t is_palettelized_;
        std::int32_t bits_per_pixel_;
        std::uint32_t video_address_;
        std::int32_t offset_to_first_pixel_;
        std::int32_t offset_between_lines_;
        std::int32_t is_pixel_order_rgb_;
        std::int32_t is_pixel_order_landspace_;
        std::int32_t display_mode_;
    };

    static_assert(sizeof(video_info_v1) == 52);

    struct screen_info_v1 {
    public:
        std::int32_t window_handle_valid_;
        eka2l1::ptr<void> window_handle_;
        std::int32_t screen_address_valid_;
        eka2l1::ptr<void> screen_address_;
        eka2l1::vec2 screen_size_;
    };

    struct digitiser_info_v1 {
        eka2l1::vec2 offset_to_first_useable_;
        eka2l1::vec2 size_usable_;
    };

    /*! \brief A HAL function. Each function has minimum of 0 arg and maximum of 2 args. */
    using hal_func = std::function<int(int *, int *, std::uint16_t)>;
    using hal_cagetory_funcs = std::unordered_map<uint32_t, hal_func>;

    /**
     * \brief Base class for all High Level Abstraction Layer Cagetory.
     */
    struct hal {
        hal_cagetory_funcs funcs;
        eka2l1::system *sys;

    public:
        explicit hal(eka2l1::system *sys);

        /**
         * \brief Execute an HAL function. 
         *
         * \param func Function opcode.
         * \returns HAL operation result.
         */
        int do_hal(uint32_t func, int *a1, int *a2, const std::uint16_t device_number);
    };

    /** 
     * \brief Initalize HAL. 
     *
     * This adds all HAL cagetory to the system. 
    */
    void init_hal(eka2l1::system *sys);

    /**
     * \brief Do a HAL function with the specified cagetory and key. 
     *
     * \param cage The HAL cagetory.
     * \param func The HAL function opcode.
     *
     * \returns Operation result.
     */
    int do_hal(eka2l1::system *sys, uint32_t cage, uint32_t func, int *a1, int *a2);
}
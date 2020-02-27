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

#include <epoc/epoc.h>
#include <epoc/hal.h>
#include <epoc/kernel.h>
#include <epoc/timing.h>
#include <epoc/services/window/window.h>

#include <epoc/loader/rom.h>

#include <common/algorithm.h>
#include <common/log.h>

#include <epoc/utils/err.h>

#include <drivers/graphics/graphics.h>
#include <epoc/utils/des.h>

#include <manager/config.h>

#define REGISTER_HAL_FUNC(op, hal_name, func) \
    funcs.emplace(op, std::bind(&hal_name::func, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))

#define REGISTER_HAL_D(sys, cage, hal_name) \
    std::unique_ptr<epoc::hal> hal_com = std::make_unique<hal_name>(sys);   \
    sys->add_new_hal(cage, hal_com);

#define REGISTER_HAL(sys, cage, hal_name) \
    hal_com = std::make_unique<hal_name>(sys);   \
    sys->add_new_hal(cage, hal_com);

namespace eka2l1::epoc {
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
            TMemoryInfoV1 memInfo;

            memInfo.iTotalRamInBytes = static_cast<int>(common::MB(256));
            memInfo.iRomIsReprogrammable = false;
            memInfo.iMaxFreeRamInBytes = static_cast<int>(common::MB(256));
            memInfo.iFreeRamInBytes = static_cast<int>(common::MB(256));
            memInfo.iTotalRomInBytes = sys->get_rom_info()->header.rom_size;
            memInfo.iInternalDiskRamInBytes = memInfo.iTotalRomInBytes; // This value is appr. the same as rom.

            std::string dat;
            dat.resize(sizeof(memInfo));

            memcpy(dat.data(), &memInfo, sizeof(memInfo));

            buf->assign(sys->get_kernel_system()->crr_process(), dat);

            return epoc::error_none;
        }

        explicit kern_hal(eka2l1::system *sys)
            : hal(sys) {
            REGISTER_HAL_FUNC(EKernelHalPageSizeInBytes, kern_hal, page_size);
            REGISTER_HAL_FUNC(EKernelHalMemoryInfo, kern_hal, memory_info);
        }
    };

    struct variant_hal : public eka2l1::epoc::hal {
        int get_variant_info(int *a1, int *a2, const std::uint16_t device_num) {
            epoc::des8 *package = reinterpret_cast<epoc::des8 *>(a1);
            epoc::TVariantInfoV1 *info_ptr = reinterpret_cast<epoc::TVariantInfoV1 *>(
                package->get_pointer(sys->get_kernel_system()->crr_process()));

            loader::rom &rom_info = *(sys->get_rom_info());
            info_ptr->iMajor = rom_info.header.major;
            info_ptr->iMinor = rom_info.header.minor;
            info_ptr->iBuild = rom_info.header.build;

            info_ptr->iProessorClockInMhz = sys->get_timing_system()->get_clock_frequency_mhz();
            info_ptr->iMachineUid = 0x70000001;

            return 0;
        }

        variant_hal(eka2l1::system *sys)
            : hal(sys) {
            REGISTER_HAL_FUNC(EVariantHalVariantInfo, variant_hal, get_variant_info);
        }
    };

    struct display_hal : public hal {
        window_server *winserv_;
        
        int current_mode_info(int *a1, int *a2, const std::uint16_t device_num) {
            assert(winserv_);

            epoc::des8 *package = reinterpret_cast<epoc::des8 *>(a1);
            epoc::video_info_v1 *info_ptr = reinterpret_cast<epoc::video_info_v1 *>(
                package->get_pointer(sys->get_kernel_system()->crr_process()));

            epoc::screen *scr = winserv_->get_screen(device_num);

            if (!scr) {
                return epoc::error_not_found;
            }

            info_ptr->size_in_pixels_ = scr->size();
            info_ptr->size_in_twips_ = info_ptr->size_in_pixels_ * 15;
            info_ptr->is_mono_ = is_display_mode_mono(scr->disp_mode);
            info_ptr->bits_per_pixel_ = get_bpp_from_display_mode(scr->disp_mode);
            info_ptr->is_pixel_order_rgb_ = (scr->disp_mode >= epoc::display_mode::color4k);

            // TODO: Verify
            info_ptr->is_pixel_order_landspace_ = (scr->size().x > scr->size().y);
            info_ptr->is_palettelized_ = !info_ptr->is_mono_ && (scr->disp_mode < epoc::display_mode::color4k);
            
            // Intentional
            info_ptr->offset_to_first_pixel_ = 0x10000;
            info_ptr->display_mode_ = static_cast<std::int32_t>(scr->disp_mode);

            LOG_TRACE("{}", info_ptr->video_address_);

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

        explicit display_hal(system *sys)
            : hal(sys)
            , winserv_(nullptr) {
            REGISTER_HAL_FUNC(EDisplayHalCurrentModeInfo, display_hal, current_mode_info);
            REGISTER_HAL_FUNC(EDisplayHalColors, display_hal, color_count);
            
            winserv_ = reinterpret_cast<window_server*>(sys->get_kernel_system()
                ->get_by_name<service::server>(WINDOW_SERVER_NAME));
        }
    };

    void init_hal(eka2l1::system *sys) {
        REGISTER_HAL_D(sys, 0, kern_hal);
        REGISTER_HAL(sys, 1, variant_hal);
        REGISTER_HAL(sys, 4, display_hal);
    }

    int do_hal(eka2l1::system *sys, uint32_t cage, uint32_t func, int *a1, int *a2) {
        hal *hal_com = sys->get_hal(static_cast<std::uint16_t>(cage));

        if (!hal_com) {
            LOG_TRACE("HAL cagetory not found or unimplemented: 0x{:x} (for function: 0x{:x})",
                cage, func);

            return epoc::error_not_found;
        }

        int ret = hal_com->do_hal(func, a1, a2, static_cast<std::uint16_t>(cage >> 16));

        if (ret == -1) {
            LOG_WARN("Unimplemented HAL function, cagetory: 0x{:x}, function: 0x{:x}",
                cage, func);
        }

        return ret;
    }
}
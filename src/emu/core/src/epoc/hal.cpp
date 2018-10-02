/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <core/core.h>
#include <core/epoc/hal.h>

#include <common/algorithm.h>
#include <common/log.h>

#include <common/e32inc.h>
#include <e32err.h>

#include <core/epoc/des.h>

#define REGISTER_HAL_FUNC(op, hal_name, func) \
    funcs.emplace(op, std::bind(&hal_name::func, this, std::placeholders::_1, std::placeholders::_2))

#define REGISTER_HAL(sys, cage, hal_name) \
    sys->add_new_hal(cage, std::make_shared<hal_name>(sys))

namespace eka2l1::epoc {
    hal::hal(eka2l1::system *sys)
        : sys(sys) {}

    int hal::do_hal(uint32_t func, int *a1, int *a2) {
        auto func_pair = funcs.find(func);

        if (func_pair == funcs.end()) {
            return -1;
        }

        return func_pair->second(a1, a2);
    }

    /*! \brief Kernel HAL cagetory. 
     * Contains HAL of drivers, memory, etc...
     */
    struct kern_hal : public eka2l1::epoc::hal {
        /*! \brief Get the size of a page. 
         *
         * \param a1 The pointer to the integer destination, supposed to
         *           contains the page size.
         * \param a2 Unused.
         */
        int page_size(int *a1, int *a2) {
            *a1 = static_cast<int>(sys->get_memory_system()->get_page_size());
            return KErrNone;
        }

        int memory_info(int *a1, int *a2) {
            TDes8 *buf = reinterpret_cast<TDes8 *>(a1);
            TMemoryInfoV1 memInfo;

            memInfo.iTotalRamInBytes = common::MB(256);
            memInfo.iRomIsReprogrammable = false;
            memInfo.iMaxFreeRamInBytes = common::MB(256);
            memInfo.iFreeRamInBytes = common::MB(256);
            memInfo.iTotalRomInBytes = sys->get_rom_info().header.rom_size;
            memInfo.iInternalDiskRamInBytes = memInfo.iTotalRomInBytes; // This value is appr. the same as rom.

            std::string dat;
            dat.resize(sizeof(memInfo));

            memcpy(dat.data(), &memInfo, sizeof(memInfo));

            buf->Assign(sys, dat);

            return KErrNone;
        }

        kern_hal(eka2l1::system *sys)
            : hal(sys) {
            REGISTER_HAL_FUNC(EKernelHalPageSizeInBytes, kern_hal, page_size);
            REGISTER_HAL_FUNC(EKernelHalMemoryInfo, kern_hal, memory_info);
        }
    };

    struct variant_hal : public eka2l1::epoc::hal {
        int get_variant_info(int *a1, int *a2) {
            epoc::TDes8 *package = reinterpret_cast<epoc::TDes8 *>(a1);
            epoc::TVariantInfoV1 *info_ptr = reinterpret_cast<epoc::TVariantInfoV1 *>(package->Ptr(sys));

            loader::rom &rom_info = sys->get_rom_info();
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
        int current_mode_info(int *a1, int *a2) {
            epoc::TDes8 *package = reinterpret_cast<epoc::TDes8 *>(a1);
            epoc::TVideoInfoV01 *info_ptr = reinterpret_cast<epoc::TVideoInfoV01 *>(package->Ptr(sys));

            info_ptr->iSizeInPixels = sys->get_screen_driver()->get_window_size();
            info_ptr->iSizeInTwips = info_ptr->iSizeInPixels * 15;
            info_ptr->iIsMono = false; // This can be changed
            info_ptr->iBitsPerPixel = 3;
            info_ptr->iIsPixelOrderRGB = true;
            info_ptr->iIsPixelOrderLandscape = false; // This can be changed

            return 0;
        }

        int color_count(int *a1, int *a2) {
            *a1 = 1 << 24;

            LOG_WARN("Color count stubbed with 1 << 24 (24 bit).");

            return 0;
        }

        display_hal(system *sys)
            : hal(sys) {
            REGISTER_HAL_FUNC(EDisplayHalCurrentModeInfo, display_hal, current_mode_info);
            REGISTER_HAL_FUNC(EDisplayHalColors, display_hal, color_count);
        }
    };

    void init_hal(eka2l1::system *sys) {
        REGISTER_HAL(sys, 0, kern_hal);
        REGISTER_HAL(sys, 1, variant_hal);
        REGISTER_HAL(sys, 4, display_hal);
    }

    int do_hal(eka2l1::system *sys, uint32_t cage, uint32_t func, int *a1, int *a2) {
        hal_ptr hal_com = sys->get_hal(cage);

        if (!hal_com) {
            LOG_TRACE("HAL cagetory not found or unimplemented: 0x{:x} (for function: 0x{:x})",
                cage, func);

            return KErrNotFound;
        }

        int ret = hal_com->do_hal(func, a1, a2);

        if (ret == -1) {
            LOG_WARN("Unimplemented HAL function, cagetory: 0x{:x}, function: 0x{:x}",
                cage, func);
        }

        return ret;
    }
}
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

#include <common/cvt.h>
#include <common/log.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>
#include <epoc/kernel/codeseg.h>
#include <epoc/kernel/libmanager.h>
#include <epoc/utils/dll.h>

#include <epoc/loader/e32img.h>
#include <epoc/loader/romimage.h>

namespace eka2l1::epoc {
    std::optional<std::u16string> get_dll_full_path(eka2l1::system *sys, const std::uint32_t addr) {
        hle::lib_manager &mngr = *sys->get_lib_manager();
        kernel::process *crr_process = sys->get_kernel_system()->crr_process();

        for (const auto &seg_obj : sys->get_kernel_system()->get_codeseg_list()) {
            codeseg_ptr seg = reinterpret_cast<codeseg_ptr>(seg_obj.get());

            if (seg && seg->get_entry_point(crr_process) == addr) {
                return seg->get_full_path();
            }
        }

        return std::optional<std::u16string>{};
    }

    address get_exception_descriptor_addr(eka2l1::system *sys, address runtime_addr) {
        hle::lib_manager *manager = sys->get_lib_manager();
        kernel::process *crr_process = sys->get_kernel_system()->crr_process();

        for (const auto &seg_obj : sys->get_kernel_system()->get_codeseg_list()) {
            codeseg_ptr seg = reinterpret_cast<codeseg_ptr>(seg_obj.get());
            const address seg_code_addr = seg->get_code_run_addr(crr_process);

            if (seg_code_addr <= runtime_addr && seg_code_addr + seg->get_code_size() >= runtime_addr) {
                return seg->get_exception_descriptor(crr_process);
            }
        }

        return 0;
    }

    bool get_image_info(hle::lib_manager *mngr, const std::u16string &name, epoc::lib_info &linfo) {
        auto imgs = mngr->try_search_and_parse(name);

        LOG_TRACE("Get Info of {}", common::ucs2_to_utf8(name));

        if (!imgs.first && !imgs.second) {
            return false;
        }

        if (!imgs.first && imgs.second) {
            auto &rimg = imgs.second;

            linfo.uid1 = rimg->header.uid1;
            linfo.uid2 = rimg->header.uid2;
            linfo.uid3 = rimg->header.uid3;
            linfo.secure_id = rimg->header.sec_info.secure_id;
            linfo.caps[0] = rimg->header.sec_info.cap1;
            linfo.caps[1] = rimg->header.sec_info.cap2;
            linfo.vendor_id = rimg->header.sec_info.vendor_id;
            linfo.major = rimg->header.major;
            linfo.minor = rimg->header.minor;

            return true;
        }

        auto &eimg = imgs.first;
        memcpy(&linfo.uid1, &eimg->header.uid1, 12);

        linfo.secure_id = eimg->header_extended.info.secure_id;
        linfo.caps[0] = eimg->header_extended.info.cap1;
        linfo.caps[1] = eimg->header_extended.info.cap2;
        linfo.vendor_id = eimg->header_extended.info.vendor_id;
        linfo.major = eimg->header.major;
        linfo.minor = eimg->header.minor;

        return true;
    }
}

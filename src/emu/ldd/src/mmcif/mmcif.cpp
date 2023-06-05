/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <ldd/mmcif/consts.h>
#include <ldd/mmcif/mmcif.h>

#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/thread.h>

#include <common/log.h>

#include <common/pystr.h>
#include <config/config.h>

namespace eka2l1::ldd {
    static void get_cid_from_config(config::state *conf, std::uint32_t *dat) {
        common::pystr strconf(conf->current_mmc_id);
        std::vector<common::pystr> nums = strconf.split('-');

        if (nums.size() != 4) {
            LOG_WARN(LDD_MMCIF, "Config's MMC ID does not have full 4 code numbers!");
        }

        for (std::size_t i = 0; i < nums.size(); i++) {
            const std::uint32_t num = nums[i].as_int<std::uint32_t>(0, 16);
            dat[i] = num;
        }
    }

    mmcif_factory::mmcif_factory(kernel_system *kern, system *sys)
        : factory(kern, sys) {
    }

    void mmcif_factory::install() {
        obj_name = MMCIF_FACTORY_NAME;
    }

    std::unique_ptr<channel> mmcif_factory::make_channel(epoc::version ver) {
        return std::make_unique<mmcif_channel>(kern, sys_, ver);
    }

    mmcif_channel::mmcif_channel(kernel_system *kern, system *sys, epoc::version ver)
        : channel(kern, sys, ver) {
    }

    std::int32_t mmcif_channel::do_control(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
        const eka2l1::ptr<void> arg2) {
        if (kern->is_eka1()) {
            switch (n) {
            case mmcif_control_op_select_card:
                LOG_TRACE(LDD_MMCIF, "Card slot {} selected", arg1.ptr_address());
                break;

            case mmcif_control_op_card_info: {
                mmcif_card_legacy *info = arg1.cast<mmcif_card_legacy>().get(r->owning_process());
                if (info) {
                    info->ctype_ = mmcif_card_type_rom;
                    get_cid_from_config(kern->get_config(), info->cid_);
                }
                break;
            }

            default:
                LOG_TRACE(LDD_MMCIF, "Unimplemented control opcode {}", n);
            }
        }

        return 0;
    }

    std::int32_t mmcif_channel::do_request(epoc::notify_info info, const std::uint32_t n,
        const eka2l1::ptr<void> arg1, const eka2l1::ptr<void> arg2,
        const bool is_supervisor) {
        LOG_TRACE(LDD_MMCIF, "Unimplemented request opcode {}", n);
        return 0;
    }
}
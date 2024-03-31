/*
* Copyright (c) 2024 EKA2L1 Team.
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

#include <ldd/oldcamera/consts.h>
#include <ldd/oldcamera/oldcamera.h>

#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/thread.h>

#include <common/log.h>
#include <config/config.h>
#include <utils/err.h>

namespace eka2l1::ldd {
   old_camera_factory::old_camera_factory(kernel_system *kern, system *sys)
       : factory(kern, sys) {
   }

   void old_camera_factory::install() {
       obj_name = OLD_CAMERA_FACTORY_NAME;
   }

   std::unique_ptr<channel> old_camera_factory::make_channel(epoc::version ver) {
       return std::make_unique<old_camera_channel>(kern, sys_, ver);
   }

   old_camera_channel::old_camera_channel(kernel_system *kern, system *sys, epoc::version ver)
       : channel(kern, sys, ver) {
   }

   std::int32_t old_camera_channel::do_control(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
       const eka2l1::ptr<void> arg2) {
       if (kern->is_eka1()) {
           switch (n) {
           default:
               LOG_TRACE(LDD_MMCIF, "Unimplemented old camera control opcode {}", n);
           }
       }

       return 0;
   }

   std::int32_t old_camera_channel::do_request(epoc::notify_info info, const std::uint32_t n,
       const eka2l1::ptr<void> arg1, const eka2l1::ptr<void> arg2,
       const bool is_supervisor) {
       LOG_TRACE(LDD_MMCIF, "Unimplemented old camera request opcode {}", n);
       info.complete(epoc::error_none);

       return 0;
   }
}
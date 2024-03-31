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

#include <ldd/collection.h>
#include <ldd/ecomm/ecomm.h>
#include <ldd/ekeyb/ekeyb.h>
#include <ldd/hal/hal.h>
#include <ldd/mmcif/mmcif.h>
#include <ldd/oldcamera/oldcamera.h>
#include <ldd/videodriver/videodriver.h>

#include <system/epoc.h>

#include <functional>
#include <string>
#include <unordered_map>

namespace eka2l1::ldd {
#define FACTORY_DECLARE(type)                                       \
    std::unique_ptr<factory> factory_create_##type(system *ss) {    \
        return std::make_unique<type>(ss->get_kernel_system(), ss); \
    }

#define FACTORY_REGISTER(lddname, type) \
    { lddname, factory_create_##type }

    FACTORY_DECLARE(mmcif_factory)
    FACTORY_DECLARE(ecomm_factory)
    FACTORY_DECLARE(hal_factory)
    FACTORY_DECLARE(video_driver_factory)
    FACTORY_DECLARE(ekeyb_factory)
    FACTORY_DECLARE(old_camera_factory)

    static std::unordered_map<std::string, factory_instantiate_func> insts_map = {
        FACTORY_REGISTER("gd1drv", mmcif_factory),
        FACTORY_REGISTER("ecomm", ecomm_factory),
        FACTORY_REGISTER("dhal", hal_factory),
        FACTORY_REGISTER("videodriver", video_driver_factory),
        FACTORY_REGISTER("ekeyb", ekeyb_factory),
        FACTORY_REGISTER("cameraldd", old_camera_factory),
    };

    factory_instantiate_func get_factory_func(const char *name) {
        auto res = insts_map.find(name);
        if (res != insts_map.end()) {
            return res->second;
        }

        return nullptr;
    }
}
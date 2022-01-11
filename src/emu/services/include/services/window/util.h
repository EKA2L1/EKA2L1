/*
 * Copyright (c) 2021 EKA2L1 Team
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

#include <services/window/opheader.h>
#include <services/context.h>
#include <common/region.h>

#include <optional>

namespace eka2l1 {
    namespace drivers {
        class graphics_command_builder;
    }

    std::optional<common::region> get_region_from_context(service::ipc_context &ctx, ws_cmd &cmd);
    void clip_region(drivers::graphics_command_builder &builder, const common::region &region,
        const bool stencil_one_for_valid = true);
}
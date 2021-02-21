/*
 * Copyright (c) 2020 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
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

#include <services/utils.h>
#include <services/context.h>

#include <kernel/kernel.h>

namespace eka2l1::service {
    std::optional<epoc::version> get_server_version(kernel_system *kern, ipc_context *ctx) {
        epoc::version ver;

        if (kern->is_ipc_old()) {
            std::optional<std::uint32_t> ver_package = ctx->get_argument_data_from_descriptor<std::uint32_t>(1);
            if (!ver_package) {
                return std::nullopt;
            }

            ver.u32 = ver_package.value();
        } else {
            ver.u32 = ctx->get_argument_value<std::uint32_t>(0).value();
        }

        return ver;
    }
}
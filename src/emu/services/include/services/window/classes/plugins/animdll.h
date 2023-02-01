/*
 * Copyright (c) 2019 EKA2L1 Team
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

#pragma once

#include <cstdint>
#include <services/window/classes/wsobj.h>
#include <common/container.h>

namespace eka2l1::epoc {
    struct screen;
    struct canvas_base;

    struct anim_create_instance_args {
        std::uint32_t win_handle_;
        std::uint32_t anim_type_;
    };

    struct anim_request_info {
        std::uint32_t handle_;
        std::uint32_t opcode_;
    };

    struct anim_executor {
        canvas_base *canvas_;

    public:
        explicit anim_executor(canvas_base *canvas)
            : canvas_(canvas) {
        }

        virtual std::int32_t handle_request(const std::int32_t opcode, void *args) = 0;
        virtual ~anim_executor() = default;
    };

    struct anim_executor_factory {
    public:
        virtual std::unique_ptr<anim_executor> new_executor(canvas_base *canvas, const std::uint32_t anim_type) = 0;
        virtual std::string name() const = 0;
        virtual ~anim_executor_factory() = default;
    };

    struct anim_dll : public window_client_obj {
    protected:
        anim_executor_factory *factory_;
        common::identity_container<std::unique_ptr<anim_executor>> executors_;

    public:
        explicit anim_dll(window_server_client_ptr client, screen *scr, anim_executor_factory *factory);
        bool execute_command(service::ipc_context &context, ws_cmd &cmd) override;
    };
}
/*
 * Copyright (c) 2023 EKA2L1 Team
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

#pragma once

#include <services/window/classes/plugins/animdll.h>

namespace eka2l1::epoc {
    struct clock_anim_executor_factory : public anim_executor_factory {
    public:
        std::unique_ptr<anim_executor> new_executor(canvas_base *canvas, const std::uint32_t anim_type) override;

        std::string name() const override {
            return "Clock animation factory";
        }

        ~clock_anim_executor_factory() override = default;
    };
}
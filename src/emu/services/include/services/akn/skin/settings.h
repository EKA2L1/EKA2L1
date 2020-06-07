/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <cstdint>
#include <services/akn/skin/common.h>

namespace eka2l1 {
    struct central_repo;

    class central_repo_server;
    class io_system;
}

namespace eka2l1::epoc {
    class akn_ss_settings {
        central_repo *avkon_rep_{ nullptr };
        central_repo *skins_rep_{ nullptr };
        central_repo *theme_rep_{ nullptr };

        io_system *io_;

        pid default_skin_pid_{ 0, 0 };
        pid active_skin_pid_{ 0, 0 };
        bool ah_mirroring_active; ///< Arabic/hebrew mirroring active
        bool highlight_anim_enabled;

    protected:
        bool read_default_skin_id();
        bool read_ah_mirroring_active();
        bool read_active_skin_id();
        bool read_highlight_anim_enabled();

    public:
        explicit akn_ss_settings(io_system *io, central_repo_server *svr);

        pid active_skin_pid() const {
            return active_skin_pid_;
        }

        pid default_skin_pid() const {
            return default_skin_pid_;
        }
    };
}

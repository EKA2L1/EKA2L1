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

#include <services/socket/connection.h>

namespace eka2l1::epoc::socket {
    class generic_connect_agent: public connect_agent {
    public:
        explicit generic_connect_agent(socket_server *ss)
            : connect_agent(ss) {
        }

        std::u16string agent_name() const override {
            return u"GenConn";
        }

        std::unique_ptr<connection> start_connection(conn_preferences &prefs) override;
    };
}
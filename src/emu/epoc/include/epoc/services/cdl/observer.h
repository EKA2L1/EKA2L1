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

#include <epoc/services/cdl/watcher.h>
#include <epoc/services/cdl/common.h>

#pragma once

namespace eka2l1 {
    class cdl_server;
    class io_system;
}

namespace eka2l1::epoc {
    class cdl_ecom_generic_observer: public cdl_ecom_watcher_observer {
        cdl_server *serv_;

    public:
        bool read_refs_of_instance(io_system *io, const std::u16string &path
            , cdl_ref_collection &collection_);

        explicit cdl_ecom_generic_observer(cdl_server *serv);

        void entry_added(const std::u16string &plugin_path) override;
        void entry_removed(const std::u16string &plugin_path) override;
    };
}

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

#include <epoc/services/ecom/plugin.h>

#include <string>
#include <vector>

namespace eka2l1 {
    class ecom_server;
    struct ecom_implementation_info;
}

namespace eka2l1::epoc {
    class cdl_ecom_watcher_observer {
    public:
        virtual void entry_added(const std::u16string &plugin_path) = 0;
        virtual void entry_removed(const std::u16string &plugin_path) = 0;
    };

    class cdl_ecom_watcher {
        ecom_server *ecom_;
        cdl_ecom_watcher_observer *observer_;

        std::vector<ecom_implementation_info> *curr;
        std::vector<ecom_implementation_info>  last;

    protected:
        void refresh_plugin_list();

    public:
        explicit cdl_ecom_watcher(ecom_server *ecom, cdl_ecom_watcher_observer *observer)
            : ecom_(ecom), observer_(observer) {
            // TODO: Put notification
            refresh_plugin_list();
        }
    };
}

/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <common/buffer.h>
#include <utils/des.h>
#include <cstdint>

namespace eka2l1 {
    enum data_type_priority {
        data_type_priority_last_resort = -20000,
        data_type_priority_low = -10000,
        data_type_priority_normal = 0,
        data_type_priority_high = 10000
    };
    
    struct data_type {
        std::int32_t priority_;
        std::string type_;
    };

    struct view_data {
        std::uint32_t uid_;
        std::int32_t screen_mode_;
        std::uint32_t icon_count_;
        std::u16string caption_;
    };

    using file_ownership_list = std::vector<std::u16string>;

    /**
     * \brief Struct describles the high-level usage of a application for the
     *        APA framework.
     */
    struct apa_capability {
        /**
         * \brief The embeddability of an app.
         *
         * This was used during EKA1.
        */
        enum class embeddability {
            not_embeddable,
            embeddable,
            embeddable_only,
            embeddable_ui_or_standalone = 5,
            embeddable_ui_not_standalone
        };

        /**
         * \brief The attribute of the capability.
         */
        enum capability_attrib {
            built_as_dll = 1,
            control_panel_item = 2,
            non_native = 4
        };

        embeddability ability;

        // If the app supports create new file if being asked (for example, document application
        // creates new .doc file), this will be checked.
        std::int32_t support_being_asked_to_create_new_file;

        // If the application is not visible, this will be marked.
        std::int32_t is_hidden;
        std::int32_t launch_in_background;

        // If you have use Symbian 9.x before, you can put apps and games into folders
        // The name of the folder is the group name.
        epoc::buf_static<char16_t, 0x10> group_name;
        std::uint32_t flags{ 0 };

        int reserved;

        explicit apa_capability() = default;
        bool internalize(common::ro_stream &stream);
    };

    static_assert(sizeof(apa_capability) == 64);
}
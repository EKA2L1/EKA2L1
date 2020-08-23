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

#include <utils/apacmd.h>
#include <utils/des.h>

namespace eka2l1::epoc::apa {
    command_line::command_line()
        : launch_cmd_(command_run)
        , server_differentiator_(0)
        , default_screen_number_(-1)
        , parent_window_group_id_(0)
        , debug_mem_fail_(0)
        , app_startup_instrumentation_event_id_base_(0)
        , parent_process_id_(0) {
    }

    void command_line::do_it_newarch(common::chunkyseri &seri) {
        epoc::absorb_des_string(executable_path_, seri, true);
        epoc::absorb_des_string(document_name_, seri, true);
        epoc::absorb_des_string(opaque_data_, seri, false);
        epoc::absorb_des_string(tail_end_, seri, false);

        seri.absorb(launch_cmd_);
        seri.absorb(server_differentiator_);
        seri.absorb(default_screen_number_);
        seri.absorb(parent_window_group_id_);
        seri.absorb(debug_mem_fail_);
        seri.absorb(app_startup_instrumentation_event_id_base_);
        seri.absorb(parent_process_id_);
    }

    static char16_t get_char_correspond_to_command(const command cmd) {
        switch (cmd) {
        case command_open:
            return 'O';

        case command_create:
            return 'C';

        case command_run:
            return 'R';

        case command_background:
            return 'B';

        case command_view_activate:
            return 'V';

        case command_run_without_views:
            return 'W';

        default:
            break;
        }

        return 'R';
    }

    std::u16string command_line::to_string(const bool oldarch) {
        if (oldarch) {
            // From RE. The order: .app path -> command (O, C, R, B, V, W) -> document name -> tail end
            // .app path and document name are in quote (not neccessary if they don't have spaces in their string).
            // Each components are separated by space.
            std::u16string result;

            // Add executable path
            result += '"';
            result += executable_path_;
            result += u"\" ";

            // Add command
            result += get_char_correspond_to_command(launch_cmd_);

            // Add document name
            result += '"';
            result += document_name_;
            result += u"\" ";

            // Add tail end
            result.append(reinterpret_cast<char16_t*>(tail_end_.data()), (tail_end_.length() + 1) >> 1);
            
            // Other parameters are unused.
            return result;
        }

        common::chunkyseri seri(nullptr, 0, common::chunkyseri_mode::SERI_MODE_MEASURE);
        do_it_newarch(seri);

        std::u16string data;
        data.resize((seri.size() + 1) >> 1);

        seri = common::chunkyseri(reinterpret_cast<std::uint8_t*>(data.data()), data.length() * 2,
            common::chunkyseri_mode::SERI_MODE_WRITE);

        do_it_newarch(seri);

        return data;
    }
}
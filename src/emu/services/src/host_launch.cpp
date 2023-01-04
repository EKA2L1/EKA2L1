
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

#include <services/applist/applist.h>
#include <services/internet/browser.h>
#include <services/host_launch.h>

#include <kernel/kernel.h>
#include <system/epoc.h>

#include <common/applauncher.h>
#include <common/log.h>
#include <utils/apacmd.h>

namespace eka2l1::service {
    static constexpr epoc::uid BROWSER_APP_UID = 0x10008D39;
    static const std::u16string BROWSER_APP_HOST_NAME_MAP = u"browser";

    bool handle_launch_browser(kernel::process *pr, const epoc::apa::command_line &cmd_line) {
        kernel_system *kern = pr->get_kernel_object_owner();
        
        if (cmd_line.server_differentiator_ != 0) {
            kern->create_no_kernel_param_and_add_thread<browser_for_app_server>(kernel::owner_type::process, pr->get_primary_thread(),
                kern->get_system(), cmd_line.server_differentiator_);
        }
  
        if (!cmd_line.document_name_.empty()) {
            // Launch right away, else it will probably send a command to the server to launch later.
            return common::launch_browser(common::ucs2_to_utf8(cmd_line.document_name_));
        }

        return true;
    }

    using handle_host_launch_callback = std::function<bool(kernel::process*, const epoc::apa::command_line &)>;
    static const std::map<std::u16string, handle_host_launch_callback> HANDLE_HOST_LAUNCH_CALLBACKS_LOOKUP = {
        { BROWSER_APP_HOST_NAME_MAP, handle_launch_browser }
    };

    void init_symbian_app_launch_to_host_launch(system *sys) {
        kernel_system *kern = sys->get_kernel_system();
        
        applist_server *serv = kern->get_by_name<applist_server>(
            get_app_list_server_name_by_epocver(kern->get_epoc_version()));

        if (!serv) {
            LOG_ERROR(SERVICE_TRACK, "Unable to initialize Symbian app launch to host app launch: Applist server is not available!");
            return;
        }

        serv->add_app_uid_to_host_launch_name(BROWSER_APP_UID, BROWSER_APP_HOST_NAME_MAP);
        kern->register_guomen_process_run_callback([](kernel::process *pr) {
            std::optional<kernel::pass_arg> launch_arg_raw = pr->get_arg_slot(epoc::apa::PROCESS_ENVIRONMENT_ARG_SLOT_MAIN);
            if (!launch_arg_raw.has_value()) {
                LOG_ERROR(SERVICE_APPLIST, "Unable to perform launching host app instead of Symbian app: environment variable argument not available!");
                return false;
            }

            common::chunkyseri seri(launch_arg_raw->data.data(), launch_arg_raw->data.size(),
                common::SERI_MODE_READ);

            epoc::apa::command_line launch_cmdline;
            launch_cmdline.do_it_newarch(seri);

            std::u16string run_command = launch_cmdline.executable_path_;

            if (run_command.find(MAPPED_EXECUTABLE_HEAD_STRING) == 0) {
                run_command = run_command.substr(MAPPED_EXECUTABLE_HEAD_STRING_LENGTH + 1,
                    run_command.length() - MAPPED_EXECUTABLE_HEAD_STRING_LENGTH - 1 - UNIQUE_MAPPED_EXTENSION_STRING_LENGTH);
            }

            auto launch_callback_ite = HANDLE_HOST_LAUNCH_CALLBACKS_LOOKUP.find(run_command);
            if (launch_callback_ite == HANDLE_HOST_LAUNCH_CALLBACKS_LOOKUP.end()) {
                LOG_ERROR(SERVICE_APPLIST, "No host launcher correspond for launch command: {}", common::ucs2_to_utf8(run_command));
                return false;
            }

            return launch_callback_ite->second(pr, launch_cmdline);
        });
    }
}
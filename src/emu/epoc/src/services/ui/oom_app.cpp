/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <epoc/services/ui/oom_app.h>
#include <common/log.h>
#include <common/cvt.h>

#include <common/e32inc.h>
#include <e32err.h>

#include <epoc/epoc.h>
#include <epoc/vfs.h>

namespace eka2l1 {
    oom_ui_app_server::oom_ui_app_server(eka2l1::system *sys) 
        : service::server(sys, "101fdfae_10207218_AppServer", true) {
        REGISTER_IPC(oom_ui_app_server, get_layout_config_size, EAknEikAppUiLayoutConfigSize, "OOM::GetLayoutConfigSize");
        REGISTER_IPC(oom_ui_app_server, get_layout_config, EAknEikAppUiGetLayoutConfig, "OOM::GetLayoutConfig");
        REGISTER_IPC(oom_ui_app_server, set_sgc_params, EAknEikAppUiSetSgcParams, "OOM::SetSgcParams");
    }

    std::string oom_ui_app_server::get_layout_buf() {
        akn_layout_config akn_config;
        
        akn_config.num_screen_mode = static_cast<int>(scr_config.modes.size());
        // TODO: Find out what this really does
        akn_config.num_hardware_mode = 0;

        // Static check on those pointer
        akn_config.screen_modes = sizeof(akn_layout_config);
        akn_config.hardware_infos = sizeof(akn_layout_config) + 
            sizeof(akn_screen_mode_info) * akn_config.num_screen_mode;

        std::string result;
        result.append(reinterpret_cast<char*>(&akn_config), sizeof(akn_layout_config));

        for (std::size_t i = 0 ; i < scr_config.modes.size(); i++) {
            akn_screen_mode_info mode_info;

            // TODO: Change this based on user settings
            mode_info.loc = akn_softkey_loc::bottom;
            mode_info.mode_num = scr_config.modes[i].mode_number;
            mode_info.dmode = epoc::display_mode::color16ma;
            mode_info.info.orientation = epoc::number_to_orientation(scr_config.modes[i].rotation);
            mode_info.info.pixel_size = scr_config.modes[i].size;
            mode_info.info.twips_size = mode_info.info.pixel_size * twips_mul;

            result.append(reinterpret_cast<char*>(&mode_info), sizeof(akn_screen_mode_info));
        }

        // TODO: Hardware mode

        return result;
    }
    
    void oom_ui_app_server::load_screen_mode() {
        io_system *io = sys->get_io_system();
        std::optional<eka2l1::drive> drv;
        drive_number dn = drive_z;

        for (; dn >= drive_a; dn = (drive_number)((int)dn - 1)) {
            drv = io->get_drive_entry(dn);

            if (drv && drv->media_type == drive_media::rom) {
                break;
            }
        }

        std::u16string wsini_path;
        wsini_path += static_cast<char16_t>((char)dn + 'A');
        wsini_path += u":\\system\\data\\wsini.ini";

        auto wsini_path_host = io->get_raw_path(wsini_path);

        if (!wsini_path_host) {
            LOG_ERROR("Can't find the window config file, app using window server will broken");
            return;
        }

        std::string wsini_path_host_utf8 = common::ucs2_to_utf8(*wsini_path_host);
        common::ini_file ws_config;

        int err = ws_config.load(wsini_path_host_utf8.c_str());

        if (err != 0) {
            LOG_ERROR("Loading wsini file broke with code {}", err);
        }
        
        std::string screen_key = "SCREEN0";
        auto screen_node = ws_config.find(screen_key.c_str());

        if (!screen_node || !common::ini_section::is_my_type(screen_node)) {
            LOG_ERROR("Loading wsini failed");
            return;
        }

        scr_config.screen_number = 0;
        int total_mode = 0;

        do {
            std::string screen_mode_width_key = "SCR_WIDTH";
            screen_mode_width_key += std::to_string(total_mode + 1);

            common::ini_node_ptr mode_width = screen_node->
                get_as<common::ini_section>()->find(screen_mode_width_key.c_str());
            
            if (!mode_width) {
                break;
            }

            std::string screen_mode_height_key = "SCR_HEIGHT";
            screen_mode_height_key += std::to_string(total_mode + 1);

            common::ini_node_ptr mode_height = screen_node->
                get_as<common::ini_section>()->find(screen_mode_height_key.c_str());

            total_mode++;

            epoc::config::screen_mode   scr_mode;
            scr_mode.screen_number = 0;
            scr_mode.mode_number = total_mode;

            mode_width->get_as<common::ini_pair>()->get(reinterpret_cast<std::uint32_t*>(&scr_mode.size.x),
                1, 0);
            mode_height->get_as<common::ini_pair>()->get(reinterpret_cast<std::uint32_t*>(&scr_mode.size.y),
                1, 0);
            
            std::string screen_mode_rot_key = "SCR_ROTATION";
            screen_mode_rot_key += std::to_string(total_mode);

            common::ini_node_ptr mode_rot = screen_node->
                get_as<common::ini_section>()->find(screen_mode_rot_key.c_str());

            mode_rot->get_as<common::ini_pair>()->get(reinterpret_cast<std::uint32_t*>(&scr_mode.rotation),
                1, 0);

            scr_config.modes.push_back(scr_mode);
        } while (true);

        layout_buf = get_layout_buf();
        layout_config_loaded = true;
    }

    void oom_ui_app_server::get_layout_config_size(service::ipc_context ctx) {
        if (!layout_config_loaded) {
            load_screen_mode();
        }

        ctx.write_arg_pkg<int>(0, static_cast<int>(layout_buf.size()));
        ctx.set_request_status(KErrNone);
    }

    void oom_ui_app_server::get_layout_config(service::ipc_context ctx) {
        if (!layout_config_loaded) {
            load_screen_mode();
        }

        ctx.write_arg_pkg(0, reinterpret_cast<std::uint8_t*>(&layout_buf[0]), 
            static_cast<std::uint32_t>(layout_buf.size()));

        ctx.set_request_status(KErrNone);
    }
    
    void oom_ui_app_server::set_sgc_params(service::ipc_context ctx) {
        auto params_op = ctx.get_arg_packed<epoc::sgc_params>(0);

        if (!params_op) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        params = std::move(*params_op);
        LOG_TRACE("OOM UI app server: set sgc params stubbed, returns KErrNone");

        ctx.set_request_status(KErrNone);
    }
}
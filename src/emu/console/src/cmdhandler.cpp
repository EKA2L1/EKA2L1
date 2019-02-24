/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <common/arghandler.h>
#include <common/cvt.h>
#include <console/cmdhandler.h>
#include <console/global.h>
#include <manager/device_manager.h>
#include <manager/manager.h>
#include <manager/package_manager.h>

using namespace eka2l1;

bool app_install_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    const char *path = parser->next_token();

    if (!path) {
        *err = "Request to install a SIS, but path not given";
        return false;
    }

    // Since it's inconvinient for user to specify the drive (they are all the same on computer),
    // and it's better to install in C since there is many apps required
    // to be in it and hardcoded the drive, just hardcode drive C here.
    bool result = symsys->install_package(common::utf8_to_ucs2(path), drive_c);

    if (!result) {
        *err = "Installation of SIS failed";
        return false;
    }

    return true;
}

bool app_specifier_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    const char *tok = parser->next_token();

    if (!tok) {
        *err = "No application specified";
        return false;
    }

    auto infos = symsys->get_manager_system()->get_package_manager()->get_apps_info();
    std::uint8_t app_idx = 0;

    try {
        app_idx = static_cast<decltype(app_idx)>(std::atoi(tok));
    } catch (...) {
        *err = "Expect an app index after --app, got ";
        *err += tok;

        return false;
    }

    bool result = symsys->load(infos[app_idx].id);

    if (!result) {
        *err = "Application load failed";
        return false;
    }

    return true;
}

bool help_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    std::cout << parser->get_help_string();
    return false;
}

bool rpkg_unpack_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    // Base Z drive is mount_z
    // The RPKG path is the next token
    const char *path = parser->next_token();

    if (!path) {
        *err = "RPKG installation failed. No path provided";
        return false;
    }

    bool install_result = symsys->install_rpkg(mount_z, path);
    if (!install_result) {
        *err = "RPKG installation failed. Something is wrong, see log";
        return false;
    }

    return false;
}

bool list_app_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    auto infos = symsys->get_manager_system()->get_package_manager()->get_apps_info();

    for (auto &info : infos) {
        std::cout << "[0x" << common::to_string(info.id, std::hex) << "]: "
                  << common::ucs2_to_utf8(info.name) << " (drive: " << static_cast<char>('A' + info.drive)
                  << " , executable name: " << common::ucs2_to_utf8(info.executable_name) << ")" << std::endl;
    }

    return false;
}

bool list_devices_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    auto &devices_list = symsys->get_manager_system()->get_device_manager()->get_devices();

    for (std::size_t i = 0; i < devices_list.size(); i++) {
        std::cout << i << " : " << devices_list[i].model << " (" << devices_list[i].firmware_code << ", "
                  << devices_list[i].model << ", epocver:";

        switch (devices_list[i].ver) {
        case epocver::epocu6: {
            std::cout << " under 6.0";
            break;
        }

        case epocver::epoc6: {
            std::cout << " 6.0";
            break;
        }

        case epocver::epoc93: {
            std::cout << " 9.3";
            break;
        }

        case epocver::epoc94: {
            std::cout << " 9.4";
            break;
        }

        case epocver::epoc10: {
            std::cout << " 10.0";
            break;
        }

        default: {
            std::cout << " Unknown";
            break;
        }
        }

        std::cout << ")" << std::endl;
    }

    return false;
}

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
#include <common/pystr.h>
#include <common/path.h>
#include <console/cmdhandler.h>
#include <console/global.h>
#include <manager/device_manager.h>
#include <manager/manager.h>
#include <manager/package_manager.h>

#if ENABLE_SCRIPTING
#include <manager/script_manager.h>
#include <pybind11/embed.h>
#endif

#include <epoc/services/applist/applist.h>
#include <epoc/kernel.h>

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

bool package_remove_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    const char *uid = parser->next_token();

    if (!uid) {
        *err = "Request to remove a package, but UID not given";
        return false;
    }

    std::uint32_t vuid = common::pystr(uid).as_int<std::uint32_t>();
    bool result = symsys->get_manager_system()->get_package_manager()->uninstall_package(vuid);

    if (!result) {
        *err = "Fail to remove package.";
        return false;
    }

    return true;
}

extern void init_stage2();

bool app_specifier_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    const char *tok = parser->next_token();

    if (!tok) {
        *err = "No application specified";
        return false;
    }

    std::string tokstr = tok;

    const char *cmdline = parser->peek_token();

    std::string cmdlinestr = "";

    if (cmdline) {
        cmdlinestr = cmdline;

        if (cmdlinestr.substr(0, 2) == "--") {
            cmdlinestr = "";
        }
    }

    init_stage2();

    // Get app list server
    std::shared_ptr<eka2l1::applist_server> svr = 
        std::reinterpret_pointer_cast<eka2l1::applist_server>(symsys->get_kernel_system()
        ->get_by_name<service::server>("!AppListServer"));

    if (!svr) {
        *err = "Can't get app list server!\n";
        return false;
    }

    // It's an UID if it's starting with 0x
    if (tokstr.length() > 2 && tokstr.substr(0, 2) == "0x") {
        const std::uint32_t uid = common::pystr(tokstr).as_int<std::uint32_t>();
        eka2l1::apa_app_registry *registry = svr->get_registeration(uid);

        if (registry) {
            symsys->load(registry->mandatory_info.app_path.to_std_string(nullptr), common::utf8_to_ucs2(cmdlinestr));
            return true;
        }

        // Load
        *err = "App with UID: ";
        *err += tokstr;
        *err += " doesn't exist";
    } else {
        if (eka2l1::has_root_dir(tokstr)) {
            if (!symsys->load(common::utf8_to_ucs2(tokstr), common::utf8_to_ucs2(cmdlinestr))) {
                *err = "Executable doesn't exists";
                return false;
            }

            return true;
        }

        // Load with name
        std::map<std::uint32_t, apa_app_registry> &regs = svr->get_registerations();

        for (auto &reg: regs) {
            if (common::ucs2_to_utf8(reg.second.mandatory_info.long_caption.to_std_string(nullptr))
                == tokstr) {
                // Load the app
                symsys->load(reg.second.mandatory_info.app_path.to_std_string(nullptr), common::utf8_to_ucs2(cmdlinestr));
                return true;
            }
        }

        *err = "No app name found with the name: {}";
        *err += tokstr;
        *err += ". Make sure the name is right and try again.";
    }

    return false;
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

    bool install_result = symsys->install_rpkg(conf.z_mount, path);
    if (!install_result) {
        *err = "RPKG installation failed. Something is wrong, see log";
        return false;
    }

    return false;
}

bool list_app_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    init_stage2();

    // Get app list server
    std::shared_ptr<eka2l1::applist_server> svr = 
        std::reinterpret_pointer_cast<eka2l1::applist_server>(symsys->get_kernel_system()
        ->get_by_name<service::server>("!AppListServer"));

    if (!svr) {
        *err = "Can't get app list server!\n";
        return false;
    }

    [[maybe_unused]] const auto &regs = svr->get_registerations();
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

#if ENABLE_SCRIPTING
bool python_docgen_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    try {
        pybind11::exec(
            "import os\n"
            "import sys\n"
            "if not hasattr(sys, 'argv'):\n"
            "   sys.argv = ['temp', '-b', 'html', 'scripts/source/', 'scripts/build/']\n"
            "import sphinx.cmd.build\n"
            "sphinx.cmd.build.build_main(['-b', 'html', 'scripts/source/', 'scripts/build/'])"
        );
    } catch (pybind11::error_already_set &exec) {
        std::cout << std::string("Generate documents failed with error: ") + exec.what() << std::endl;
    }

    return false;
}
#endif

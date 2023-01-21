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
#include <common/path.h>
#include <common/pystr.h>
#include <qt/cmdhandler.h>
#include <qt/state.h>
#include <system/devices.h>

#include <package/manager.h>

#if ENABLE_SCRIPTING
#include <scripting/manager.h>

#if ENABLE_PYTHON_SCRIPTING
#include <pybind11/embed.h>
#endif
#endif

#include <kernel/kernel.h>
#include <services/applist/applist.h>

#include <utils/apacmd.h>
#include <vfs/vfs.h>

#include <iostream>

using namespace eka2l1;

bool app_install_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err) {
    const char *path = parser->next_token();

    if (!path) {
        *err = "Request to install a SIS, but path not given";
        return false;
    }

    desktop::emulator *emu = reinterpret_cast<desktop::emulator *>(userdata);

    // Since it's inconvenient for user to specify the drive (they are all the same on computer),
    // and it's better to install in C since there is many apps required
    // to be in it and hardcoded the drive, just hardcode drive E here.
    bool result = emu->symsys->install_package(common::utf8_to_ucs2(path), drive_e);

    if (!result) {
        *err = "Installation of SIS failed";
        return false;
    }

    return true;
}

bool package_remove_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err) {
    const char *uid = parser->next_token();

    if (!uid) {
        *err = "Request to remove a package, but UID not given";
        return false;
    }

    desktop::emulator *emu = reinterpret_cast<desktop::emulator *>(userdata);
    manager::packages *pkgmngr = emu->symsys->get_packages();
    std::uint32_t vuid = common::pystr(uid).as_int<std::uint32_t>();

    package::object *oobj = pkgmngr->package(vuid);
    bool result = true;

    if (oobj) {
        result = pkgmngr->uninstall_package(*oobj);
    } else {
        result = false;
    }

    if (!result) {
        *err = "Failed to remove package.";
        return false;
    }

    return true;
}

bool app_specifier_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err) {
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
        } else {
            parser->next_token();
        }
    }

    desktop::emulator *emu = reinterpret_cast<desktop::emulator *>(userdata);

    // Get app list server
    kernel_system *kern = emu->symsys->get_kernel_system();
    eka2l1::applist_server *svr = nullptr;

    if (kern) {
        svr = reinterpret_cast<eka2l1::applist_server *>(kern->get_by_name<service::server>(
            get_app_list_server_name_by_epocver(kern->get_epoc_version())));
    }

    if (!svr) {
        *err = "Can't get app list server!\n";
        return false;
    }

    // It's an UID if it's starting with 0x
    if (tokstr.length() > 2 && tokstr.substr(0, 2) == "0x") {
        const std::uint32_t uid = common::pystr(tokstr).as_int<std::uint32_t>();
        eka2l1::apa_app_registry *registry = svr->get_registration(uid);

        if (registry) {
            // Load the app
            epoc::apa::command_line cmdline;
            cmdline.launch_cmd_ = epoc::apa::command_create;

            svr->launch_app(*registry, cmdline, nullptr);
            emu->init_app_launched = true;

            return true;
        }

        // Load
        *err = "App with UID: ";
        *err += tokstr;
        *err += " doesn't exist";
    } else {
        if (eka2l1::has_root_dir(tokstr)) {
            process_ptr pr = kern->spawn_new_process(common::utf8_to_ucs2(tokstr), common::utf8_to_ucs2(cmdlinestr));

            if (!pr) {
                LOG_ERROR(FRONTEND_CMDLINE, "Unable to launch process: {}", tokstr);
            } else {
                pr->run();
                emu->init_app_launched = true;
            }

            return true;
        }

        // Load with name
        std::vector<apa_app_registry> &regs = svr->get_registerations();

        for (auto &reg : regs) {
            if (common::ucs2_to_utf8(reg.mandatory_info.long_caption.to_std_string(nullptr))
                == tokstr) {
                // Load the app
                epoc::apa::command_line cmdline;
                cmdline.launch_cmd_ = epoc::apa::command_create;

                svr->launch_app(reg, cmdline, nullptr);
                emu->init_app_launched = true;

                return true;
            }
        }

        *err = "No app name found with the name: {}";
        *err += tokstr;
        *err += ". Make sure the name is right and try again.";
    }

    return false;
}

bool help_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err) {
    std::cout << parser->get_help_string();
    return false;
}

bool list_app_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err) {
    desktop::emulator *emu = reinterpret_cast<desktop::emulator *>(userdata);
    kernel_system *kern = emu->symsys->get_kernel_system();

    // Get app list server
    eka2l1::applist_server *svr = reinterpret_cast<eka2l1::applist_server *>(kern->get_by_name<service::server>(
        get_app_list_server_name_by_epocver(kern->get_epoc_version())));

    if (!svr) {
        *err = "Can't get app list server!\n";
        return false;
    }

    [[maybe_unused]] const auto &regs = svr->get_registerations();
    return false;
}

bool list_devices_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err) {
    desktop::emulator *emu = reinterpret_cast<desktop::emulator *>(userdata);
    auto &devices_list = emu->symsys->get_device_manager()->get_devices();

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

        case epocver::epoc93fp1: {
            std::cout << " 9.3 FP1";
            break;
        }

        case epocver::epoc93fp2: {
            std::cout << " 9.3 FP2";
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

bool fullscreen_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err) {
    desktop::emulator *emu = reinterpret_cast<desktop::emulator *>(userdata);

    emu->init_fullscreen = true;
    *err = "";

    return true;
}

bool mount_card_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err) {
    desktop::emulator *emu = reinterpret_cast<desktop::emulator *>(userdata);
    const char *path = parser->next_token();

    if (!path) {
        *err = "No folder specified";
        return true;
    }

    bool writable_sd = false;

    if (eka2l1::common::compare_ignore_case("writeable", path) == 0) {
        writable_sd = true;
        path = parser->next_token();

        if (!path) {
            *err = "No folder specified";
            return true;
        }
    }

    eka2l1::io_system *io = emu->symsys->get_io_system();

    io->unmount(drive_e);

    if (eka2l1::common::is_dir(path)) {
        io->mount_physical_path(drive_e, drive_media::physical, io_attrib_removeable | (writable_sd ? 0 : io_attrib_write_protected), common::utf8_to_ucs2(path));
    } else {
        std::cout << "Mounting in progress. Please wait..." << std::endl;

        eka2l1::zip_mount_error error = emu->symsys->mount_game_zip(drive_e, drive_media::physical, path, (writable_sd ? 0 : io_attrib_write_protected));
        if (error != eka2l1::zip_mount_error_none) {
            switch (error) {
            case eka2l1::zip_mount_error_corrupt:
                *err = "The given ZIP file is corrupted!";
                break;

            case eka2l1::zip_mount_error_no_system_folder:
                *err = "The game dump does not have System folder in its root directory!";
                break;

            case eka2l1::zip_mount_error_not_zip:
                *err = "The given file is not a ZIP file!";
                break;

            default:
                *err = "Unidentified ZIP mount error.";
                break;
            }

            return false;
        }
    }

    return true;
}

bool device_set_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err) {
    desktop::emulator *emu = reinterpret_cast<desktop::emulator *>(userdata);
    const char *device = parser->next_token();

    if (!device) {
        *err = "No device specified";
        return true;
    }

    auto &devices_list = emu->symsys->get_device_manager()->get_devices();
    bool found = false;

    for (std::size_t i = 0; i < devices_list.size(); i++) {
        if (device == devices_list[i].firmware_code) {
            if (emu->conf.device != static_cast<int>(i)) {
                emu->conf.device = static_cast<int>(i);
                emu->symsys->set_device(static_cast<std::uint8_t>(i));
            }

            found = true;
            break;
        }
    }

    return found;
}

bool keybind_profile_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err) {
    desktop::emulator *emu = reinterpret_cast<desktop::emulator *>(userdata);
    const char *profile = parser->next_token();

    if (!profile) {
        *err = "No keybind profile specified";
        return true;
    }

    // Check if profile exists
    const std::string profile_path = fmt::format("bindings//{}.yml", profile);
    if (!eka2l1::common::exists(profile_path)) {
        *err = "No profile with name {} exists";
        return true;
    }

    emu->conf.current_keybind_profile = profile;
    emu->conf.keybinds.deserialize(profile_path);

    return true;
}

bool set_mmcid_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err) {
    desktop::emulator *emu = reinterpret_cast<desktop::emulator *>(userdata);
    const char *mmc_id = parser->next_token();

    if (!mmc_id) {
        *err = "No MMC-ID entered!";
        return true;
    }

    emu->conf.mmc_id = mmc_id;
    return true;
}

bool run_ngage_game_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err) {
    desktop::emulator *emu = reinterpret_cast<desktop::emulator *>(userdata);
    eka2l1::apa_app_registry registry;

    if (!emu->symsys->get_ngage_game_info_mounted(registry)) {
        *err = "Can't find the game mounted in E drive! If you believe this is a mistake, contact the emulator developers!";
        return false;
    }

    kernel_system *kern = emu->symsys->get_kernel_system();
    eka2l1::applist_server *svr = nullptr;

    if (kern) {
        svr = reinterpret_cast<eka2l1::applist_server *>(kern->get_by_name<service::server>(
            get_app_list_server_name_by_epocver(kern->get_epoc_version())));
    }

    if (!svr) {
        *err = "Can't get app list server!\n";
        return false;
    }

    epoc::apa::command_line cmdline;
    cmdline.launch_cmd_ = epoc::apa::command_create;

    if (!svr->launch_app(registry, cmdline, nullptr, []() {
        exit(0);
    })) {
        *err = "Launch app failed!";
        return false;
    }

    emu->init_app_launched = true;
    return true;
}

#if ENABLE_PYTHON_SCRIPTING
bool python_docgen_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err) {
    try {
        pybind11::exec(
            "import os\n"
            "import sys\n"
            "if not hasattr(sys, 'argv'):\n"
            "   sys.argv = ['temp', '-b', 'html', 'scripts/source/', 'scripts/build/']\n"
            "import sphinx.cmd.build\n"
            "sphinx.cmd.build.build_main(['-b', 'html', 'scripts/source/', 'scripts/build/'])");
    } catch (pybind11::error_already_set &exec) {
        std::cout << std::string("Generate documents failed with error: ") + exec.what() << std::endl;
    }

    return false;
}
#endif

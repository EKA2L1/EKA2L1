/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <j2me/applist.h>
#include <j2me/common.h>
#include <j2me/kmidrun.h>

#include <system/epoc.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <config/config.h>
#include <vfs/vfs.h>

#include <fmt/format.h>

namespace eka2l1::j2me {
    static std::string reduce_special_character_in_name(const std::string &previous) {
        std::string newstr;
        for (const char c : previous) {
            switch (c) {
            case '\\':
                newstr += "_00";
                break;

            case '/':
                newstr += "_01";
                break;

            case '?':
                newstr += "_04";
                break;

            case '.':
                newstr += "_09";
                break;

            case '_':
                newstr += "__";
                break;

            default:
                newstr += c;
                break;
            }
        }
        return newstr;
    }

    static std::string build_path_to_jar_dir(const app_entry &entry) {
        return fmt::format("C:\\system\\midp\\{}\\untrusted\\{}\\{}\\", reduce_special_character_in_name(entry.author_),
            reduce_special_character_in_name(entry.name_), reduce_special_character_in_name(entry.version_));
    }

    static std::string build_path_to_jar(const app_entry &entry) {
        return build_path_to_jar_dir(entry) + fmt::format("{}.jar", reduce_special_character_in_name(entry.name_));
    }

    void launch_through_kmidrun(system *sys, const app_entry &entry) {
        std::u16string jar_path = common::utf8_to_ucs2(build_path_to_jar(entry));
        io_system *io = sys->get_io_system();

        if (!io->exist(jar_path)) {
            LOG_ERROR(J2ME, "The JAR has disappeared!");
            return;
        }

        std::u16string jad_path = jar_path;
        jad_path.back() = 'd';
        
        if (!io->exist(jad_path)) {
            auto f_jad = io->open_file(jad_path, WRITE_MODE | BIN_MODE);
            if (!f_jad) {
                LOG_ERROR(J2ME, "Can't recreate JAD file for launch!");
                return;
            }

            std::string jar_real_path = common::ucs2_to_utf8(io->get_raw_path(jar_path).value());
            FILE *f_jar = common::open_c_file(jar_real_path, "rb");
            std::string jad_content;

            install_error err = create_jad_fake_link_from_jar(f_jar, jad_content);
            if (err != INSTALL_ERROR_JAR_SUCCESS) {
                LOG_ERROR(J2ME, "Can't create JAD file for launch. Error code {}", static_cast<int>(err));
                return;
            }
            
            f_jad->write_file(jad_content.c_str(), 1, static_cast<std::uint32_t>(jad_content.size()));
            f_jad->close();
        }

        // PortNumber*MidletUIDInDecimal*MidletTitle*JARPath*JADPATH*
        const std::u16string args = std::u16string(u"7049*268437956*") + common::utf8_to_ucs2(entry.original_title_)
            + u"*" + jar_path + u"*" + jad_path + u"*";

        if (!sys->load(u"Z:\\system\\programs\\kmidrun.exe", args)) {
            LOG_ERROR(J2ME, "Can't launch KMidRun for running Midlet!");
        }
    }

    install_error install_for_kmidrun(system *sys, app_list *applist, const std::string &path, app_entry &entry_info) {
        FILE *jar_file_handle = common::open_c_file(path, "rb");
        if (jar_file_handle == nullptr) {
            return INSTALL_ERROR_JAR_NOT_FOUND;
        }

        app_entry entry;
        int midp_ver;
        std::string jad_content;

        install_error fin_err = get_app_entry(jar_file_handle, entry, jad_content, midp_ver);
        if (fin_err != INSTALL_ERROR_JAR_SUCCESS) {
            return fin_err;
        }

        if (midp_ver > 1) {
            return INSTALL_ERROR_JAR_ONLY_MIDP1_SUPPORTED;
        }

        fseek(jar_file_handle, 0, SEEK_SET);

        std::string real_icon_path;
        if (extract_icon_to_store(jar_file_handle, *sys->get_config(), entry, real_icon_path) == INSTALL_ERROR_JAR_SUCCESS) {
            entry.icon_path_ = real_icon_path;
        }
        
        fclose(jar_file_handle);

        std::uint32_t add_res = applist->add_entry(entry, true);
        if (add_res == static_cast<std::uint32_t>(-1)) {
            return INSTALL_ERROR_JAR_CANT_ADD_TO_DB;
        }

        entry_info = entry;

        std::u16string path_to_dir = common::utf8_to_ucs2(build_path_to_jar_dir(entry));
        io_system *io = sys->get_io_system();

        io->create_directories(path_to_dir);
        std::optional<std::u16string> raw_path_to_dir = io->get_raw_path(path_to_dir);

        if (raw_path_to_dir.has_value()) {
            std::string path_to_real = common::ucs2_to_utf8(raw_path_to_dir.value()) + fmt::format("{}.jar", reduce_special_character_in_name(entry.name_));
            common::copy_file(path, path_to_real, true);

            // Change to jad
            path_to_real.back() = 'd';
            FILE *f = common::open_c_file(path_to_real, "wb");
            if (f) {
                fwrite(jad_content.c_str(), 1, jad_content.size(), f);
                fclose(f);
            }
        }

        return INSTALL_ERROR_JAR_SUCCESS;
    }
    
    void uninstall_for_kmidrun(system *sys, app_list *applist, const app_entry &entry) {
        applist->remove_entry(entry.id_);

        const std::string icon_real_path = eka2l1::add_path(sys->get_config()->storage, entry.icon_path_);
        common::remove(icon_real_path);

        std::u16string jar_path = common::utf8_to_ucs2(build_path_to_jar(entry));
        io_system *io = sys->get_io_system();

        io->delete_entry(jar_path);
        jar_path.back() = 'd';

        io->delete_entry(jar_path);
    }
}
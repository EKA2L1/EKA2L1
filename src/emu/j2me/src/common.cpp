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

#include <j2me/common.h>
#include <j2me/applist.h>

#include <common/algorithm.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <common/pystr.h>
#include <config/config.h>

#include <fmt/format.h>
#include <miniz.h>
#include <memory>
#include <sstream>

namespace eka2l1::j2me {
    static bool locate_file_in_zip(mz_zip_archive *archive, const std::string &full_path, mz_uint &index, std::uint64_t &size) {
        mz_zip_archive_file_stat stat;
        for (mz_uint i = 0; i < mz_zip_reader_get_num_files(archive); i++) {
            if (mz_zip_reader_file_stat(archive, i, &stat)) {
                if (common::compare_ignore_case(full_path.c_str(), stat.m_filename) == 0) {
                    index = i;
                    size = stat.m_uncomp_size;
                    return true;
                }
            }
        }

        return false;
    }

    install_error create_jad_fake_link_from_jar(FILE *jar_file_handle, std::string &jad_content) {
        std::unique_ptr<mz_zip_archive> archive = std::make_unique<mz_zip_archive>();
        if (!mz_zip_reader_init_cfile(archive.get(), jar_file_handle, 0, 0)) {
            return INSTALL_ERROR_JAR_INVALID;
        }

        static const char *MANIFEST_FILE_PATH = "META-INF/MANIFEST.MF";
        std::uint32_t index = 0;
        std::uint64_t fsize = 0;
        
        if (!locate_file_in_zip(archive.get(), MANIFEST_FILE_PATH, index, fsize)) {
            return INSTALL_ERROR_JAR_INVALID;
        }

        // Suspicous large
        if (fsize >= common::MB(1)) {
            return INSTALL_ERROR_JAR_INVALID;
        }

        char *string_buffer = new char[fsize];
        if (!mz_zip_reader_extract_to_mem(archive.get(), index, string_buffer, fsize, 0)) {
            delete[] string_buffer;
            return INSTALL_ERROR_JAR_INVALID;
        }

        mz_zip_reader_end(archive.get());

        fseek(jar_file_handle, 0, SEEK_END);
        long file_size = ftell(jar_file_handle);

        jad_content = std::string(string_buffer, fsize);
        jad_content += "\n";
        jad_content += "MIDlet-Jar-URL: https://12z1.com/jar/fake.jar\n";
        jad_content += fmt::format("MIDlet-Jar-Size: {}", file_size);

        delete[] string_buffer;
        return INSTALL_ERROR_JAR_SUCCESS;
    }

    install_error extract_icon_to_store(FILE *jar_file_handle, const config::state &conf, const app_entry &entry, std::string &final_real_path) {
        std::unique_ptr<mz_zip_archive> archive = std::make_unique<mz_zip_archive>();
        if (!mz_zip_reader_init_cfile(archive.get(), jar_file_handle, 0, 0)) {
            return INSTALL_ERROR_JAR_INVALID;
        }

        const std::string fname = eka2l1::filename(entry.icon_path_);
        const std::string relative_path = fmt::format("j2me\\{}_{}_{}", common::lowercase_string(entry.name_),
            common::lowercase_string(entry.version_), fname);
        const std::string storing_file = eka2l1::add_path(conf.storage, relative_path);

        FILE *out_file = common::open_c_file(storing_file, "wb");
        if (!out_file) {
            LOG_WARN(J2ME, "Can't open storing icon file!");

            final_real_path = "";
            return INSTALL_ERROR_JAR_SUCCESS;
        }

        std::uint32_t index = 0;
        std::uint64_t fsize = 0;
        
        if (!locate_file_in_zip(archive.get(), entry.icon_path_.c_str(), index, fsize)) {
            return INSTALL_ERROR_JAR_INVALID;
        }

        if (!mz_zip_reader_extract_to_cfile(archive.get(), index, out_file, 0)) {
            LOG_WARN(J2ME, "Can't extract icon file with path {} for storing. Ignoring...", entry.icon_path_);
            final_real_path = "";
        } else {
            final_real_path = relative_path;
        }

        fclose(out_file);

        return INSTALL_ERROR_JAR_SUCCESS;
    }
    
    install_error get_app_entry(FILE *jar_file_handle, app_entry &entry, std::string &jad_content, int &midp_ver) {
        install_error return_err = create_jad_fake_link_from_jar(jar_file_handle, jad_content);

        if (return_err != INSTALL_ERROR_JAR_SUCCESS) {
            return return_err;
        }

        std::istringstream jad_stream(jad_content);
        std::string line;

        midp_ver = 2;

        // Use this getline, it will detects platforms newline
        while (std::getline(jad_stream, line)) {
            std::size_t key_sep = line.find_first_of(':');
            if (key_sep == std::string::npos) {
                continue;
            }

            std::string key = common::trim_spaces(line.substr(0, key_sep));
            std::string val = common::trim_spaces(line.substr(key_sep + 1));
            if (key == "MIDlet-Name") {
                entry.name_ = val;
            } else if (key == "MIDlet-Version") {
                entry.version_ = val;
            } else if (key == "MIDlet-Vendor") {
                entry.author_ = val;
            } else if (key == "MIDlet-1") {
                auto comps = common::pystr(val).split(",");
                if (comps.size() >= 1) {
                    entry.title_ = comps[0].strip().std_str();
                }
                if (comps.size() >= 2) {
                    entry.icon_path_ = comps[1].strip().std_str();
                }
            } else if (key == "MicroEdition-Profile") {
                if (val == "MIDP-1.0") {
                    midp_ver = 1;
                }
            }
        }

        if (!entry.icon_path_.empty() && eka2l1::is_separator(entry.icon_path_[0])) {
            entry.icon_path_.erase(entry.icon_path_.begin());
        }

        entry.original_title_ = entry.title_;

        return INSTALL_ERROR_JAR_SUCCESS;
    }
}
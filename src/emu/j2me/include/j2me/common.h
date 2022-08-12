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

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace eka2l1::config {
    struct state;
}

namespace eka2l1::j2me {
    static constexpr std::uint32_t CURRENT_VERSION = 1;

    enum install_error {
        INSTALL_ERROR_JAR_SUCCESS = 0,
        INSTALL_ERROR_JAR_NOT_FOUND = 1,
        INSTALL_ERROR_JAR_INVALID = 2,
        INSTALL_ERROR_JAR_ONLY_MIDP1_SUPPORTED = 3,
        INSTALL_ERROR_JAR_CANT_ADD_TO_DB = 4,
        INSTALL_ERROR_NOT_SUPPORTED_FOR_PLAT = 5
    };

    struct app_entry;

    install_error create_jad_fake_link_from_jar(FILE *jar_file_handle, std::string &jad_content);
    install_error get_app_entry(FILE *jar_file_handle, app_entry &entry, std::string &jad_content, int &midp_ver);
    install_error extract_icon_to_store(FILE *jar_file_handle, const config::state &conf, const app_entry &entry, std::string &final_real_path);
}
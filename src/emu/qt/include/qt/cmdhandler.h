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

#pragma once

#include <common/configure.h>
#include <string>

namespace eka2l1::common {
    class arg_parser;
}

bool app_install_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err);
bool package_remove_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err);
bool app_specifier_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err);
bool help_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err);
bool rpkg_unpack_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err);
bool list_app_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err);
bool list_devices_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err);
bool fullscreen_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err);
bool mount_card_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err);

#if ENABLE_SCRIPTING
bool python_docgen_option_handler(eka2l1::common::arg_parser *parser, void *userdata, std::string *err);
#endif

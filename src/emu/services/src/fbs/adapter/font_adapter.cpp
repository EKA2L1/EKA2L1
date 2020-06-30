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

#include <services/fbs/adapter/font_adapter.h>
#include <services/fbs/adapter/stb_font_adapter.h>
#include <services/fbs/adapter/gdr_font_adapter.h>

namespace eka2l1::epoc::adapter {
    std::unique_ptr<font_file_adapter_base> make_font_file_adapter(const font_file_adapter_kind kind, std::vector<std::uint8_t> &dat) {
        switch (kind) {
        case font_file_adapter_kind::stb: {
            return std::make_unique<stb_font_file_adapter>(dat);
        }

        case font_file_adapter_kind::gdr: {
            return std::make_unique<gdr_font_file_adapter>(dat);
        }

        default: {
            break;
        }
        }

        return nullptr;
    }
}

/*
* Copyright (c) 2019 EKA2L1 Team.
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

#include <common/types.h>
#include <string>

namespace eka2l1 {
    class io_system;

    namespace utils {
        /**
         * \brief Get nearest existed language file.
         */
        std::u16string get_nearest_lang_file(io_system *io, const std::u16string &path,
            const language preferred_lang, const drive_number on_drive);

        bool is_file_compatible_with_language(const std::string &path, const std::string &target_ext,
            const language ideal_lang);
    }
}

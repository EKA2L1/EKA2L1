/*
 * Copyright (c) 2018- EKA2L1 Team.
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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1::common {
    enum chunkyseri_mode {
        SERI_MODE_READ,
        SERI_MODE_WRITE,
        SERI_MODE_MESAURE  
    };

    struct chunkyseri_section {
        std::uint8_t    *buf;
        std::int16_t   ver;
        std::string     name;

        explicit chunkyseri_section(const std::string &name, std::uint8_t *buf, 
            const std::int16_t v)
            : buf(buf), name(name), ver(v) {
        }

        operator bool() const {
            return ver > 0;
        }
    };

    class chunkyseri {
        std::uint8_t *buf;
        chunkyseri_mode mode;

        void absorb_impl(std::uint8_t *dat, const std::size_t s);
        bool do_marker(const std::string &name, std::uint16_t cookie = 0x18);

    public:
        chunkyseri_mode get_seri_mode() const {
            return mode;
        }

        chunkyseri_section section(const std::string &name, const std::int16_t ver) {
            return section(name, ver, ver);
        }

        chunkyseri_section section(const std::string &name, const std::int16_t ver_min,
            const std::int16_t ver);

        bool expect(const std::uint8_t *dat, const std::size_t s);

        template <typename T>
        void absorb(T &dat);

        template <typename T, typename F>
        void absorb_container(std::vector<T> &c, F func) {
            std::uint32_t s = static_cast<std::uint32_t>(c.size());
            absorb(s);

            if (mode == SERI_MODE_WRITE) {
                c.resize(s);
            }

            for (auto &member: c) {
                func(*this, member);
            }
        }
    };
}
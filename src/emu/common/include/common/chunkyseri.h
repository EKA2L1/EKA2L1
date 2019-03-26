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
#include <type_traits>
#include <vector>

namespace eka2l1::common {
    enum chunkyseri_mode {
        SERI_MODE_READ,
        SERI_MODE_WRITE,
        SERI_MODE_MEASURE
    };

    struct chunkyseri_section {
        std::uint8_t *buf;
        std::int16_t ver;
        std::string name;

        explicit chunkyseri_section(const std::string &name, std::uint8_t *buf,
            const std::int16_t v)
            : buf(buf)
            , name(name)
            , ver(v) {
        }

        operator bool() const {
            return ver > 0;
        }
    };

    /**
     * @brief Simple serialization class.
     * 
     * Using this class, you can serialize your data to buffer, and deserialize it later.
     * The method of serialize is easy to understand, it simply stores your data to buffer as
     * binary.
     * 
     * There are three serialization mode:
     * - Read: Deserialize data from buffer.
     * - Write: Serialize data to buffer.
     * - Measure: Calculate total bytes of buffer needed when data are done serialized.
     * 
     * @see basic_stream ro_stream wo_stream
     */
    class chunkyseri {
        std::uint8_t *buf;
        std::uint8_t *org;
        std::uint8_t *end;

        chunkyseri_mode mode;

        bool do_marker(const std::string &name, std::uint16_t cookie = 0x18);

    public:
        explicit chunkyseri(std::uint8_t *buf, const std::size_t max, const chunkyseri_mode mode)
            : buf(buf)
            , org(buf)
            , mode(mode)
            , end(org + max) {
        }

        std::size_t size() {
            return buf - org;
        }

        bool eos() {
            return buf >= end;
        }

        chunkyseri_mode get_seri_mode() const {
            return mode;
        }

        chunkyseri_section section(const std::string &name, const std::int16_t ver) {
            return section(name, ver, ver);
        }

        chunkyseri_section section(const std::string &name, const std::int16_t ver_min,
            const std::int16_t ver);

        bool expect(const std::uint8_t *dat, const std::size_t s);
        void absorb_impl(std::uint8_t *dat, const std::size_t s);

        template <typename T>
        std::enable_if_t<std::is_integral_v<T>> absorb(T &dat) {
            absorb_impl(reinterpret_cast<std::uint8_t *>(&dat), sizeof(T));
        }

        template <typename T>
        std::enable_if_t<std::is_enum_v<T>> absorb(T &dat) {
            absorb_impl(reinterpret_cast<std::uint8_t *>(&dat), sizeof(T));
        }

        void absorb(std::string &dat);
        void absorb(std::u16string &dat);

        template <typename T>
        void absorb_container(std::vector<T> &c) {
            std::uint32_t s = static_cast<std::uint32_t>(c.size());
            absorb(s);

            if (mode == SERI_MODE_READ) {
                c.resize(s);
            }

            for (std::uint32_t i = 0; i < s; i++) {
                absorb(c[i]);
            }
        }

        template <typename T, typename F, typename CONTAINER_SIZE_TYPE = std::uint32_t>
        void absorb_container(std::vector<T> &c, F func) {
            CONTAINER_SIZE_TYPE s = static_cast<CONTAINER_SIZE_TYPE>(c.size());
            absorb(s);

            if (mode == SERI_MODE_READ) {
                c.resize(s);
            }

            for (auto &member : c) {
                func(*this, member);
            }
        }

        template <typename T, typename CONTAINER_SIZE_TYPE = std::uint32_t>
        void absorb_container_do(std::vector<T> &c) {
            CONTAINER_SIZE_TYPE s = static_cast<CONTAINER_SIZE_TYPE>(c.size());
            absorb(s);

            if (mode == SERI_MODE_READ) {
                c.resize(s);
            }

            for (auto &m : c) {
                m.do_state(*this);
            }
        }
    };
}

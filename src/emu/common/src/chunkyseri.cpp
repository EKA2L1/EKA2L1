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

#include <common/chunkyseri.h>
#include <common/log.h>

#include <type_traits>

#include <string.h>

namespace eka2l1::common {
    void chunkyseri::absorb_impl(std::uint8_t *dat, const std::size_t s) {
        switch (mode) {
        case SERI_MODE_MESAURE:
            break;

        case SERI_MODE_WRITE: {
            memcpy(reinterpret_cast<void *>(buf), reinterpret_cast<const void *>(dat), s);
            break;
        }

        case SERI_MODE_READ: {
            memcpy(reinterpret_cast<void *>(dat), reinterpret_cast<const void *>(buf), s);
            break;
        }

        default:
            break;
        }

        buf += s;
    }

    bool chunkyseri::expect(const std::uint8_t *dat, const std::size_t s) {
        switch (mode) {
        case SERI_MODE_MESAURE:
            break;

        case SERI_MODE_WRITE: {
            memcpy(reinterpret_cast<void *>(buf), reinterpret_cast<const void *>(dat), s);
            break;
        }

        case SERI_MODE_READ: {
            if (memcmp(reinterpret_cast<const void *>(dat), reinterpret_cast<const void *>(buf), s) != 0) {
                return false;
            }

            break;
        }

        default:
            break;
        }

        buf += s;
        return true;
    }

    void chunkyseri::absorb(std::string &dat) {
        std::uint32_t s = static_cast<std::uint32_t>(dat.size());
        absorb_impl(reinterpret_cast<std::uint8_t *>(&s), sizeof(std::uint32_t));

        if (mode == SERI_MODE_READ) {
            dat.resize(s);
        }

        absorb_impl(reinterpret_cast<std::uint8_t *>(&dat[0]), s);
    }

    void chunkyseri::absorb(std::u16string &dat) {
        std::uint32_t s = static_cast<std::uint32_t>(dat.size());
        absorb_impl(reinterpret_cast<std::uint8_t *>(&s), sizeof(std::uint32_t));

        if (mode == SERI_MODE_READ) {
            dat.resize(s);
        }

        absorb_impl(reinterpret_cast<std::uint8_t *>(&dat[0]), s * 2);
    }

    bool chunkyseri::do_marker(const std::string &name, std::uint16_t cookie) {
        std::uint16_t ca = cookie;
        absorb(ca);

        if (mode == SERI_MODE_READ && ca != cookie) {
            LOG_ERROR("Marker failed for {} ({} vs {})", name, cookie, ca);
            return false;
        }

        return true;
    }

    chunkyseri_section chunkyseri::section(const std::string &name, const std::int16_t ver_min,
        const std::int16_t ver) {
        std::int16_t found_ver = ver;

        if (expect(reinterpret_cast<const std::uint8_t *>(&name[0]), name.length())) {
            absorb(found_ver);

            if (found_ver < ver_min || found_ver > ver) {
                LOG_ERROR("Chunk section {} has version that is not in range ({}-{})", ver_min, ver);
                return chunkyseri_section(name, nullptr, -1);
            }

            return chunkyseri_section(name, buf, found_ver);
        }

        return chunkyseri_section(name, nullptr, -1);
    }
}
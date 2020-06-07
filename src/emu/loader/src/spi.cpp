/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <loader/spi.h>

#include <common/chunkyseri.h>
#include <common/crypt.h>

namespace eka2l1::loader {
    static std::uint32_t calculate_checksum(const void *uids) {
        const std::uint8_t *cur = reinterpret_cast<decltype(cur)>(uids);
        const std::uint8_t *end = cur + 12;

        std::uint8_t buf[6];
        std::uint8_t *p = &buf[0];

        while (cur < end) {
            *p++ = (*cur);
            cur += 2;
        }

        std::uint16_t crc = 0;
        crypt::crc16(crc, &buf[0], 6);

        return crc;
    }

    static std::uint32_t calculate_checked_uid_checksum(const std::uint32_t *uids) {
        return (calculate_checksum(reinterpret_cast<const std::uint8_t *>(uids) + 1) << 16) | calculate_checksum(uids);
    }

    spi_header::spi_header(const std::uint32_t type) {
        uids[0] = SPI_UID;
        uids[1] = type;
        uids[2] = 0;

        crc = calculate_checked_uid_checksum(uids);
    }

    bool spi_header::do_state(common::chunkyseri &seri) {
        seri.absorb(uids[0]);

        if (uids[0] != SPI_UID) {
            return false;
        }

        seri.absorb(uids[1]);
        seri.absorb(uids[2]);

        seri.absorb(crc);

        if (calculate_checked_uid_checksum(uids) != crc) {
            return false;
        }

        // 16 bytes are reserved for the header
        // Fill padding in
        std::vector<std::uint8_t> padding(16, 0);
        seri.absorb_impl(&padding[0], 16);

        return true;
    }

    bool spi_entry::do_state(common::chunkyseri &seri) {
        std::uint32_t name_len = static_cast<std::uint32_t>(name.length());
        seri.absorb(name_len);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            name.resize(name_len);
        }

        // Read file size
        std::uint32_t rsc_size = static_cast<std::uint32_t>(file.size());
        seri.absorb(rsc_size);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            file.resize(rsc_size);
        }

        if (seri.eos()) {
            return false;
        }

        // Read the name
        seri.absorb_impl(reinterpret_cast<std::uint8_t *>(&name[0]), name_len);
        seri.absorb_impl(reinterpret_cast<std::uint8_t *>(&file[0]), rsc_size);

        // Pad to align of 4
        // We don't want extra ARM instructions, either do they, so it has to be aligned.
        std::uint8_t padding_bytes_size = 4 - ((name_len + rsc_size) % 4);
        char padding_byte = '\0';

        if (padding_bytes_size < 4) {
            for (std::uint8_t i = 0; i < padding_bytes_size; i++) {
                seri.absorb(padding_byte);
            }
        }

        return true;
    }

    bool spi_file::do_state(common::chunkyseri &seri) {
        if (!header.do_state(seri)) {
            return false;
        }

        while (!seri.eos()) {
            spi_entry entry;

            if (!entry.do_state(seri)) {
                return false;
            }

            entries.push_back(std::move(entry));
        }

        return true;
    }
}

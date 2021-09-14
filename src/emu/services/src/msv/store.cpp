/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/msv/store.h>
#include <common/buffer.h>
#include <utils/des.h>
#include <utils/cardinality.h>

namespace eka2l1::epoc::msv {
    static constexpr std::uint32_t STORE_FILE_UID = 0x10003C68;
    static const std::uint32_t STANDARD_HEADER_UID[4] = { STORE_FILE_UID, STORE_FILE_UID, 0, 0x008D8E4B };

    store::store() {

    }

    bool store::read(common::ro_stream &stream) {
        std::uint32_t uid_check[4] = { 0, 0, 0, 0 };
        if (stream.read(uid_check, 4 * sizeof(std::uint32_t)) != 4 * sizeof(std::uint32_t)) {
            return false;
        }

        if (std::memcmp(uid_check, STANDARD_HEADER_UID, 4 * sizeof(std::uint32_t)) != 0) {
            LOG_ERROR(SERVICE_MSV, "Store has invalid UID check!");
            return false;
        }

        utils::cardinality cardinality;
        if (!cardinality.internalize(stream)) {
            return false;
        }

        for (std::size_t i = 0; i < cardinality.value(); i++) {
            std::uint32_t uid_buffer = 0;
            if (stream.read(&uid_buffer, 4) != 4) {
                return false;
            }

            std::string buffer;
            if (!epoc::read_des_string(buffer, &stream, false)) {
                return false;
            }

            stores_[uid_buffer].insert(stores_[uid_buffer].begin(), reinterpret_cast<std::uint8_t*>(buffer.data()),
                reinterpret_cast<std::uint8_t*>(buffer.data() + buffer.size()));
        }

        return true;
    }

    bool store::write(common::wo_stream &stream) {
        if (stream.write(STANDARD_HEADER_UID, 4 * sizeof(std::uint32_t)) != 4 * sizeof(std::uint32_t)) {
            return false;
        }

        utils::cardinality cardinality;
        cardinality.val_ = static_cast<std::uint32_t>(stores_.size());

        if (!cardinality.externalize(stream)) {
            return false;
        }

        for (auto &[val, buffer]: stores_) {
            if (stream.write(&val, 4) != 4) {
                return false;
            }

            cardinality.val_ = static_cast<std::uint32_t>((buffer.size() << 1) + 1);
            if (!cardinality.externalize(stream)) {
                return false;
            }

            if (stream.write(buffer.data(), buffer.size()) != buffer.size()) {
                return false;
            }
        }

        return true;
    }

    store_buffer &store::buffer_for(const std::uint32_t uid) {
        return stores_[uid];
    }

    bool store::buffer_exists(const std::uint32_t uid) {
        return (stores_.find(uid) != stores_.end());
    }
}
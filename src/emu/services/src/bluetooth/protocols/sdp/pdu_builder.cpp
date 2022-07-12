/*
 * Copyright (c) 2022 EKA2L1 Team
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

#include <services/bluetooth/protocols/sdp/pdu_builder.h>
#include <common/bytes.h>

namespace eka2l1::epoc::bt {
    pdu_builder::pdu_builder()
        : trans_id_counter_(0) {
    }

    void pdu_builder::new_packet() {
        buffer_.clear();
        buffer_.resize(PDU_HEADER_SIZE);

        trans_id_counter_++;
        *reinterpret_cast<std::uint16_t*>(&buffer_[TRANS_ID_OFFSET]) = common::to_network_order(trans_id_counter_);
    }

    void pdu_builder::set_pdu_id(const std::uint8_t id) {
        buffer_[PDU_ID_OFFSET] = static_cast<char>(id);
    }

    void pdu_builder::put_data(const char *data, const std::size_t data_size) {
        buffer_.insert(buffer_.end(), data, data + data_size);
    }

    void pdu_builder::put_be32(const std::uint32_t data) {
        std::uint32_t netted_data = common::to_network_order(data);
        buffer_.insert(buffer_.end(), reinterpret_cast<const char*>(&netted_data), reinterpret_cast<const char*>(&netted_data + 1));
    }

    void pdu_builder::put_be16(const std::uint16_t data) {
        std::uint16_t netted_data = common::to_network_order(data);
        buffer_.insert(buffer_.end(), reinterpret_cast<const char*>(&netted_data), reinterpret_cast<const char*>(&netted_data + 1));
    }

    void pdu_builder::put_byte(const std::uint8_t byte) {
        buffer_.push_back(static_cast<char>(byte));
    }

    const char *pdu_builder::finalize(std::uint32_t &packet_size) {
        if (buffer_.empty()) {
            return nullptr;
        }

        std::uint16_t param_len = static_cast<std::uint16_t>(buffer_.size() - PDU_HEADER_SIZE);
        *reinterpret_cast<std::uint16_t*>(&buffer_[PARAM_LEN_OFFSET]) = common::to_network_order(param_len);

        packet_size = static_cast<std::uint32_t>(buffer_.size());
        return buffer_.data();
    }
}
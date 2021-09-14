/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <services/sms/common.h>
#include <common/chunkyseri.h>

#include <common/log.h>

namespace eka2l1::epoc::sms {
    void sms_octet::absorb(common::chunkyseri &seri) {
        seri.absorb(value_);        
    }

    void sms_validity_period::absorb(common::chunkyseri &seri) {
        first_octet_.absorb(seri);
        seri.absorb(interval_minutes_);
    }

    void sms_address::absorb(common::chunkyseri &seri) {
        addr_type_.absorb(seri);
        epoc::absorb_des_string(buffer_, seri, true);
    }

    void sms_submit::absorb(common::chunkyseri &seri) {
        service_center_address_.absorb(seri);
        first_octet_.absorb(seri);
        msg_reference_.absorb(seri);
        dest_addr_.absorb(seri);
        protocol_identifier_.absorb(seri);
        encoding_scheme_.absorb(seri);
        validity_period_.absorb(seri);

        // TODO: Absorb user data, but since we don't care much now laterz
    }

    bool sms_message::absorb(common::chunkyseri &seri) {
        seri.absorb(flags_);
        seri.absorb(store_status_);
        seri.absorb(log_id_);
        seri.absorb(time_);

        std::uint8_t type = 0;
        if (pdu_) {
            type = pdu_->pdu_type_;
        }

        if ((seri.get_seri_mode() != common::SERI_MODE_READ) && (type != sms_pdu_type_submit)) {
            LOG_ERROR(SERVICE_SMS, "Serialization PDU type other then SUBMIT not supported!");
            return false;
        }

        seri.absorb(type);
        
        if ((seri.get_seri_mode() == common::SERI_MODE_READ) && (type != sms_pdu_type_submit)) {
            LOG_ERROR(SERVICE_SMS, "Serialization PDU type other then SUBMIT not supported!");
            return false;
        }

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            pdu_ = std::make_unique<sms_submit>();
        }

        pdu_->pdu_type_ = static_cast<sms_pdu_type>(type);
        pdu_->absorb(seri);

        // How many bytes left, throw into don't care
        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            dontcare_.resize(seri.left());
        }

        seri.absorb_impl(dontcare_.data(), dontcare_.size());
        return true;
    }

    bool sms_header::absorb(common::chunkyseri &seri) {
        std::uint16_t version = 0;
        seri.absorb(version);

        std::uint32_t count = static_cast<std::uint32_t>(recipients_.size());
        seri.absorb(count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            recipients_.resize(count);
        }

        for (std::size_t i = 0; i < count; i++) {
            recipients_[i].absorb(seri);
        }

        seri.absorb(flags_);
        seri.absorb(bio_type_);

        return message_.absorb(seri);
    }
}
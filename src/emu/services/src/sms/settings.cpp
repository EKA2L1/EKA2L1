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

#include <services/sms/settings.h>
#include <common/chunkyseri.h>
#include <utils/des.h>

#include <system/epoc.h>
#include <kernel/kernel.h>
#include <vfs/vfs.h>

#include <common/log.h>

namespace eka2l1::epoc::sms {
    sms_number::sms_number()
        : recp_status_(recipient_status_not_yet_sent)
        , error_(0)
        , retries_(0)
        , time_(0)
        , log_id_(0)
        , delivery_status_(sms_no_acknowledge) {
    }

    void sms_number::absorb(common::chunkyseri &seri) {
        std::int16_t version = 1;
        seri.absorb(version);

        seri.absorb(recp_status_);
        seri.absorb(error_);
        seri.absorb(retries_);
        seri.absorb(time_);

        epoc::absorb_des_string(number_, seri, true);
        epoc::absorb_des_string(name_, seri, true);

        seri.absorb(log_id_);

        if (version <= 1) {
            if (seri.get_seri_mode() == common::SERI_MODE_READ) {
                delivery_status_ = sms_no_acknowledge;
            } 
        } else {
            std::uint8_t val = static_cast<std::uint8_t>(delivery_status_);
            seri.absorb(val);

            if (seri.get_seri_mode() == common::SERI_MODE_READ) {
                delivery_status_ = static_cast<sms_acknowledge_status>(val);
            }
        }
    }
    
    sms_message_settings::sms_message_settings()
        : msg_flags_(0)
        , message_conversion_(sms_pid_conversion_none)
        , alphabet_(sms_encoding_alphabet_ucs2)
        , validty_period_minutes_(0)
        , validity_period_format_(sms_vpf_integer) {
    }

    void sms_message_settings::absorb(common::chunkyseri &seri) {
        std::uint16_t version = 1;

        seri.absorb(version);
        seri.absorb(validty_period_minutes_);
        seri.absorb(validity_period_format_);
        seri.absorb(alphabet_);
        seri.absorb(msg_flags_);
        seri.absorb(message_conversion_);
    }

    sms_settings::sms_settings()
        : sms_message_settings()
        , set_flags_(0)
        , default_sc_index_(-1)
        , delivery_(sms_delivery_immediately)
        , status_report_handling_(sms_report_to_inbox_visible_and_match)
        , special_message_handling_(sms_report_to_inbox_visible_and_match)
        , commdb_action_(sms_commdb_no_action)
        , bearer_action_(sms_commdb_no_action)
        , sms_bearer_(mobile_sms_bearer_packet_preferred)
        , external_save_count_(300)
        , class2_(0x1002)
        , desc_length_(32) {
    }

    void sms_settings::absorb(common::chunkyseri &seri) {
        std::uint16_t version = 5;
        seri.absorb(version);

        if (version <= 4) {
            external_save_count_ = 0;
        } else {
            // This save count vs MTM save count, it will choose the one higher.
            // This with the dat file on s60v2
            if (seri.get_seri_mode() == common::SERI_MODE_WRITE) {
                external_save_count_++;
            }

            seri.absorb(external_save_count_);
        }

        sms_message_settings::absorb(seri);

        seri.absorb(set_flags_);
        seri.absorb(status_report_handling_);
        seri.absorb(special_message_handling_);
        seri.absorb(commdb_action_);
        seri.absorb(delivery_);
        seri.absorb(default_sc_index_);

        std::uint32_t count = static_cast<std::uint32_t>(sc_numbers_.size());
        seri.absorb(count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            sc_numbers_.resize(count);
        }

        for (std::uint32_t i = 0; i < count; i++) {
            sc_numbers_[i].absorb(seri);
        }

        if (version > 1) {
            seri.absorb(bearer_action_);
            seri.absorb(sms_bearer_);
        }

        if (version > 2) {
            seri.absorb(class2_);
        } else {
            class2_ = 0x1002;
        }

        if (version > 3) {
            seri.absorb(desc_length_);
        } else {
            desc_length_ = 32;          // Maximum
        }
    }
    
    void supply_sim_settings(eka2l1::system *sys, sms_settings *furnished) {
        io_system *io = sys->get_io_system();

        // 1st. Supply data to SMS settings dat file, and modify the MSV store file that contains
        static const char16_t *SMS_SETTING_DIR = u"C:\\System\\Data\\";
        static const char16_t *SMS_SETTING_PATH = u"C:\\System\\Data\\sms_settings.dat";
        symfile setting_read_file = io->open_file(SMS_SETTING_PATH, READ_MODE | BIN_MODE);
        
        sms_settings settings;

        // File should not be too big
        if (setting_read_file && setting_read_file->valid() && setting_read_file->size() <= common::KB(10)) {
            const std::size_t fsize = setting_read_file->size();

            std::vector<std::uint8_t> buffer;
            buffer.resize(fsize);

            if (setting_read_file->read_file(buffer.data(), static_cast<std::uint32_t>(fsize), 1) == fsize) {
                common::chunkyseri seri(buffer.data(), fsize, common::SERI_MODE_READ);
                settings.absorb(seri);
            }
        }

        // Check if no service center is available
        if (settings.sc_numbers_.empty()) {
            sms_number new_service_center;
            new_service_center.name_ = u"EKA2L1 Russia";
            new_service_center.number_ = u"+79099969037";

            settings.sc_numbers_.push_back(new_service_center);
            settings.default_sc_index_ = 0;
        }

        // Serialize and write the buffer back
        std::vector<std::uint8_t> buffer;
        common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MEASURE);

        settings.absorb(seri);
        buffer.resize(seri.size());

        seri = common::chunkyseri(buffer.data(), buffer.size(), common::SERI_MODE_WRITE);
        settings.absorb(seri);

        if (setting_read_file) {
            setting_read_file->close();
        }

        io->create_directories(SMS_SETTING_DIR);

        setting_read_file = io->open_file(SMS_SETTING_PATH, WRITE_MODE | BIN_MODE);
        if (!setting_read_file) {
            LOG_ERROR(SERVICE_SMS, "Unable to create SMS setting file!");
        } else {
            setting_read_file->write_file(buffer.data(), static_cast<std::uint32_t>(buffer.size()), 1);
            setting_read_file->close();
        }

        if (furnished) {
            *furnished = settings;
        }

        // TODO: Supply data to cenrep too. It was the main source
    }
}
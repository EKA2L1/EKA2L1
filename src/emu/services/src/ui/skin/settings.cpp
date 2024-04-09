/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <services/centralrepo/centralrepo.h>
#include <services/ui/skin/common.h>
#include <services/ui/skin/settings.h>
#include <services/ui/skin/utils.h>

#include <common/cvt.h>
#include <common/log.h>

#include <system/devices.h>

#include <system/epoc.h>

namespace eka2l1::epoc {
    enum {
        DEFAULT_SKIN_UID_KEY = 0x14,
        ACTIVE_SKIN_UID_KEY = 0x02,
        DEFAULT_SKIN_ID_KEY = 0x13,
        AKN_LAYOUT_ID_KEY = 0x01,
        HIGHTLIGHT_ANIM_ENABLED_KEY = 0x10
    };

    akn_ss_settings::akn_ss_settings(io_system *io, central_repo_server *svr)
        : io_(io)
        , dvcmngr_(nullptr) {
        if (svr) {
            dvcmngr_ = svr->get_system()->get_device_manager();

            avkon_rep_ = svr->load_repo_with_lookup(io, dvcmngr_, AVKON_UID);
            skins_rep_ = svr->load_repo_with_lookup(io, dvcmngr_, PERSONALISATION_UID);
            theme_rep_ = svr->load_repo_with_lookup(io, dvcmngr_, THEMES_UID);

            if (read_default_skin_id()) {
                LOG_INFO(SERVICE_UI, "Default skin UID: 0x{:X}, timestamp: {}", default_skin_pid_.first, default_skin_pid_.second);
            }

            if (read_active_skin_id()) {
                LOG_INFO(SERVICE_UI, "Active skin UID: 0x{:X}, timestamp: {}", active_skin_pid_.first, active_skin_pid_.second);
            }

            read_ah_mirroring_active();
            read_highlight_anim_enabled();

            // Dump the info
            LOG_INFO(SERVICE_UI, "Arabic/hebrew mirroring active: {}", ah_mirroring_active ? "True" : "False");
            LOG_INFO(SERVICE_UI, "Highlight anim enabled: {}", highlight_anim_enabled ? "True" : "False");
        } else {
            LOG_WARN(SERVICE_UI, "Central repository server not provided. Configuration will be tough");
        }
    }

    static bool read_pid(central_repo_entry *entry_, pid &pid_, const int base = 16, const bool uid_only = false) {
        pid_.second = 0;

        if (entry_->data.etype != central_repo_entry_type::string) {
            LOG_ERROR(SERVICE_UI, "Data of entry PID is not buffer. Reading PID failed!");
            return false;
        }

        const std::string uid_in_str = common::ucs2_to_utf8(std::u16string(reinterpret_cast<char16_t *>(entry_->data.strd.data()),
            entry_->data.strd.size() / 2));

        if (uid_in_str.length() <= 8 || uid_only) {
            // Only UID
            pid_.first = std::strtoul(uid_in_str.data(), nullptr, base);
            return true;
        }

        const std::string uid = uid_in_str.substr(0, 8);
        const std::string timestamp = uid_in_str.substr(8);

        // Set UID and timestamp
        pid_.first = std::strtoul(uid.data(), nullptr, base);
        pid_.second = std::strtoul(timestamp.data(), nullptr, base);

        return true;
    }

    bool akn_ss_settings::read_default_skin_id() {
        if (!skins_rep_) {
            LOG_ERROR(SERVICE_UI, "Skin repository failed to load!");
            return false;
        }

        // Set timestamp to 0 first
        central_repo_entry *default_skin_entry = skins_rep_->find_entry(DEFAULT_SKIN_UID_KEY);

        if (!default_skin_entry) {
            default_skin_entry = skins_rep_->find_entry(DEFAULT_SKIN_ID_KEY);

            if (!default_skin_entry) {
                LOG_ERROR(SERVICE_UI, "Can't read default skin id!");
                return false;
            }

            default_skin_pid_.first = static_cast<std::uint32_t>(default_skin_entry->data.intd);
            return true;
        }


        return read_pid(default_skin_entry, default_skin_pid_);
    }

    bool akn_ss_settings::read_ah_mirroring_active() {
        ah_mirroring_active = false;

        if (!avkon_rep_) {
            return false;
        }

        central_repo_entry *entry = avkon_rep_->find_entry(AKN_LAYOUT_ID_KEY);

        if (!entry) {
            return true;
        }

        if (entry->data.etype == central_repo_entry_type::integer) {
            ah_mirroring_active = static_cast<bool>(entry->data.intd);
        }

        return true;
    }

    bool akn_ss_settings::read_active_skin_id() {
        if (!skins_rep_) {
            LOG_ERROR(SERVICE_UI, "Skin repository failed to load!");
            return false;
        }

        // Set timestamp to 0 first
        central_repo_entry *default_skin_entry = skins_rep_->find_entry(ACTIVE_SKIN_UID_KEY);

        if (!default_skin_entry) {
            LOG_ERROR(SERVICE_UI, "Can't read active skin id!");
            return false;
        }

        return read_pid(default_skin_entry, active_skin_pid_, 10, true);
    }

    bool akn_ss_settings::read_highlight_anim_enabled() {
        highlight_anim_enabled = false;

        if (!skins_rep_) {
            return false;
        }

        central_repo_entry *entry = skins_rep_->find_entry(HIGHTLIGHT_ANIM_ENABLED_KEY);

        if (!entry) {
            return true;
        }

        if (entry->data.etype == central_repo_entry_type::integer) {
            highlight_anim_enabled = static_cast<bool>(entry->data.intd);
        }

        return true;
    }

    void akn_ss_settings::set_pid_to_skins_repo(const std::uint32_t key, const epoc::pid id, const bool uid_only) {
        central_repo_entry *active_skin_entry = skins_rep_->find_entry(key);
        std::u16string data = uid_only ? common::utf8_to_ucs2(common::to_string(id.first)) : epoc::pid_to_string(id);

        if (!active_skin_entry) {
            eka2l1::central_repo_entry_variant variant;

            variant.etype = central_repo_entry_type::string;
            variant.strd.resize(data.size() * 2);
 
            std::memcpy(variant.strd.data(), data.data(), variant.strd.size());

            skins_rep_->add_new_entry(key, variant);
        } else {
            active_skin_entry->data.etype = central_repo_entry_type::string;
            active_skin_entry->data.strd.resize(data.size() * 2);

            std::memcpy(active_skin_entry->data.strd.data(), data.data(), active_skin_entry->data.strd.size());
        }

        skins_rep_->write_changes(io_, dvcmngr_);
    }

    void akn_ss_settings::active_skin_pid(const epoc::pid id) {
        active_skin_pid_ = id;
        set_pid_to_skins_repo(ACTIVE_SKIN_UID_KEY, id, true);
    }

    void akn_ss_settings::default_skin_pid(const epoc::pid id) {
        default_skin_pid_ = id;
        set_pid_to_skins_repo(DEFAULT_SKIN_UID_KEY, id, false);
    }
}

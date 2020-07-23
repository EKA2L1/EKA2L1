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

#include <services/drm/notifier/events.h>
#include <common/buffer.h>

namespace eka2l1::drm {
    bool notifier_event::internalize(common::ro_stream &stream) {
        return (stream.read(&type_, sizeof(type_)) == sizeof(type_));
    }

    static std::optional<std::string> read_content_id_string(common::ro_stream &stream) {
        std::uint32_t len = 0;

        if (stream.read(&len, 4) != 4) {
            return std::nullopt;
        }

        std::string content_id;
        content_id.resize(len);

        if (stream.read(&content_id[0], len) != len) {
            return std::nullopt;
        }

        return content_id;
    }

    bool notifier_add_remove_event::internalize(common::ro_stream &stream) {
        if (!notifier_event::internalize(stream)) {
            return false;
        }

        if (auto cid = read_content_id_string(stream)) {
            content_id_ = std::move(cid.value());
        } else {
            return false;
        }

        if (stream.read(&status_, sizeof(status_)) != sizeof(status_)) {
            return false;
        }

        return true;
    }
    
    bool notifier_modify_event::internalize(common::ro_stream &stream) {
        if (!notifier_event::internalize(stream)) {
            return false;
        }

        if (auto cid = read_content_id_string(stream)) {
            content_id_ = std::move(cid.value());
        } else {
            return false;
        }

        if (stream.read(&uid_, sizeof(uid_)) != sizeof(uid_)) {
            return false;
        }

        return true;
    }
    
    bool notifier_time_change_event::internalize(common::ro_stream &stream) {
        if (!notifier_event::internalize(stream)) {
            return false;
        }

        if (stream.read(&old_time_, sizeof(old_time_)) != sizeof(old_time_)) {
            return false;
        }

        if (stream.read(&new_time_, sizeof(new_time_)) != sizeof(new_time_)) {
            return false;
        }

        if (stream.read(&old_time_zone_, sizeof(old_time_zone_)) != sizeof(old_time_zone_)) {
            return false;
        }

        if (stream.read(&new_time_zone_, sizeof(new_time_zone_)) != sizeof(new_time_zone_)) {
            return false;
        }

        if (stream.read(&old_security_level_, sizeof(old_security_level_)) != sizeof(old_security_level_)) {
            return false;
        }

        if (stream.read(&new_security_level_, sizeof(new_security_level_)) != sizeof(new_security_level_)) {
            return false;
        }

        return true;
    }
}
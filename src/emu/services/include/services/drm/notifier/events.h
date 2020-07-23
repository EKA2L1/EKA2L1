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

#pragma once

#include <utils/locale.h>

#include <cstdint>
#include <optional>

namespace eka2l1::common {
    class ro_stream;
}

namespace eka2l1::drm {
    enum notifier_event_type {
        notifier_event_type_none = 0,
        notifier_event_type_add_remove = 1,
        notifier_event_type_modify = 2,
        notifier_event_type_time_change = 3
    };

    struct notifier_event {
        std::uint32_t type_;

        virtual std::optional<std::string> content_id() = 0;
        virtual bool internalize(common::ro_stream &stream);
    };

    struct notifier_add_remove_event: public notifier_event {
        std::string content_id_;
        std::uint8_t status_;

        std::optional<std::string> content_id() override {
            return content_id_;
        }

        bool internalize(common::ro_stream &stream) override;
    };

    struct notifier_modify_event: public notifier_event {
        std::string content_id_;
        std::uint32_t uid_;

        std::optional<std::string> content_id() override {
            return content_id_;
        }

        bool internalize(common::ro_stream &stream) override;
    };

    struct notifier_time_change_event: public notifier_event {
        epoc::time old_time_;
        epoc::time new_time_;
        
        std::int32_t old_time_zone_;
        std::int32_t new_time_zone_;

        std::int8_t old_security_level_;
        std::int8_t new_security_level_;

        std::optional<std::string> content_id() override {
            return std::nullopt;
        }

        bool internalize(common::ro_stream &stream) override;
    };
}
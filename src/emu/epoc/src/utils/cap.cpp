/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <epoc/utils/cap.h>
#include <algorithm>

namespace eka2l1::epoc {
    security_info::security_info(std::vector<capability> c_caps) {
        for (auto &cap: c_caps) {
            caps.set(static_cast<std::size_t>(cap));
        }
    }
    
    security_policy::security_policy(std::vector<capability> c_caps) {
        assert(c_caps.size() > 0 && c_caps.size() < 8);

        std::fill(caps, caps + 3, static_cast<std::uint8_t>(cap_none));
        std::fill(extra_caps, extra_caps + 4, static_cast<std::uint8_t>(cap_none));

        for (std::size_t i = 0; i < common::min(c_caps.size(), static_cast<std::size_t>(3)); i++) {
            caps[i] = static_cast<std::uint8_t>(c_caps[i]);
        }
        
        if (c_caps.size() <= 3) {
            type = c3;
        } else {
            type = c7;

            for (std::size_t i = 0; i < common::min(c_caps.size() - 3, static_cast<std::size_t>(4)); i++) {
                extra_caps[i] = static_cast<std::uint8_t>(c_caps[3 + i]);
            }
        }
    }

    bool security_policy::check(const security_info &against, security_info &missing) {
        bool result = false;
        missing.reset();

        missing.caps.set(caps[0]);
        missing.caps.set(caps[1]);
        missing.caps.set(caps[2]);

        missing.caps.unset(against.caps);

        switch (type) {
        case always_pass: {
            result = true;
            break;
        }

        case always_fail: {
            break;
        }

        case c7: {
            missing.caps.set(extra_caps[0]);
            missing.caps.set(extra_caps[1]);
            missing.caps.set(extra_caps[2]);
            missing.caps.set(extra_caps[3]);
            missing.caps.unset(against.caps);

            // Fallthrough so we can check if the caps is empty or not
            [[fallthrough]];
        }

        case c3: {
            if (!missing.caps.not_empty()) {
                result = true;
            }

            break;
        }

        case s3: {
            if (!missing.caps.not_empty() && sec_id == against.secure_id) {
                result = true;
            } else if (sec_id != against.secure_id) {
                missing.secure_id = sec_id;
            }

            break;
        }
        
        case v3: {
            if (!missing.caps.not_empty() && sec_id == against.vendor_id) {
                result = true;
            } else if (sec_id != against.vendor_id) {
                missing.vendor_id = sec_id;
            }

            break;
        }

        default:
            break;
        }

        return result;
    }
}
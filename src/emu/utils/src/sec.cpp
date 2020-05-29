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

#include <algorithm>
#include <utils/sec.h>

namespace eka2l1::epoc {
    security_info::security_info(std::vector<capability> c_caps) {
        reset();

        for (auto &cap : c_caps) {
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

    const char *capability_to_string(const capability &cap) {
        switch (cap) {
        case cap_all_files:
            return "AllFiles";
        case cap_comm_dd:
            return "CommDD";
        case cap_denied:
            return "Denied";
        case cap_disk_admin:
            return "DiskAdmin";
        case cap_drm:
            return "DRM";
        case cap_loc:
            return "Location";
        case cap_local_srv:
            return "LocalService";
        case cap_multimedia_dd:
            return "MultiMediaDD";
        case cap_network_control:
            return "NetworkCtrl";
        case cap_network_srv:
            return "NetworkSvr";
        case cap_power_mgmt:
            return "PowerMgmt";
        case cap_prot_serv:
            return "ProtServer";
        case cap_read_dvc_data:
            return "ReadDeviceData";
        case cap_read_user_data:
            return "ReadUserData";
        case cap_surrounding_dd:
            return "SurroundingDD";
        case cap_sw_event:
            return "SwEvent";
        case cap_tcb:
            return "TCB";
        case cap_trusted_ui:
            return "TrustedUI";
        case cap_user_env:
            return "UserEnv";
        case cap_write_dvc_data:
            return "WriteDeviceData";
        case cap_write_user_data:
            return "WriteUserData";
        default:
            break;
        }

        return "Unknown";
    }
}
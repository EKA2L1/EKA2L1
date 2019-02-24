/*
 * Copyright (c) 2001-2009 Nokia Corporation and/or its subsidiary(-ies).
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

#pragma once

#include <common/algorithm.h>
#include <common/bitfield.h>

#include <cassert>
#include <cstdint>
#include <vector>

namespace eka2l1::epoc {
    enum capability {
        /**
         * Grants write access to executables and shared read-only resources.
         *
         * This is the most critical capability as it grants access to executables and
         * therefore to their capabilities. It also grants write access to
         * the /sys and /resource directories.
         */
        cap_tcb = 0,

        /**
         * Grants direct access to all communication device drivers. This includes:
         * the EComm, Ethernet, USB device drivers etc.
         */
        cap_comm_dd = 1,

        /**
         * Grants the right:
         *
         * - to kill any process in the system
         * - to power off unused peripherals
         * - to switch the machine into standby state
         * - to wake the machine up
         * - to power the machine down completely.
         *
         * Note that this does not control access to anything and everything
         * that might drain battery power.
         */
        cap_power_mgmt = 2,

        /**
         * Grants direct access to all multimedia device drivers.
         *
         * This includes the sound, camera, video device drivers etc.
         */
        cap_multimedia_dd = 3,

        /**
         * Grants read access to network operator, phone manufacturer and device
         * confidential settings or data.
         *
         * For example, the pin lock code, the list of applications that are installed.
         */
        cap_read_dvc_data = 4,

        /**
         * Grants write access to settings that control the behaviour of the device.
         *
         * For example, device lock settings, system time, time zone, alarms, etc.
         */	
        cap_write_dvc_data = 5,

        /**
         * Grants access to protected content.
         *
         * DRM (Digital Rights Management) agents use this capability to decide whether
         * or not an application should have access to DRM content. 
         * Applications granted DRM are trusted to respect the rights associated
         * with the content.
         */	
        cap_drm = 6,

        /**
         * Grants the right to create a trusted UI session, and therefore to display
         * dialogs in a secure UI environment.
         * 
         * Trusted UI dialogs are rare. They must be used only when confidentiality
         * and security are critical; for example, for password dialogs. 
         * 
         * Normal access to the user interface and the screen does not require
         * this capability.
         */	
        cap_trusted_ui = 7,

        /**
         * Grants the right to a server to register with a protected name.
         * 
         * Currently, protected names start with a "!" character. The kernel prevents
         * servers without this capability from using such a name, and therefore
         * prevents protected servers from being impersonated.
         */	
        cap_prot_serv = 8,

        /**
         * Grants access to disk administration operations that affect more than one
         * file or one directory (or overall filesystem integrity/behaviour, etc).
         * 
         * For examples, reformatting a disk partition.
         */	
        cap_disk_admin = 9,

        /**
         * Grants the right to modify or access network protocol controls.
         * 
         * Typically when an action can change the behaviour of all existing and
         * future connections, it should be protected by this capability.
         * 
         * For example, forcing all existing connections on a specific protocol
         * to be dropped, or changing the priority of a call.
         */	
        cap_network_control = 10,

        /**
         * Grants read access to the entire file system; grants write access to
         * the private directories of other processes.
         * 
         * This capability is very strictly controlled and should rarely be granted.
         */
        cap_all_files = 11,

        /**
         * Grants the right to generate software key & pen events, and to capture any
         * of them regardless of the status of the application.
         * 
         * Note that after obtaining the focus, normal applications do not need this
         * capability to be dispatched key and pen events.
         */	
        cap_sw_event = 12,

        /**
         * A user capability that grants access to remote services without any
         * restriction on its physical location.
         * 
         * Typically, such a location is unknown to the phone user, and such services
         * may incur cost for the phone user.
         * 
         * Voice calls, SMS, and internet services are good examples of
         * such network services. They are supported by GSM, CDMA and all IP transport
         * protocols including Bluetooth profiles over IP.
         */	
        cap_network_srv = 13,

        /**
         * A user capability that grants access to remote services in the close
         * vicinity of the phone.
         *  
         * The location of the remote service is well-known to the phone user, and in
         * most cases, such services will not incur cost for the phone user.
         */	
        cap_local_srv = 14,

        /**
         * A user capability that grants read access to data that is confidential to
         * the phone user. 
         * 
         * This capability supports the management of the user's privacy.
         * 
         * Typically, contacts, messages and appointments are always seen user
         * confidential data.
         */
        cap_read_user_data = 15,

        /**
         * A user capability that grants write access to user data. 
         * 
         * This capability supports the management of the integrity of user data.
         * 
         * Note that this capability is not symmetric with the ECapabilityReadUserData
         * capability. For example, you may want to prevent rogue applications from
         * deleting music tracks but you may not want to restrict read access to them.
         */
        cap_write_user_data = 16,

        /**
         * A user capability that grants access to the location of the device.
         * 
         * This capability supports the management of the user's privacy with regard
         * to the phone location.
         */
        cap_loc = 17,

        /**
         * Grants access to logical device drivers that provide input information about
         * the surroundings of the device. 
         * 
         * Good examples of drivers that require this capability would be GPS and biometrics
         * device drivers. For complex multimedia logical device drivers that provide both
         * input and output functions, such as Sound device driver, the  MultimediaDD
         * capability should be used if it is too difficult to separate the input from the
         * output calls at its API level.
         */
        cap_surrounding_dd = 18,
      
        /**
         * Grants access to live confidential information about the user and his/her
         * immediate environment. This capability protect the user's privacy.
         * 
         * Examples are audio, picture and video recording, biometrics (such as blood
         * pressure) recording.
         * 
         * Please note that the location of the device is excluded from this capability.
         * The protection of this is achieved by using the dedicated capability Location
         */
        cap_user_env = 19,

        /**
         * Capabilities limit.
         */
        cap_limit,

        cap_none = -1,
        cap_denined = -2,

        cap_hard_limit = 255
    };

    static constexpr auto   cap_set_max_size = (cap_hard_limit + 7) >> 3;
    using                   capability_set   = common::ba_t<cap_set_max_size>;

    struct security_info {
        std::uint32_t                  secure_id;
        std::uint32_t                  vendor_id;

        static constexpr auto total_caps_u_size = cap_set_max_size / sizeof(std::uint32_t);

        union {
            common::ba_t<cap_set_max_size> caps;
            std::uint32_t                  caps_u[total_caps_u_size];
        };

        security_info()
            : secure_id(0)
            , vendor_id(0) {
        }

        security_info(std::vector<capability> c_caps);

        void reset() {
            secure_id = 0;
            vendor_id = 0;
            caps.clear();
        }
    };

    struct security_policy {
        enum sec_type {
            always_fail = 0,
            always_pass = 1,
            c3 = 2,
            c7 = 3,
            s3 = 4,
            v3 = 5
        };

        std::uint8_t type;
        std::uint8_t caps[3];

        union {
            std::uint32_t sec_id;
            std::uint32_t vendor_id;
            std::uint8_t extra_caps[4];
        };

        security_policy() = default;
        ~security_policy() = default;
        
        explicit security_policy(std::vector<capability> c_caps);

        /**
         * Check to see if the security info can pass the policy test.
         * 
         * Any required info missing will be filled in missing struct.
         */
        bool check(const security_info &against, security_info &missing);
    };
}
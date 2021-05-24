/*
 * Copyright (c) 2021 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
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

#include <package/registry.h>
#include <common/chunkyseri.h>

#include <utils/des.h>

namespace eka2l1::package {
    void package::do_state(common::chunkyseri &seri) {
        seri.absorb(uid);
        epoc::absorb_des_string(package_name, seri, true);
        epoc::absorb_des_string(vendor_name, seri, true);
        seri.absorb(index);
    }

    void file_description::do_state(common::chunkyseri &seri) {
        epoc::absorb_des_string(target, seri, true);
        epoc::absorb_des_string(mime_type, seri, true);
        seri.absorb(operation);
        seri.absorb(operation_options);

        hash.do_state(seri);

        seri.absorb(uncompressed_length);
        seri.absorb(index);
        seri.absorb(sid);
    }

    void hash_container::do_state(common::chunkyseri &seri) {
        algorithm = 1;
        seri.absorb(algorithm);
        epoc::absorb_des_string(data, seri, true);
    }

    void trust_status::do_state(common::chunkyseri &seri) {
        seri.absorb(validation_status);
        seri.absorb(revocation_status);
        seri.absorb(result_date);
        seri.absorb(last_check_date);
        seri.absorb(quarantined);
        seri.absorb(quarantined_date);
    }

    void dependency::do_state(common::chunkyseri &seri) {
        seri.absorb(uid);
        seri.absorb(from_version.u32);
        seri.absorb(from_version.u32);
        seri.absorb(from_version.u32);

        seri.absorb(to_version.u32);
        seri.absorb(to_version.u32);
        seri.absorb(to_version.u32);
    }

    void property::do_state(common::chunkyseri &seri) {
        seri.absorb(key);
        seri.absorb(value);
    }

    void controller_info::do_state(common::chunkyseri &seri) {
        seri.absorb(version.u32);
        seri.absorb(version.u32);
        seri.absorb(version.u32);
        seri.absorb(offset);
        hash.do_state(seri);
    }

    void token::do_state(common::chunkyseri &seri) {
        package::do_state(seri);
        std::uint32_t count = 0;

        seri.absorb(drives);
        seri.absorb(completely_present);

        // UIDs
        count = static_cast<std::uint32_t>(sids.size());
        seri.absorb(count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            sids.resize(count);
        }

        for (uint32_t i = 0; i < count; i++) {
            seri.absorb(sids[i]);
        }

        // Controller infos
        count = static_cast<std::uint32_t>(controller_infos.size());
        seri.absorb(count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            controller_infos.resize(count);
        }

        for (uint32_t i = 0; i < count; i++) {
            controller_infos[i].do_state(seri);
        }

        std::uint32_t major = version.major;
        std::uint32_t minor = version.minor;
        std::uint32_t build = version.build;

        seri.absorb(major);
        seri.absorb(minor);
        seri.absorb(build);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            version.major = static_cast<std::uint8_t>(major);
            version.minor = static_cast<std::uint8_t>(minor);
            version.build = static_cast<std::uint16_t>(build);
        }

        seri.absorb(language);
        seri.absorb(selected_drive);
        seri.absorb(unused1);
        seri.absorb(unused2);
    }

    void object::do_state(common::chunkyseri &seri) {
        token::do_state(seri);
        std::uint32_t count = 0;

        seri.absorb(file_major_version);
        seri.absorb(file_minor_version);

        epoc::absorb_des_string(vendor_localized_name, seri, true);
        seri.absorb(install_type);

        seri.absorb(in_rom);
        seri.absorb(deletable_preinstalled);
        seri.absorb(signed_);
        seri.absorb(trust);
        seri.absorb(remove_with_last_dependent);
        seri.absorb(trust_timestamp);

        // Dependencies
        count = static_cast<std::uint32_t>(dependencies.size());
        seri.absorb(count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            dependencies.resize(count);
        }

        for (size_t i = 0; i < count; i++) {
            dependencies[i].do_state(seri);
        }

        // Embedded packages
        count = static_cast<std::uint32_t>(embedded_packages.size());
        seri.absorb(count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            embedded_packages.resize(count);
        }
        for (size_t i = 0; i < count; i++) {
            embedded_packages[i].do_state(seri);
        }

        // Properties
        count = static_cast<std::uint32_t>(properties.size());
        seri.absorb(count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            properties.resize(count);
        }
        for (size_t i = 0; i < count; i++) {
            properties[i].do_state(seri);
        }

        // File descriptions
        count = static_cast<std::uint32_t>(file_descriptions.size());
        seri.absorb(count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            file_descriptions.resize(count);
        }

        for (size_t i = 0; i < count; i++) {
            file_descriptions[i].do_state(seri);
        }

        trust_status_value.do_state(seri);
        
        // Install chain index
        count = static_cast<std::uint32_t>(install_chain_indices.size());
        seri.absorb(count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            install_chain_indices.resize(count);
        }
        for (size_t i = 0; i < count; i++) {
            seri.absorb(install_chain_indices[i]);
        }

        seri.absorb(is_removable);
        seri.absorb(signed_by_sucert);

        if (file_major_version > 5 || ((file_major_version == 5) && (file_minor_version >= 4))) {
            count = static_cast<std::uint32_t>(supported_language_ids.size());
            seri.absorb(count);

            if (seri.get_seri_mode() == common::SERI_MODE_READ) {
                supported_language_ids.resize(count);
            }

            for (std::uint32_t i = 0; i < count; i++) {
                seri.absorb(supported_language_ids[i]);
            }

            count = static_cast<std::uint32_t>(localized_package_names.size());
            seri.absorb(count);

            if (seri.get_seri_mode() == common::SERI_MODE_READ) {
                localized_package_names.resize(count);
            }

            for (std::uint32_t i = 0; i < count; i++) {
                epoc::absorb_des_string(localized_package_names[i], seri, true);
            }

            count = static_cast<std::uint32_t>(localized_vendor_names.size());
            seri.absorb(count);

            if (seri.get_seri_mode() == common::SERI_MODE_READ) {
                localized_vendor_names.resize(count);
            }

            for (std::uint32_t i = 0; i < count; i++) {
                epoc::absorb_des_string(localized_vendor_names[i], seri, true);
            }
        }
    }
}
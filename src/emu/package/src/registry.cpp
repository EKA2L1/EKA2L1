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

        seri.absorb(count);
        for (uint32_t i = 0; i < count; i++) {
            epoc::uid uid;
            seri.absorb(uid);
        }
        seri.absorb(count);
        for (uint32_t i = 0; i < count; i++) {
            controller_info info;
            info.do_state(seri);

            controller_infos.push_back(std::move(info));
        }

        std::uint32_t major = version.major;
        std::uint32_t minor = version.minor;
        std::uint32_t build = version.build;

        seri.absorb(major);
        seri.absorb(minor);
        seri.absorb(build);
        seri.absorb(language);
        seri.absorb(selected_drive);
        seri.absorb(unused1);
        seri.absorb(unused2);
    }

    void object::do_state(common::chunkyseri &seri) {
        token::do_state(seri);
        std::uint32_t count = 0;
        
        file_major_version = 5;
        file_minor_version = 3;
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

        seri.absorb(count);
        for (size_t i = 0; i < count; i++) {
            dependency dependency;
            dependency.do_state(seri);

            dependencies.push_back(std::move(dependency));
        }
        seri.absorb(count);
        for (size_t i = 0; i < count; i++) {
            package package;
            package.do_state(seri);

            embedded_packages.push_back(std::move(package));
        }
        seri.absorb(count);
        for (size_t i = 0; i < count; i++) {
            property property;
            property.do_state(seri);

            properties.push_back(std::move(property));
        }
        seri.absorb(count);
        for (size_t i = 0; i < count; i++) {
            file_description desc;
            desc.do_state(seri);

            file_descriptions.push_back(std::move(desc));
        }

        trust_status_value.do_state(seri);
        
        seri.absorb(count);
        for (size_t i = 0; i < count; i++) {
            std::int32_t install_chain_index = 0;
            seri.absorb(install_chain_index);

            install_chain_indices.push_back(install_chain_index);
        }

        seri.absorb(is_removable);
        seri.absorb(signed_by_sucert);
    }
}
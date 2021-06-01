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

#pragma once

#include <common/uid.h>
#include <utils/version.h>
#include <utils/consts.h>

#include <cstdint>
#include <vector>
#include <string>

namespace eka2l1::common {
    class chunkyseri;
}

namespace eka2l1::package {
    enum validation_status_value {
        validation_unknown = 0,
        validation_expired = 10,
        validation_invalid = 20,
        validation_unsigned = 30,
        validation_validated = 40,
        validation_validated_to_anchor = 50,
        validation_package_in_rom = 60 
    };

    enum revocation_status_value {
        revocation_unknown2 = 0,
        revocation_ocsp_not_performed = 10,
        revocation_ocsp_revoked = 20,
        revocation_ocsp_unknown = 30,
        revocation_ocsp_transient = 40,
        revocation_ocsp_good = 50, 
    };

    enum install_type_value {
        install_type_normal_install,    // Base installation
        install_type_augmentations,     // This is basically DLC
        install_type_partial_update,    // Update to existing application
        install_type_preinstalled_app,
        install_type_preinstalled_patch
    };

    enum package_trust {
        package_trust_unsigned_or_self_signed = 0,
        package_trust_validation_failed = 50,
        package_trust_certificate_chain_no_trust_anchor = 100,
        package_trust_certificate_chain_validated_to_trust_anchor = 200,
        package_trust_chain_validated_to_trust_anchor_ocsp_transient_error = 300, 
        package_trust_chain_validated_to_trust_anchor_and_ocsp_valid = 400,
        package_trust_built_into_rom = 500
    };

    struct package {
        epoc::uid uid;
        std::u16string package_name;
        std::u16string vendor_name;
        std::int32_t index;

        void do_state(common::chunkyseri &seri);
    };

    struct trust_status {
        validation_status_value validation_status;
        revocation_status_value revocation_status;
        std::uint64_t result_date;
        std::uint64_t last_check_date;
        std::uint32_t quarantined;
        std::uint64_t quarantined_date;

        void do_state(common::chunkyseri &seri);
    };

    struct hash_container {
        std::uint32_t algorithm;
        std::string data;

        void do_state(common::chunkyseri &seri);
    };

    struct file_description {
        std::u16string target;
        std::u16string mime_type;
        hash_container hash;
        std::uint32_t operation;
        std::uint32_t operation_options;
        std::uint64_t uncompressed_length;
        std::uint32_t index;
        epoc::uid sid;
        std::string capabilities_data;

        void do_state(common::chunkyseri &seri);
    };

    struct dependency {
        epoc::uid uid;
        epoc::version from_version;
        epoc::version to_version;

        void do_state(common::chunkyseri &seri);
    };

    struct property {
        std::int32_t key;
        std::int32_t value;

        void do_state(common::chunkyseri &seri);
    };

    struct controller_info {
        epoc::version version;
        std::int32_t offset;
        hash_container hash;

        void do_state(common::chunkyseri &seri);
    };

    struct token : package {
        std::vector<epoc::uid> sids;
        std::uint32_t drives;
        std::uint32_t completely_present;
        std::uint32_t present_removable_drives;
        std::uint32_t current_drives;
        std::vector<controller_info> controller_infos;
        epoc::version version;
        std::uint32_t language;
        std::uint32_t selected_drive;
        std::int32_t unused1;
        std::int32_t unused2;
        
        void do_state(common::chunkyseri &seri);
    };

    struct object : token {
        std::u16string vendor_localized_name;
        install_type_value install_type;
        std::vector<dependency> dependencies;
        std::vector<package> embedded_packages;
        std::vector<property> properties;
        std::int32_t owned_file_descriptions;
        std::vector<file_description> file_descriptions;
        std::uint32_t in_rom;
        std::uint32_t signed_;
        std::uint32_t signed_by_sucert;
        std::uint32_t deletable_preinstalled;
        std::uint16_t file_major_version { 5 };
        std::uint16_t file_minor_version { 3 };
        package_trust trust;
        std::int32_t remove_with_last_dependent;
        std::uint64_t trust_timestamp;
        trust_status trust_status_value;
        std::vector<std::int32_t> install_chain_indices;
        std::vector<std::int32_t> supported_language_ids;
        std::vector<std::u16string> localized_package_names;
        std::vector<std::u16string> localized_vendor_names;
        std::uint32_t is_removable;

        void do_state(common::chunkyseri &seri);
        bool is_preinstalled() const {
            return (install_type == install_type_preinstalled_app) || (install_type == install_type_preinstalled_patch);
        }

        std::uint64_t total_size() const;
    };

    static constexpr const char16_t *REGISTRY_STORE_FOLDER = u"\\sys\\install\\sisregistry\\";
    static constexpr const char16_t *REGISTRY_FILE_FORMAT = u"{:08x}.reg";
    static constexpr const char16_t *CONTROLLER_FILE_FORMAT = u"{:08x}_{:04x}.ctl";
}
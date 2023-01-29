/*
 * Copyright (c) 2021 EKA2L1 Team
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

namespace eka2l1 {
    enum sisregistry_opcode {
        sisregistry_open_registry_uid = 0x0,
        sisregistry_open_registry_package = 0x1,
        sisregistry_close_registry_entry = 0x3,
        sisregistry_version = 0x5,
        sisregistry_localized_vendor_name = 0x8,
        sisregistry_package_name = 0x9,
        sisregistry_in_rom = 0xA,
        sisregistry_selected_drive = 0xD,
        sisregistry_get_matching_supported_languages = 0x10,
        sisregistry_trust_timestamp = 0x16,
        sisregistry_trust_status_op = 0x17,
        sisregistry_sid_to_filename = 0x19,
        sisregistry_shutdown_all_apps = 0x1A,
        sisregistry_package_exists_in_rom = 0x1D,
        sisregistry_stub_file_entries = 0x1E,
        sisregistry_separator_minimum_read_user_data = 0x20,
        sisregistry_installed_uids = 0x21,
        sisregistry_installed_packages = 0x22,
        sisregistry_sid_to_package = 0x23,
        sisregistry_installed_uid = 0x24,
        sisregistry_uid = 0x29,
        sisregistry_get_entry = 0x2A,
        sisregistry_sids = 0x2D,
        sisregistry_files = 0x2E,
        sisregistry_file_descriptions = 0x2F,
        sisregistry_package_augmentations = 0x30,
        sisregistry_preinstalled = 0x32,
        sisregistry_size = 0x33,
        sisregistry_package_op = 0x34,
        sisregistry_non_removable = 0x36,
        sisregistry_add_entry = 0x41,
        sisregistry_update_entry = 0x42,
        sisregistry_delete_entry = 0x43,
        // FP1
        sisregistry_dependent_packages_fp1 = 0x44,
        sisregistry_dependencies_fp1 = 0x45,
        sisregistry_embedded_packages_fp1 = 0x46,
        sisregistry_embedding_packages_fp1 = 0x47,
        sisregistry_install_type_op_fp1 = 0x48,
        sisregistry_regenerate_cache_fp1 = 0x49,
        sisregistry_deletable_preinstalled_fp1 = 0x4A,
        // FP2 and above
        sisregistry_install_type_op = 0x44,
        sisregistry_regenerate_cache = 0x45,
        sisregistry_deletable_preinstalled = 0x102,
        sisregistry_dependent_packages = 0x103,
        sisregistry_embedded_packages = 0x104,
        sisregistry_dependencies = 0x106,
        sisregistry_signed_by_sucert = 0x107
    };

    enum sisregistry_stub_extraction_mode {
        sisregistry_stub_extraction_mode_get_count,
        sisregistry_stub_extraction_mode_get_files
    };
}
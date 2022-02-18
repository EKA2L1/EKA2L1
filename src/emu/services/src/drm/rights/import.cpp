/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <services/drm/rights/import.h>

#include <common/crypt.h>
#include <common/log.h>
#include <common/pystr.h>
#include <common/time.h>

namespace eka2l1::epoc::drm {
    std::optional<rights_object> parse_dr_file(pugi::xml_document &doc) {
        rights_object final_result;
        final_result.common_data_.content_hash_.resize(20, 0);

        pugi::xml_node root = doc.child("o-ex:rights");
        if (!root) {
            return std::nullopt;
        }

        // Default to main 1, minor 0
        int main_version = 1;
        int minor_version = 0;

        pugi::xml_node grand_context_node = root.child("o-ex:context");
        if (grand_context_node) {
            pugi::xml_node version_node = grand_context_node.child("o-dd:version");
            if (version_node) {
                common::pystr version_parser(version_node.child_value());
                std::vector<common::pystr> version_strings = version_parser.split(".");

                if (version_strings.size() >= 1) {
                    main_version = version_strings[0].as_int(0, 10);
                }

                if (version_strings.size() >= 2) {
                    minor_version = version_strings[1].as_int(0, 10);
                }
            }
        }

        // Parse agreement
        pugi::xml_node agreement_node = root.child("o-ex:agreement");
        if (!agreement_node) {
            return std::nullopt;
        }

        // Parse assets
        pugi::xml_node asset_node = agreement_node.child("o-ex:asset");
        if (!asset_node) {
            return std::nullopt;
        }

        // Get content ID
        {
            pugi::xml_node context_asset_node = asset_node.child("o-ex:context");
            if (!context_asset_node) {
                return std::nullopt;
            }

            pugi::xml_node content_uid_value_node = context_asset_node.child("o-dd:uid");
            if (!content_uid_value_node) {
                return std::nullopt;
            }

            final_result.common_data_.content_id_ = content_uid_value_node.child_value();
        }

        // Get key
        {
            pugi::xml_node key_info_node = asset_node.child("ds:KeyInfo");
            if (key_info_node) {
                pugi::xml_node key_value_node = key_info_node.child("ds:KeyValue");
                if (key_value_node) {
                    const pugi::char_t *key_in_base64 = key_value_node.child_value();
                    if (strlen(key_in_base64) != 24) {
                        LOG_ERROR(SERVICE_DRMSYS, "Key in DR file is not 24 base64 string!");
                    } else {
                        final_result.encrypt_key_.resize(16);
                        crypt::base64_decode(reinterpret_cast<const std::uint8_t*>(key_in_base64), 24, final_result.encrypt_key_.data(), 16);
                    }
                }
            }
        }

        // Permission is optional
        for (pugi::xml_node &perm_node: agreement_node.children("o-ex:permission")) {
            rights_permission perm;
            perm.version_rights_main_ = main_version;
            perm.version_rights_sub_ = minor_version;
            perm.insert_time_ = common::get_current_utc_time_in_microseconds_since_0ad();

            for (pugi::xml_node &section_node: perm_node.children()) {
                // Check if we can fill in any section
                rights_type the_type = rights_type_none;
                rights_constraint *the_constraint = nullptr;

                if (strcmp(section_node.name(), "o-dd:play") == 0) {
                    the_type = rights_type_play;
                    the_constraint = &perm.play_constraint_;
                } else if (strcmp(section_node.name(), "o-dd:display") == 0) {
                    the_type = rights_type_display;
                    the_constraint = &perm.display_constraint_;
                } else if (strcmp(section_node.name(), "o-dd:execute") == 0) {
                    the_type = rights_type_execute;
                    the_constraint = &perm.execute_constraint_;
                } else if (strcmp(section_node.name(), "o-dd:print") == 0) {
                    the_type = rights_type_print;
                    the_constraint = &perm.print_constraint_;
                } else if (strcmp(section_node.name(), "o-dd:export") == 0) {
                    the_type = rights_type_export;
                    the_constraint = &perm.export_contraint_;
                }

                if (!the_constraint) {
                    continue;
                }

                for (pugi::xml_node &constraint_root: section_node.children("o-ex:constraint")) {
                    // Search for properties
                    for (pugi::xml_node &container_node: constraint_root.children("o-ex:container")) {
                        for (pugi::xml_node &software_node: container_node.children("o-dd:software")) {
                            for (pugi::xml_node &context_node: software_node.children("o-ex:context")) {
                                for (pugi::xml_node &property_node: context_node.children("property")) {
                                    pugi::xml_attribute attrib = property_node.attribute("schema");
                                    if (strcmp(attrib.value(), "symbianvid") == 0) {
                                        the_constraint->vendor_id_ = common::pystr(property_node.child_value()).as_int<std::uint32_t>(0, 16);
                                        the_constraint->active_constraints_ |= rights_constraint_vendor;
                                    } else if (strcmp(attrib.value(), "symbiansid") == 0) {
                                        the_constraint->secure_id_ = common::pystr(property_node.child_value()).as_int<std::uint32_t>(0, 16);
                                        the_constraint->active_constraints_ |= rights_constraint_software;
                                    }
                                }
                            }
                        }
                    }
                }

                perm.available_rights_ |= the_type;
            }

            final_result.permissions_.push_back(perm);
        }

        return final_result;
    }
}
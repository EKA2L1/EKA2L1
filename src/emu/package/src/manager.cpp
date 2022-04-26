/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>

#include <loader/e32img.h>
#include <loader/sis.h>

#include <config/config.h>

#include <package/manager.h>
#include <package/sis_script_interpreter.h>
#include <package/sis_v1_installer.h>
#include <vfs/vfs.h>

#include <fstream>
#include <yaml-cpp/yaml.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

namespace eka2l1 {
    namespace manager {
        static constexpr const char *APP_REGISTRY_FILENAME = "apps_registry.yml";
        static constexpr const char *PACKAGE_FOLDER_PATH = "packages";

        packages::packages(io_system *io, config::state *conf, const drive_number residing)
            : residing_(residing)
            , sys(io)
            , conf(conf) {
        }

        static std::string get_bucket_stream_path(config::state *state, const uid package_uid) {
            return add_path(state->storage, add_path(PACKAGE_FOLDER_PATH, common::to_string(package_uid, std::hex) + ".txt"));
        }

        static std::u16string get_virtual_registry_parent_folder(const drive_number residing) {
            return std::u16string(1, drive_to_char16(residing)) + u":" + package::REGISTRY_STORE_FOLDER;
        }

        static std::u16string get_virtual_registry_folder(const drive_number residing, const uid package_uid) {
            std::u16string package_uid_folder = common::utf8_to_ucs2(fmt::format("{:08x}", package_uid)) + u"\\";
            return get_virtual_registry_parent_folder(residing) + package_uid_folder;
        }

        static std::u16string get_virtual_registry_regfile(const drive_number residing, const uid package_uid, const std::uint32_t index) {
            return get_virtual_registry_folder(residing, package_uid) + fmt::format(package::REGISTRY_FILE_FORMAT, index);
        }

        void packages::install_sis_stubs() {
            static constexpr const char16_t *STUB_SIS_DIRECTORY = u"{}:\\system\\install\\";

            for (drive_number drv = drive_z; drv >= drive_a; drv--) {
                if (sys->get_drive_entry(drv)) {
                    const std::u16string stub_directory = fmt::format(STUB_SIS_DIRECTORY, drive_to_char16(drv));
                    std::unique_ptr<directory> stub_dir_iterator = sys->open_dir(stub_directory, {}, io_attrib_include_file);

                    if (stub_dir_iterator) {
                        while (std::optional<entry_info> stub_file_info = stub_dir_iterator->get_next_entry()) {
                            auto stub_file_real_path = sys->get_raw_path(common::utf8_to_ucs2(stub_file_info->full_path));
                            if (stub_file_real_path.has_value()) {
                                install_package(stub_file_real_path.value(), drv, nullptr, nullptr, true);
                            }
                        }
                    }
                }
            }
        }

        void packages::load_registries() {
            static constexpr const char16_t *STUB_CACHED_PATH_FORMAT = u"{}:\\stubcached";
            const std::u16string stub_cached_path = fmt::format(STUB_CACHED_PATH_FORMAT, drive_to_char16(drive_z));

            std::unique_ptr<directory> registry_dir = sys->open_dir(get_virtual_registry_parent_folder(residing_), {}, io_attrib_include_dir);
            if (!registry_dir) {
                LOG_INFO(PACKAGE, "Registry folder is unavailable!");
            } else {
                while (std::optional<entry_info> registry_folder_ent = registry_dir->get_next_entry()) {
                    if (registry_folder_ent->type == io_component_type::dir) {
                        std::unique_ptr<directory> registry_package_dir = sys->open_dir(add_path(common::utf8_to_ucs2(registry_folder_ent->full_path), u"*.reg"), {},
                            io_attrib_include_file);

                        while (std::optional<entry_info> registry_package_ent = registry_package_dir->get_next_entry()) {
                            if (registry_package_ent->type == io_component_type::file) {
                                const std::u16string final_reg_path = common::utf8_to_ucs2(registry_package_ent->full_path);
                                symfile registry_file = sys->open_file(final_reg_path, READ_MODE | BIN_MODE);

                                if (registry_file) {
                                    static constexpr const std::size_t MAXIMUM_REGISTRY_SIZE = common::MB(2);
                                    if (registry_file->size() > MAXIMUM_REGISTRY_SIZE) {
                                        LOG_ERROR(PACKAGE, "Registry file is too large (path: {})!", common::ucs2_to_utf8(final_reg_path));
                                        continue;
                                    }

                                    std::vector<std::uint8_t> registry_buffer;
                                    registry_buffer.resize(registry_file->size());

                                    if (registry_file->read_file(registry_buffer.data(), static_cast<std::uint32_t>(registry_buffer.size()), 1) != static_cast<std::uint32_t>(registry_buffer.size())) {
                                        LOG_ERROR(PACKAGE, "Registry file can not be fully loaded! (path: {})!", common::ucs2_to_utf8(final_reg_path));
                                    }

                                    common::chunkyseri seri(registry_buffer.data(), registry_buffer.size(), common::SERI_MODE_READ);

                                    package::object final_obj;
                                    final_obj.do_state(seri);

                                    objects_.emplace(final_obj.uid, std::move(final_obj));
                                }
                            }
                        }
                    }
                }
            }

            if (!sys->exist(stub_cached_path)) {
                install_sis_stubs();

                // Life hacks sorry guys
                auto stub_cached_real_path = sys->get_raw_path(stub_cached_path);
                if (stub_cached_real_path.has_value()) {
                    FILE *f = common::open_c_file(common::ucs2_to_utf8(stub_cached_real_path.value()), "wb+");
                    fclose(f);
                }
            }
        }

        void packages::migrate_legacy_registries() {
            const std::string app_registry_file_path = add_path(conf->storage, APP_REGISTRY_FILENAME);
            if (!common::exists(app_registry_file_path)) {
                return;
            }

            common::ro_std_file_stream app_registry_stream(app_registry_file_path, true);
            if (!app_registry_stream.valid()) {
                return;
            }

            std::string whole_config(app_registry_stream.size(), ' ');
            app_registry_stream.read(whole_config.data(), whole_config.size());

            YAML::Node sdb_node = YAML::Load(whole_config);

            for (const auto &maybe_app : sdb_node) {
                const YAML::Node app = maybe_app.second;
                package::object obj;

                drive_number residing = static_cast<drive_number>(app["drive"].as<int>());

                obj.drives = 1 << (residing - drive_a);
                obj.uid = app["uid"].as<manager::uid>();
                obj.package_name = common::utf8_to_ucs2(app["name"].as<std::string>());
                obj.vendor_name = common::utf8_to_ucs2(app["vendor"].as<std::string>());

                obj.version.major = 1;
                obj.version.minor = 0;
                obj.version.build = 0;

                obj.is_removable = true;
                obj.file_major_version = 5;
                obj.file_minor_version = 4;

                // Get the file bucket
                obj.signed_ = 1;
                obj.in_rom = false;
                obj.supported_language_ids.push_back(static_cast<std::int32_t>(language::en));

                const std::string bucket_path = get_bucket_stream_path(conf, obj.uid);
                if (common::exists(bucket_path)) {
                    std::ifstream bucket_stream(bucket_path);
                    std::string virtual_path;

                    while (std::getline(bucket_stream, virtual_path)) {
                        std::optional<entry_info> finfo = sys->get_entry_info(common::utf8_to_ucs2(virtual_path));

                        package::file_description desc;
                        desc.operation = static_cast<std::int32_t>(loader::ss_op::install);
                        desc.operation_options = 0;
                        desc.uncompressed_length = finfo->size;
                        desc.target = common::utf8_to_ucs2(virtual_path);
                        desc.sid = 0;
                        desc.index = static_cast<std::int32_t>(obj.file_descriptions.size());

                        obj.file_descriptions.push_back(desc);
                    }
                }

                add_package(obj, nullptr);
            }

            common::delete_folder(add_path(conf->storage, std::string(PACKAGE_FOLDER_PATH) + get_separator()));
            common::remove(app_registry_file_path);
        }

        package::object *packages::package(const uid app_uid, std::int32_t index) {
            auto ite_range = objects_.equal_range(app_uid);
            for (auto ite = ite_range.first; ite != ite_range.second; ite++) {
                if (ite->second.index == index) {
                    return &(ite->second);
                }
            }

            return nullptr;
        }

        package::object *packages::package(const uid app_uid, const std::u16string package_name, const std::u16string vendor_name) {
            auto ite_range = objects_.equal_range(app_uid);
            for (auto ite = ite_range.first; ite != ite_range.second; ite++) {
                if ((package_name.empty() || (package_name == ite->second.package_name)) && (vendor_name.empty() || (vendor_name == ite->second.vendor_name))) {
                    return &(ite->second);
                }
            }

            return nullptr;
        }

        std::vector<package::object *> packages::augmentations(const uid app_uid) {
            std::vector<package::object *> results;

            auto ite_range = objects_.equal_range(app_uid);
            for (auto ite = ite_range.first; ite != ite_range.second; ite++) {
                if (ite->second.install_type == package::install_type_augmentations) {
                    results.push_back(&(ite->second));
                }
            }

            return results;
        }

        std::vector<package::object *> packages::dependents(const uid app_uid) {
            std::vector<package::object *> results;

            auto ite_range = objects_.equal_range(app_uid);
            for (auto ite = ite_range.first; ite != ite_range.second; ite++) {
                if (ite->second.index != 0) {
                    results.push_back(&(ite->second));
                }
            }

            return results;
        }

        std::vector<uid> packages::installed_uids() const {
            std::vector<uid> uniques;

            for (auto ite = objects_.begin(), end = objects_.end(); ite != end; ite = objects_.upper_bound(ite->first))
                uniques.push_back(ite->first);

            return uniques;
        }

        bool packages::save_package(package::object &pkg) {
            bool result = true;

            std::u16string residing_folder = get_virtual_registry_folder(residing_, pkg.uid);
            sys->create_directories(residing_folder);

            std::vector<std::uint8_t> serialized_object_buffer;
            common::chunkyseri object_buffer_serializer(nullptr, 0, common::SERI_MODE_MEASURE);

            pkg.do_state(object_buffer_serializer);

            serialized_object_buffer.resize(object_buffer_serializer.size());
            object_buffer_serializer = common::chunkyseri(serialized_object_buffer.data(), serialized_object_buffer.size(), common::SERI_MODE_WRITE);

            pkg.do_state(object_buffer_serializer);

            // Write this file out
            {
                const std::u16string registry_path = add_path(residing_folder, fmt::format(package::REGISTRY_FILE_FORMAT, pkg.index));
                symfile registry_file = sys->open_file(registry_path, WRITE_MODE | BIN_MODE);

                if (registry_file) {
                    if (registry_file->write_file(serialized_object_buffer.data(), static_cast<std::uint32_t>(serialized_object_buffer.size()), 1) != static_cast<std::uint32_t>(serialized_object_buffer.size())) {
                        LOG_ERROR(PACKAGE, "Unable to fully write registry data to file!");
                        result = false;
                    }

                    registry_file->close();
                } else {
                    result = false;
                }
            }

            return result;
        }

        bool packages::add_package(package::object &pkg, const controller_info *controller_info) {
            const bool is_installed = installed(pkg.uid);

            if (is_installed && (pkg.install_type == package::install_type_normal_install)) {
                return false;
            }

            const std::lock_guard<std::mutex> guard(lockdown);
            const bool no_new_package = is_installed && (pkg.install_type == package::install_type_partial_update);
            auto ite_range = objects_.equal_range(pkg.uid);

            std::int32_t ctrl_offset = 0;
            package::object *base_package = &pkg;

            switch (pkg.install_type) {
            case package::install_type_normal_install:
                pkg.index = 0;
                break;

            case package::install_type_partial_update:
                if (no_new_package) {
                    pkg.index = -1;
                    base_package = package(pkg.uid);
                }

                break;

            default: {
                static constexpr std::int32_t MAX_INDEX = 100000;
                for (std::int32_t i = 0; i < MAX_INDEX; i++) {
                    bool overlapped = false;
                    for (auto ite = ite_range.first; ite != ite_range.second; ite++) {
                        if (ite->second.index == i) {
                            overlapped = true;
                            break;
                        }
                    }

                    if (!overlapped) {
                        pkg.index = i;
                        break;
                    }
                }

                break;
            }
            }

            if (!pkg.controller_infos.empty() && no_new_package) {
                // Find free index
                static constexpr std::int32_t MAX_INDEX = 100000;
                for (std::size_t m = 0; m < pkg.controller_infos.size(); m++) {
                    for (std::int32_t i = 0; i < MAX_INDEX; i++) {
                        bool overlapped = false;
                        for (const auto &controller_info : base_package->controller_infos) {
                            if (controller_info.offset == i) {
                                overlapped = true;
                                break;
                            }
                        }

                        if (!overlapped) {
                            ctrl_offset = i;
                            pkg.controller_infos[m].offset = i;

                            break;
                        }
                    }
                }
            }

            if (no_new_package) {
                package::package me_as_embed;
                me_as_embed.uid = pkg.uid;
                me_as_embed.package_name = pkg.package_name;
                me_as_embed.vendor_name = pkg.vendor_name;
                me_as_embed.index = pkg.index;

                base_package->embedded_packages.push_back(std::move(pkg));
                base_package->drives |= pkg.drives;
                base_package->current_drives |= pkg.current_drives;
                base_package->language |= pkg.language;

                base_package->properties.insert(base_package->properties.end(), pkg.properties.begin(), pkg.properties.end());
                base_package->dependencies.insert(base_package->dependencies.end(), pkg.dependencies.begin(), pkg.dependencies.end());
                base_package->controller_infos.insert(base_package->controller_infos.end(), pkg.controller_infos.begin(), pkg.controller_infos.end());

                // Also add the files in
                for (std::size_t i = 0; i < pkg.file_descriptions.size(); i++) {
                    package::file_description new_desc = pkg.file_descriptions[i];
                    new_desc.index = static_cast<std::uint32_t>(base_package->file_descriptions.size() + i);

                    base_package->file_descriptions.push_back(std::move(new_desc));
                }

                if (!save_package(*base_package)) {
                    LOG_ERROR(PACKAGE, "Unable to write package info for 0x{:X}", base_package->uid);
                }
            } else {
                if (!save_package(pkg)) {
                    LOG_ERROR(PACKAGE, "Unable to write package info for 0x{:X}", pkg.uid);
                }
            }

            if (controller_info) {
                const std::u16string residing_folder = get_virtual_registry_folder(residing_, pkg.uid);
                const std::u16string controller_path = add_path(residing_folder, fmt::format(package::CONTROLLER_FILE_FORMAT, base_package->index, ctrl_offset));

                symfile controller_file = sys->open_file(controller_path, WRITE_MODE | BIN_MODE);

                if (controller_file) {
                    std::uint32_t file_size = static_cast<std::uint32_t>(controller_info->size_);
                    if (controller_file->write_file(&file_size, sizeof(file_size), 1) != sizeof(file_size)) {
                        LOG_ERROR(PACKAGE, "Unable to write size of controller to controller file!");
                    }

                    if (controller_file->write_file(controller_info->data_, static_cast<std::uint32_t>(controller_info->size_), 1) != static_cast<std::uint32_t>(controller_info->size_)) {
                        LOG_ERROR(PACKAGE, "Unable to write controller data to file!");
                    }

                    controller_file->close();
                }
            }

            if (!no_new_package)
                objects_.emplace(pkg.uid, std::move(pkg));

            return true;
        }

        bool packages::remove_registeration(package::object &pkg) {
            auto obj_ite_range = objects_.equal_range(pkg.uid);

            auto pkg_ite = obj_ite_range.first;
            bool found = false;

            for (pkg_ite; pkg_ite != obj_ite_range.second; pkg_ite++) {
                if ((pkg_ite->second.uid == pkg.uid) && (pkg_ite->second.index == pkg.index)) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                return false;
            }

            // Delete registry file
            const std::u16string vpath = get_virtual_registry_regfile(residing_, pkg.uid, pkg.index);

            // Delete associated controllers
            for (std::size_t i = 0; i < pkg.controller_infos.size(); i++) {
                const std::u16string ctrl_path = add_path(get_virtual_registry_folder(residing_, pkg.uid),
                    fmt::format(package::CONTROLLER_FILE_FORMAT, pkg.index, pkg.controller_infos[i].offset));

                sys->delete_entry(ctrl_path);
            }

            // Remove the object
            objects_.erase(pkg_ite);

            if (objects_.find(pkg.uid) == objects_.end()) {
                std::u16string the_reg_path = get_virtual_registry_folder(residing_, pkg.uid);
                if (std::optional<std::u16string> real_reg_path = sys->get_raw_path(the_reg_path)) {
                    common::delete_folder(common::ucs2_to_utf8(real_reg_path.value()));
                }
            }

            return true;
        }

        bool packages::uninstall_package(package::object &pkg) {
            auto obj_ite_range = objects_.equal_range(pkg.uid);

            auto pkg_ite = obj_ite_range.first;
            bool found = false;

            for (pkg_ite; pkg_ite != obj_ite_range.second; pkg_ite++) {
                if ((pkg_ite->second.uid == pkg.uid) && (pkg_ite->second.index == pkg.index)) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                return false;
            }

            // Delete files as requested by objects
            for (const package::file_description &desc : pkg.file_descriptions) {
                if ((desc.operation == static_cast<int>(loader::ss_op::install)) || (desc.operation == static_cast<int>(loader::ss_op::null))) {
                    sys->delete_entry(desc.target);
                }
            }

            return remove_registeration(pkg);
        }

        bool packages::installed(uid app_uid) {
            const std::lock_guard<std::mutex> guard(lockdown);
            auto ite_range = objects_.equal_range(app_uid);

            for (auto ite = ite_range.first; ite != ite_range.second; ite++) {
                if (ite->second.install_type == package::install_type_normal_install) {
                    return true;
                }
            }

            return false;
        }

        void packages::traverse_tree_and_add_packages(loader::sis_registry_tree &tree) {
            // TODO: We should ask for user permission first! This is also not correct
            // Just remove that registeration, not the files...
            if (installed(tree.package_info.uid)) {
                package::object *obj = package(tree.package_info.uid);
                remove_registeration(*obj);
            }

            add_package(tree.package_info, &tree.controller_binary);
            for (std::size_t i = 0; i < tree.embeds.size(); i++) {
                traverse_tree_and_add_packages(tree.embeds[i]);
            }
        }

        package::installation_result packages::install_package(const std::u16string &path, const drive_number drive, progress_changed_callback progress_cb, cancel_requested_callback cancel_cb, const bool silent) {
            std::optional<loader::sis_type> sis_ver = loader::identify_sis_type(common::ucs2_to_utf8(path));

            if (!sis_ver) {
                return package::installation_result_invalid;
            }

            if (sis_ver.value() != loader::sis_type_old) {
                loader::sis_contents res = loader::parse_sis(common::ucs2_to_utf8(path), sis_ver == loader::sis_type_new_stub);
                common::ro_std_file_stream stream(common::ucs2_to_utf8(path), true);

                // Interpret the file
                loader::ss_interpreter interpreter(reinterpret_cast<common::ro_stream *>(&stream), sys, this, &res.controller, &res.data, drive);

                // Set up hooks
                if (show_text && !silent) {
                    interpreter.show_text = show_text;
                }

                if (choose_lang && !silent) {
                    interpreter.choose_lang = choose_lang;
                }

                if (var_resolver) {
                    interpreter.var_resolver = var_resolver;
                }

                std::unique_ptr<loader::sis_registry_tree> new_infos = interpreter.interpret(progress_cb, cancel_cb);

                if (new_infos) {
                    traverse_tree_and_add_packages(*new_infos);

                    for (const auto &another_path : interpreter.extra_sis_files()) {
                        install_package(another_path, drive, progress_cb, cancel_cb);
                    }
                } else {
                    return package::installation_result_aborted;
                }
            } else {
                package::object final_obj;
                final_obj.file_major_version = 5;
                final_obj.file_minor_version = 4;

                if (!loader::install_sis_old(path, sys, drive, final_obj, choose_lang, var_resolver, progress_cb, cancel_cb)) {
                    return package::installation_result_invalid;
                }

                add_package(final_obj, nullptr);
            }

            LOG_TRACE(PACKAGE, "Installation done!");
            return package::installation_result_success;
        }
    }
}

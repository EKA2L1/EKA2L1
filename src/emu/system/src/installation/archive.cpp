/*
 * Copyright (c) 2026 EKA2L1 Team
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

#include <common/path_types.h>
#include <system/devices.h>
#include <system/installation/archive.h>
#include <system/installation/firmware.h>
#include <system/installation/rpkg.h>
#include <system/software.h>

#include <common/algorithm.h>
#include <common/archive.h>
#include <common/buffer.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>

#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace eka2l1::loader {
    static const char *Z_DRIVE_MARKER = "drives/z/";

    // Progress is handed out on a fixed scale, because the work is a mix of things measured in bytes and
    // things measured in files.
    static constexpr std::size_t ARCHIVE_PROGRESS_TOTAL = 1000;

    struct archive_device_layout {
        enum kind {
            unknown,
            // A ROM image, plus an RPKG if the archive carries one.
            rom_and_rpkg,
            // An already-unpacked device: drive Z tree plus the ROM image.
            data_dump,
            // Nokia flash files: one or more VPLs next to the FPSX images they list.
            firmware
        };

        kind type = unknown;

        // Index into the entry list of the image that becomes SYM.ROM. Set for both layouts.
        std::size_t rom_index = 0;

        // rom_and_rpkg. Index into the entry list.
        std::size_t rpkg_index = 0;
        bool has_rpkg = false;

        // data_dump. Lowercased, ends with '/'.
        std::string z_prefix;
        std::vector<std::size_t> z_files;
        // Everything sitting in the dump's ROM folder; `rom_index` is the one picked out of it.
        std::vector<std::size_t> rom_files;
        // The pack's own devices.yml, when it ships one.
        std::size_t devices_yml_index = 0;
        bool has_devices_yml = false;

        // firmware. Indices into the entry list.
        std::vector<std::size_t> vpl_files;
    };

    // A flash package is a few KB of VPL; anything far bigger is not one.
    static constexpr std::uint64_t MAX_VPL_SIZE = 4 * 1024 * 1024;

    static bool has_extension(const std::string &lowercased_path, const char *dotted_extension) {
        const std::size_t ext_len = std::strlen(dotted_extension);

        return (lowercased_path.size() > ext_len)
            && (lowercased_path.compare(lowercased_path.size() - ext_len, ext_len, dotted_extension) == 0);
    }

    /**
     * @brief Look for the drive Z tree of an unpacked device and work out what it is rooted at.
     *
     * The marker is `drives/z/<firmware code>/`, which may sit at any depth: packs usually wrap it in a
     * `data/` folder, sometimes inside another folder named after the phone. The returned prefix covers
     * everything up to and including the firmware code, so stripping it off an entry gives the path
     * relative to drive Z's root.
     */
    static bool find_z_drive_prefix(const std::vector<common::archive_entry_info> &entries,
        std::string &prefix_out, std::string &code_out) {
        for (const common::archive_entry_info &entry : entries) {
            const std::string lowered = common::lowercase_string(entry.path);
            const std::size_t marker_pos = lowered.find(Z_DRIVE_MARKER);

            // Must start a path component, else a folder called "mydrives" would match.
            if ((marker_pos == std::string::npos) || ((marker_pos != 0) && (lowered[marker_pos - 1] != '/'))) {
                continue;
            }

            const std::size_t code_pos = marker_pos + std::strlen(Z_DRIVE_MARKER);
            const std::size_t code_end = lowered.find('/', code_pos);

            // The firmware code folder itself is stored without a trailing separator.
            const std::string code = (code_end == std::string::npos)
                ? lowered.substr(code_pos)
                : lowered.substr(code_pos, code_end - code_pos);

            if (code.empty()) {
                continue;
            }

            prefix_out = lowered.substr(0, code_pos) + code + "/";
            code_out = code;

            return true;
        }

        return false;
    }

    /**
     * @brief Pick the image to install out of everything found in a dump's ROM folder.
     *
     * The folder is not always just the one file: N-Gage packs keep the raw sections the ROM was
     * assembled from (`BOOT-<uid>.dmp`, `ROOT-<uid>.dmp`) next to `SYM.ROM`, and the boot section is an
     * 8 KB block that parses into a ROM with no root directory at all. Prefer the name the emulator
     * writes itself, then a .rom extension, and only then fall back to the largest file.
     */
    static std::size_t pick_rom_file(const std::vector<common::archive_entry_info> &entries,
        const std::vector<std::size_t> &candidates) {
        std::size_t best = candidates.front();
        int best_rank = -1;

        for (const std::size_t index : candidates) {
            const std::string name = common::lowercase_string(eka2l1::filename(entries[index].path, false));
            const int rank = (name == "sym.rom") ? 2 : (has_extension(name, ".rom") ? 1 : 0);

            if ((rank > best_rank) || ((rank == best_rank) && (entries[index].size > entries[best].size))) {
                best = index;
                best_rank = rank;
            }
        }

        return best;
    }

    static archive_device_layout determine_layout(const std::vector<common::archive_entry_info> &entries) {
        archive_device_layout layout;

        std::string z_prefix;
        std::string code;

        if (find_z_drive_prefix(entries, z_prefix, code)) {
            // `data/drives/z/<code>/` -> `data/roms/<code>/`. Anything the pack keeps elsewhere (drive C
            // contents, readme files, its own devices.yml) is left alone: drive C may be shared between
            // installed devices, so writing into it would be a side effect the user never asked for.
            const std::string data_prefix = z_prefix.substr(0, z_prefix.find(Z_DRIVE_MARKER));
            const std::string rom_prefix = data_prefix + "roms/" + code + "/";
            const std::string devices_yml_path(data_prefix + "devices.yml");

            for (std::size_t i = 0; i < entries.size(); i++) {
                if (entries[i].is_directory) {
                    continue;
                }

                const std::string lowered = common::lowercase_string(entries[i].path);

                if (lowered.compare(0, z_prefix.size(), z_prefix) == 0) {
                    layout.z_files.push_back(i);
                } else if (lowered.compare(0, rom_prefix.size(), rom_prefix) == 0) {
                    layout.rom_files.push_back(i);
                } else if (lowered == devices_yml_path) {
                    layout.devices_yml_index = i;
                    layout.has_devices_yml = true;
                }
            }

            if (layout.rom_files.empty()) {
                // Some packs drop the ROM beside the tree instead of into roms/. A lone .rom outside
                // drive Z is a better guess than giving up.
                for (std::size_t i = 0; i < entries.size(); i++) {
                    const std::string lowered = common::lowercase_string(entries[i].path);

                    if (!entries[i].is_directory && has_extension(lowered, ".rom")
                        && (lowered.compare(0, z_prefix.size(), z_prefix) != 0)) {
                        layout.rom_files.push_back(i);
                    }
                }
            }

            if (!layout.z_files.empty() && !layout.rom_files.empty()) {
                layout.type = archive_device_layout::data_dump;
                layout.z_prefix = z_prefix;
                layout.rom_index = pick_rom_file(entries, layout.rom_files);

                return layout;
            }

            LOG_WARN(SYSTEM, "Archive has a drive Z tree for {} but no ROM image to go with it", code);
        }

        for (std::size_t i = 0; i < entries.size(); i++) {
            if (!entries[i].is_directory && has_extension(common::lowercase_string(entries[i].path), ".vpl")) {
                layout.vpl_files.push_back(i);
            }
        }

        if (!layout.vpl_files.empty()) {
            layout.type = archive_device_layout::firmware;
            return layout;
        }

        bool rom_found = false;

        for (std::size_t i = 0; i < entries.size(); i++) {
            if (entries[i].is_directory) {
                continue;
            }

            const std::string lowered = common::lowercase_string(entries[i].path);

            // Prefer the biggest candidate of each kind: a pack can also ship a small stub ROM for some
            // tool, and the real image is always the large one.
            if (has_extension(lowered, ".rom")) {
                if (!rom_found || (entries[i].size > entries[layout.rom_index].size)) {
                    layout.rom_index = i;
                    rom_found = true;
                }
            } else if (has_extension(lowered, ".rpkg")) {
                if (!layout.has_rpkg || (entries[i].size > entries[layout.rpkg_index].size)) {
                    layout.rpkg_index = i;
                    layout.has_rpkg = true;
                }
            }
        }

        if (rom_found) {
            layout.type = archive_device_layout::rom_and_rpkg;
        }

        return layout;
    }

    /**
     * @brief Unpack the entries a plan asks for.
     *
     * `destinations` is parallel to the entry list: a non-empty string is where that entry goes, an empty
     * one means skip. The archive can only be read front to back (a member of a solid .7z is only
     * reachable by decoding everything ahead of it), so the plan is consulted by position as the reader
     * walks past - which is exactly the order the entries were listed in.
     *
     * @param progress_ceiling Where on the shared 0..ARCHIVE_PROGRESS_TOTAL scale unpacking should land
     *                         when it finishes; the caller owns whatever is left for its own work.
     */
    static bool extract_planned(const host_path &archive_path, const std::vector<host_path> &destinations,
        const std::size_t progress_ceiling, progress_changed_callback progress_cb,
        cancel_requested_callback cancel_cb, const host_path &case_preserving_root = {}) {
        std::size_t next_index = 0;

        progress_changed_callback wrapped_cb = nullptr;

        if (progress_cb) {
            wrapped_cb = [progress_cb, progress_ceiling](const std::size_t done, const std::size_t total) {
                if (!total) {
                    return;
                }

                progress_cb(done * progress_ceiling / total, ARCHIVE_PROGRESS_TOTAL);
            };
        }

        return common::extract_archive(archive_path,
            [&](const common::archive_entry_info &) -> host_path {
                const std::size_t index = next_index++;
                if (index >= destinations.size()) {
                    return {};
                }
                const host_path &destination = destinations[index];
                if (!case_preserving_root.empty()
                    && destination.utf8().starts_with(case_preserving_root.utf8())) {
                    return case_preserving_root.resolve_guest(eka2l1::guest_path::from_utf8(destination.utf8().substr(case_preserving_root.size())));
                }
                return destination;
            },
            wrapped_cb, cancel_cb);
    }

    static device_installation_error install_archive_rom_and_rpkg(device_manager *dvcmngr,
        const host_path &archive_path, const std::vector<common::archive_entry_info> &entries,
        const archive_device_layout &layout, const host_path &rom_resident_path,
        const host_path &drives_z_resident_path, const bool isolate_drives, progress_changed_callback progress_cb,
        cancel_requested_callback cancel_cb) {
        // The ROM and the RPKG are read several times over by the installers (the ROM is parsed, then
        // copied; the RPKG is streamed through), so they are unpacked to a scratch folder first rather
        // than being decompressed again for every pass.
        const host_path staging = add_path(rom_resident_path, "temparchive\\");
        common::delete_folder(staging);

        std::vector<host_path> destinations(entries.size());

        const host_path rom_path(add_path(staging, eka2l1::filename(entries[layout.rom_index].path, false)));
        destinations[layout.rom_index] = rom_path;

        host_path rpkg_path;

        if (layout.has_rpkg) {
            rpkg_path = add_path(staging, eka2l1::filename(entries[layout.rpkg_index].path, false));
            destinations[layout.rpkg_index] = rpkg_path;
        }

        // Unpacking is only half the story here: the ROM/RPKG installer still has to dump drive Z out of
        // what we just wrote, so the two get half the bar each.
        if (!extract_planned(archive_path, destinations, ARCHIVE_PROGRESS_TOTAL / 2, progress_cb, cancel_cb)) {
            common::delete_folder(staging);

            return (cancel_cb && cancel_cb()) ? device_installation_general_failure
                                              : device_installation_archive_corrupt;
        }

        progress_changed_callback install_cb = nullptr;

        if (progress_cb) {
            install_cb = [progress_cb](const std::size_t done, const std::size_t total) {
                if (!total) {
                    return;
                }

                progress_cb(ARCHIVE_PROGRESS_TOTAL / 2 + done * ARCHIVE_PROGRESS_TOTAL / total / 2,
                    ARCHIVE_PROGRESS_TOTAL);
            };
        }

        const device_installation_error result = install_rom_with_optional_rpkg(dvcmngr, rom_path, rpkg_path,
            rom_resident_path, drives_z_resident_path, isolate_drives, install_cb, cancel_cb);

        common::delete_folder(staging);

        return result;
    }

    /**
     * @brief Take the display name a pack states for a device over the one probed out of its dump.
     *
     * A dump's own product.txt can carry a factory string rather than a name (an N85's says "N00"), and
     * whoever assembled the pack usually wrote down the real one. Only the presentation fields are taken:
     * the firmware code stays whatever the dump says, since that is what every path on disk is named
     * after, and the Symbian version stays with the probe, which reads the same files the emulator will.
     */
    static void apply_packaged_device_name(const host_path &devices_yml_path, const std::string &firmcode,
        std::string &manufacturer, std::string &model, std::uint32_t &machine_uid) {
        common::ro_std_file_stream stream(eka2l1::host_path{devices_yml_path}, true);

        if (!stream.valid()) {
            return;
        }

        std::string contents(static_cast<std::size_t>(stream.size()), ' ');
        stream.read(contents.data(), contents.size());

        try {
            const YAML::Node root = YAML::Load(contents);

            for (const auto device_node : root) {
                if (common::lowercase_string(device_node.first.as<std::string>())
                    != common::lowercase_string(firmcode)) {
                    continue;
                }

                if (const YAML::Node node = device_node.second["model"]) {
                    model = node.as<std::string>();
                }

                if (const YAML::Node node = device_node.second["manufacturer"]) {
                    manufacturer = node.as<std::string>();
                }

                if (const YAML::Node node = device_node.second["machine-uid"]) {
                    machine_uid = node.as<std::uint32_t>();
                }

                LOG_INFO(SYSTEM, "Naming {} \"{}\" after the archive's own devices.yml", firmcode, model);
                return;
            }
        } catch (const YAML::Exception &exception) {
            LOG_WARN(SYSTEM, "The archive's devices.yml could not be read ({}), naming the device from its dump",
                exception.what());
        }
    }

    static device_installation_error install_archive_data_dump(device_manager *dvcmngr,
        const host_path &archive_path, const std::vector<common::archive_entry_info> &entries,
        const archive_device_layout &layout, const host_path &rom_resident_path,
        const host_path &drives_z_resident_path, const bool isolate_drives, progress_changed_callback progress_cb,
        cancel_requested_callback cancel_cb) {
        // Same staging convention as install_rom/install_rpkg: unpack into a temp folder, and only rename
        // it to the firmware code once the dump has proven to describe a device we can register. A run
        // that dies in between leaves a `temp` folder behind rather than a half-installed device.
        const host_path temp_z_path(add_path(drives_z_resident_path, "temp\\"));
        const host_path temp_rom_path(add_path(rom_resident_path, "temparchive\\"));

        common::delete_folder(temp_z_path);
        common::delete_folder(temp_rom_path);

        const auto revert = [&]() {
            common::delete_folder(temp_z_path);
            common::delete_folder(temp_rom_path);
        };

        std::vector<host_path> destinations(entries.size());

        for (const std::size_t index : layout.z_files) {
            const std::string relative = entries[index].path.substr(layout.z_prefix.size());

            if (!relative.empty()) {
                destinations[index] = add_path(temp_z_path, relative);
            }
        }

        // The ROM is written under the name the emulator looks for straight away, so placing it later is
        // just a move.
        const host_path rom_temp_file = add_path(temp_rom_path, "SYM.ROM");
        destinations[layout.rom_index] = rom_temp_file;

        if (layout.rom_files.size() > 1) {
            LOG_WARN(SYSTEM, "The ROM folder of this dump holds {} files, using {}", layout.rom_files.size(),
                entries[layout.rom_index].path);
        }

        const host_path packaged_devices_yml = add_path(temp_rom_path, "devices.yml");

        if (layout.has_devices_yml) {
            destinations[layout.devices_yml_index] = packaged_devices_yml;
        }

        if (!extract_planned(archive_path, destinations, ARCHIVE_PROGRESS_TOTAL * 9 / 10, progress_cb, cancel_cb, temp_z_path)) {
            revert();

            return (cancel_cb && cancel_cb()) ? device_installation_general_failure
                                              : device_installation_archive_corrupt;
        }

        // The folder name the pack used is only a hint - take the firmware code from the dump itself, so
        // a device installed from an archive ends up identical to one installed from a ROM.
        const epocver ver = determine_rpkg_symbian_version(temp_z_path);

        std::string manufacturer;
        std::string firmcode;
        std::string model;

        if (!determine_rpkg_product_info(temp_z_path, manufacturer, firmcode, model)) {
            LOG_ERROR(SYSTEM, "Revert all changes");
            revert();

            return device_installation_determine_product_failure;
        }

        if (dvcmngr->get(firmcode)) {
            LOG_ERROR(SYSTEM, "The device already exists, revert all changes");
            revert();

            return device_installation_already_exist;
        }

        std::uint32_t machine_uid = 0;

        if (layout.has_devices_yml) {
            apply_packaged_device_name(packaged_devices_yml, firmcode, manufacturer, model, machine_uid);
        }

        if (machine_uid == 0) {
            machine_uid = determine_rpkg_machine_uid(temp_z_path);
        }

        const std::string firmcode_low = common::lowercase_string(firmcode);
        const host_path final_z_path(add_path(drives_z_resident_path, firmcode_low + "\\"));
        const host_path final_rom_folder = add_path(rom_resident_path, firmcode_low + "\\");

        if (!common::move_file(temp_z_path, final_z_path)) {
            LOG_ERROR(SYSTEM, "Unable to move the drive Z of {} into place, revert all changes", firmcode);
            revert();

            return device_installation_general_failure;
        }

        const add_device_error err_adddvc = dvcmngr->add_new_device(firmcode, model, manufacturer, ver,
            machine_uid, isolate_drives);

        if (err_adddvc != add_device_none) {
            LOG_ERROR(SYSTEM, "This device ({}) failed to be install, revert all changes", firmcode);
            common::delete_folder(final_z_path);
            common::delete_folder(temp_rom_path);

            return device_installation_general_failure;
        }

        common::create_directories(final_rom_folder);

        if (!common::move_file(rom_temp_file, eka2l1::host_path{add_path(final_rom_folder, "SYM.ROM")})) {
            LOG_ERROR(SYSTEM, "Unable to place the ROM of {}, revert all changes", firmcode);
            dvcmngr->delete_device(firmcode);
            common::delete_folder(final_z_path);
            common::delete_folder(final_rom_folder);
            common::delete_folder(temp_rom_path);

            return device_installation_rom_fail_to_copy;
        }

        common::delete_folder(temp_rom_path);

        if (progress_cb) {
            progress_cb(ARCHIVE_PROGRESS_TOTAL, ARCHIVE_PROGRESS_TOTAL);
        }

        return device_installation_none;
    }

    struct archive_firmware_variant {
        std::size_t vpl_index;
        int variant_index;
        std::string description;
        // Entries holding the variant's FPSX files that the archive actually carries.
        std::vector<std::size_t> file_indices;
    };

    static std::string entry_folder(const std::string &lowercased_path) {
        const std::size_t slash = lowercased_path.find_last_of('/');
        return (slash == std::string::npos) ? std::string() : lowercased_path.substr(0, slash + 1);
    }

    /**
     * @brief List every variant of every VPL in the archive whose FPSX files are present.
     *
     * A flash package for one product type usually carries a VPL per product code, all sharing the same
     * core and ROFS2 images and differing only in the language/colour ROFS3. Variants none of whose files
     * made it into the archive are left out.
     */
    static device_installation_error collect_archive_firmware_variants(const host_path &archive_path,
        const std::vector<common::archive_entry_info> &entries, const archive_device_layout &layout,
        std::vector<archive_firmware_variant> &candidates) {
        candidates.clear();

        for (const std::size_t vpl_index : layout.vpl_files) {
            std::vector<char> content;
            if (!common::read_archive_entry(archive_path, entries[vpl_index].path, content, MAX_VPL_SIZE)) {
                return device_installation_archive_corrupt;
            }

            std::vector<firmware_variant_info> variants;
            const device_installation_error result = read_firmware_variants(content, variants);

            if (result != device_installation_none) {
                LOG_WARN(SYSTEM, "Skipping unreadable VPL {} in the archive", entries[vpl_index].path);
                continue;
            }

            const std::string folder = entry_folder(common::lowercase_string(entries[vpl_index].path));

            for (std::size_t variant_index = 0; variant_index < variants.size(); variant_index++) {
                archive_firmware_variant candidate;
                candidate.vpl_index = vpl_index;
                candidate.variant_index = static_cast<int>(variant_index);
                candidate.description = variants[variant_index].description;

                for (const std::string &file : variants[variant_index].files) {
                    const std::string wanted = folder + common::lowercase_string(file);

                    for (std::size_t i = 0; i < entries.size(); i++) {
                        if (!entries[i].is_directory && (common::lowercase_string(entries[i].path) == wanted)) {
                            candidate.file_indices.push_back(i);
                            break;
                        }
                    }
                }

                if (!candidate.file_indices.empty()) {
                    candidates.push_back(std::move(candidate));
                }
            }
        }

        return candidates.empty() ? device_installation_archive_no_device : device_installation_none;
    }

    static device_installation_error install_archive_firmware(device_manager *dvcmngr,
        const host_path &archive_path, const std::vector<common::archive_entry_info> &entries,
        const archive_device_layout &layout, const host_path &rom_resident_path,
        const host_path &drives_z_resident_path, const bool isolate_drives,
        device_firmware_choose_variant_callback choose_variant_cb, progress_changed_callback progress_cb,
        cancel_requested_callback cancel_cb) {
        std::vector<archive_firmware_variant> candidates;
        const device_installation_error list_result = collect_archive_firmware_variants(archive_path, entries,
            layout, candidates);

        if (list_result != device_installation_none) {
            return list_result;
        }

        int chosen = 0;

        if ((candidates.size() > 1) && choose_variant_cb) {
            std::vector<std::string> descriptions;
            for (const archive_firmware_variant &candidate : candidates) {
                descriptions.push_back(candidate.description);
            }

            chosen = choose_variant_cb(descriptions);
        }

        if ((chosen < 0) || (static_cast<std::size_t>(chosen) >= candidates.size())) {
            return device_installation_general_failure;
        }

        const archive_firmware_variant &variant = candidates[chosen];
        LOG_INFO(SYSTEM, "Installing firmware variant {} from {}", variant.description, entries[variant.vpl_index].path);

        // install_firmware resolves FPSX names against the VPL's folder, so both land in one folder.
        const host_path staging = add_path(rom_resident_path, "tempfirmware\\");
        common::delete_folder(staging);

        std::vector<host_path> destinations(entries.size());
        const host_path vpl_path = add_path(staging, eka2l1::filename(entries[variant.vpl_index].path, false));
        destinations[variant.vpl_index] = vpl_path;

        for (const std::size_t index : variant.file_indices) {
            destinations[index] = add_path(staging, eka2l1::filename(entries[index].path, false));
        }

        if (!extract_planned(archive_path, destinations, ARCHIVE_PROGRESS_TOTAL * 2 / 5, progress_cb, cancel_cb)) {
            common::delete_folder(staging);

            return (cancel_cb && cancel_cb()) ? device_installation_general_failure
                                              : device_installation_archive_corrupt;
        }

        progress_changed_callback install_cb = nullptr;

        if (progress_cb) {
            install_cb = [progress_cb](const std::size_t done, const std::size_t total) {
                if (!total) {
                    return;
                }

                progress_cb(ARCHIVE_PROGRESS_TOTAL * 2 / 5 + done * (ARCHIVE_PROGRESS_TOTAL * 3 / 5) / total,
                    ARCHIVE_PROGRESS_TOTAL);
            };
        }

        // drives_z_resident_path is `<storage>/drives/z/`; install_firmware wants the storage root.
        const host_path storage_path(std::filesystem::path(drives_z_resident_path.utf8()).lexically_normal()
                                         .parent_path().parent_path().parent_path().string());

        const int variant_index = variant.variant_index;
        const device_installation_error result = install_firmware(dvcmngr, vpl_path, storage_path,
            rom_resident_path, isolate_drives, [variant_index](const std::vector<std::string> &) { return variant_index; },
            install_cb, cancel_cb);

        common::delete_folder(staging);

        return result;
    }

    device_installation_error list_archive_firmware_variants(const host_path &path,
        std::vector<std::string> &descriptions) {
        descriptions.clear();

        std::vector<common::archive_entry_info> entries;

        if (!common::list_archive(path, entries)) {
            return device_installation_archive_corrupt;
        }

        const archive_device_layout layout = determine_layout(entries);
        if (layout.type != archive_device_layout::firmware) {
            return device_installation_none;
        }

        std::vector<archive_firmware_variant> candidates;
        const device_installation_error result = collect_archive_firmware_variants(path, entries, layout, candidates);

        for (const archive_firmware_variant &candidate : candidates) {
            descriptions.push_back(candidate.description);
        }

        return result;
    }

    device_installation_error install_archive(device_manager *dvcmngr, const host_path &path,
        const host_path &rom_resident_path, const host_path &drives_z_resident_path,
        const bool isolate_drives, progress_changed_callback progress_cb, cancel_requested_callback cancel_cb,
        device_firmware_choose_variant_callback choose_variant_cb) {
        if (!common::exists(path)) {
            return device_installation_not_exist;
        }

        std::vector<common::archive_entry_info> entries;

        if (!common::list_archive(path, entries)) {
            return device_installation_archive_corrupt;
        }

        const archive_device_layout layout = determine_layout(entries);

        switch (layout.type) {
        case archive_device_layout::data_dump:
            LOG_INFO(SYSTEM, "Installing device from an unpacked dump in {} ({} files on drive Z)", path,
                layout.z_files.size());

            return install_archive_data_dump(dvcmngr, path, entries, layout, rom_resident_path,
                drives_z_resident_path, isolate_drives, progress_cb, cancel_cb);

        case archive_device_layout::rom_and_rpkg:
            LOG_INFO(SYSTEM, "Installing device from a ROM{} in {}", layout.has_rpkg ? " and RPKG" : "", path);

            return install_archive_rom_and_rpkg(dvcmngr, path, entries, layout, rom_resident_path,
                drives_z_resident_path, isolate_drives, progress_cb, cancel_cb);

        case archive_device_layout::firmware:
            LOG_INFO(SYSTEM, "Installing device from firmware flash files in {}", path);

            return install_archive_firmware(dvcmngr, path, entries, layout, rom_resident_path,
                drives_z_resident_path, isolate_drives, choose_variant_cb, progress_cb, cancel_cb);

        default:
            break;
        }

        LOG_ERROR(SYSTEM, "The archive {} contains neither a ROM image nor an unpacked device dump", path);
        return device_installation_archive_no_device;
    }
}

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

#include <system/devices.h>
#include <system/installation/archive.h>
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
            data_dump
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
    };

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
            // contents, readme files, its own devices.yml) is left alone: drive C is shared between every
            // installed device, so writing into it would be a side effect the user never asked for.
            const std::string data_prefix = z_prefix.substr(0, z_prefix.find(Z_DRIVE_MARKER));
            const std::string rom_prefix = data_prefix + "roms/" + code + "/";
            const std::string devices_yml_path = data_prefix + "devices.yml";

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
    static bool extract_planned(const std::string &archive_path, const std::vector<std::string> &destinations,
        const std::size_t progress_ceiling, progress_changed_callback progress_cb,
        cancel_requested_callback cancel_cb) {
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
            [&](const common::archive_entry_info &) -> std::string {
                const std::size_t index = next_index++;
                return (index < destinations.size()) ? destinations[index] : std::string();
            },
            wrapped_cb, cancel_cb);
    }

    static device_installation_error install_archive_rom_and_rpkg(device_manager *dvcmngr,
        const std::string &archive_path, const std::vector<common::archive_entry_info> &entries,
        const archive_device_layout &layout, const std::string &rom_resident_path,
        const std::string &drives_z_resident_path, progress_changed_callback progress_cb,
        cancel_requested_callback cancel_cb) {
        // The ROM and the RPKG are read several times over by the installers (the ROM is parsed, then
        // copied; the RPKG is streamed through), so they are unpacked to a scratch folder first rather
        // than being decompressed again for every pass.
        const std::string staging = add_path(rom_resident_path, "temparchive\\");
        common::delete_folder(staging);

        std::vector<std::string> destinations(entries.size());

        const std::string rom_path = add_path(staging, eka2l1::filename(entries[layout.rom_index].path, false));
        destinations[layout.rom_index] = rom_path;

        std::string rpkg_path;

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
            rom_resident_path, drives_z_resident_path, install_cb, cancel_cb);

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
    static void apply_packaged_device_name(const std::string &devices_yml_path, const std::string &firmcode,
        std::string &manufacturer, std::string &model, std::uint32_t &machine_uid) {
        common::ro_std_file_stream stream(devices_yml_path, true);

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
        const std::string &archive_path, const std::vector<common::archive_entry_info> &entries,
        const archive_device_layout &layout, const std::string &rom_resident_path,
        const std::string &drives_z_resident_path, progress_changed_callback progress_cb,
        cancel_requested_callback cancel_cb) {
        // Same staging convention as install_rom/install_rpkg: unpack into a temp folder, and only rename
        // it to the firmware code once the dump has proven to describe a device we can register. A run
        // that dies in between leaves a `temp` folder behind rather than a half-installed device.
        const std::string temp_z_path = add_path(drives_z_resident_path, "temp\\");
        const std::string temp_rom_path = add_path(rom_resident_path, "temparchive\\");

        common::delete_folder(temp_z_path);
        common::delete_folder(temp_rom_path);

        const auto revert = [&]() {
            common::delete_folder(temp_z_path);
            common::delete_folder(temp_rom_path);
        };

        std::vector<std::string> destinations(entries.size());

        // Drive Z is stored lowercased (see install_rpkg), and everything that reads it back - the
        // product info probe, the emulator's own VFS - assumes that. On a case-sensitive host, keeping
        // the archive's `System\` spelling would make the dump unreadable.
        for (const std::size_t index : layout.z_files) {
            const std::string relative = common::lowercase_string(entries[index].path.substr(layout.z_prefix.size()));

            if (!relative.empty()) {
                destinations[index] = add_path(temp_z_path, relative);
            }
        }

        // The ROM is written under the name the emulator looks for straight away, so placing it later is
        // just a move.
        const std::string rom_temp_file = add_path(temp_rom_path, "SYM.ROM");
        destinations[layout.rom_index] = rom_temp_file;

        if (layout.rom_files.size() > 1) {
            LOG_WARN(SYSTEM, "The ROM folder of this dump holds {} files, using {}", layout.rom_files.size(),
                entries[layout.rom_index].path);
        }

        const std::string packaged_devices_yml = add_path(temp_rom_path, "devices.yml");

        if (layout.has_devices_yml) {
            destinations[layout.devices_yml_index] = packaged_devices_yml;
        }

        if (!extract_planned(archive_path, destinations, ARCHIVE_PROGRESS_TOTAL * 9 / 10, progress_cb, cancel_cb)) {
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

        const std::string firmcode_low = common::lowercase_string(firmcode);
        const std::string final_z_path = add_path(drives_z_resident_path, firmcode_low + "\\");
        const std::string final_rom_folder = add_path(rom_resident_path, firmcode_low + "\\");

        if (!common::move_file(temp_z_path, final_z_path)) {
            LOG_ERROR(SYSTEM, "Unable to move the drive Z of {} into place, revert all changes", firmcode);
            revert();

            return device_installation_general_failure;
        }

        const add_device_error err_adddvc = dvcmngr->add_new_device(firmcode, model, manufacturer, ver,
            machine_uid);

        if (err_adddvc != add_device_none) {
            LOG_ERROR(SYSTEM, "This device ({}) failed to be install, revert all changes", firmcode);
            common::delete_folder(final_z_path);
            common::delete_folder(temp_rom_path);

            return device_installation_general_failure;
        }

        common::create_directories(final_rom_folder);

        if (!common::move_file(rom_temp_file, add_path(final_rom_folder, "SYM.ROM"))) {
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

    device_installation_error install_archive(device_manager *dvcmngr, const std::string &path,
        const std::string &rom_resident_path, const std::string &drives_z_resident_path,
        progress_changed_callback progress_cb, cancel_requested_callback cancel_cb) {
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
                drives_z_resident_path, progress_cb, cancel_cb);

        case archive_device_layout::rom_and_rpkg:
            LOG_INFO(SYSTEM, "Installing device from a ROM{} in {}", layout.has_rpkg ? " and RPKG" : "", path);

            return install_archive_rom_and_rpkg(dvcmngr, path, entries, layout, rom_resident_path,
                drives_z_resident_path, progress_cb, cancel_cb);

        default:
            break;
        }

        LOG_ERROR(SYSTEM, "The archive {} contains neither a ROM image nor an unpacked device dump", path);
        return device_installation_archive_no_device;
    }
}

// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <common/algorithm.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <system/devices.h>

#include <array>
#include <algorithm>
#include <filesystem>

namespace eka2l1::ios {
    inline bool commit_device_install(device_manager &manager, device imported,
        const std::string &staging, const std::string &storage, bool &discard_staging) {
        namespace fs = std::filesystem;
        std::unique_lock<std::mutex> guard(manager.lock);
        const std::string code = common::lowercase_string(imported.firmware_code);
        auto &devices = manager.get_devices();
        auto existing = std::find_if(devices.begin(), devices.end(), [&](const device &entry) {
            return common::lowercase_string(entry.firmware_code) == code;
        });
        if (existing != devices.end()) {
            imported.firmware_code = existing->firmware_code;
            imported.isolated_drives = existing->isolated_drives;
        }
        const std::string firmware_code = imported.firmware_code;

        struct directory_swap {
            fs::path source, target, backup;
            bool backed_up = false;
            bool installed = false;
        };
        std::array<directory_swap, 2> swaps{{
            {fs::path(staging) / "drives/z" / code, fs::path(storage) / "drives/z" / code,
                fs::path(staging) / "previous-z"},
            {fs::path(staging) / "roms" / code, fs::path(storage) / "roms" / code,
                fs::path(staging) / "previous-rom"}}};
        std::error_code rom_error;
        if (!fs::is_regular_file(swaps[1].source / "SYM.ROM", rom_error)
            || fs::file_size(swaps[1].source / "SYM.ROM", rom_error) == 0 || rom_error) {
            return false;
        }

        const auto rollback = [&] {
            for (auto it = swaps.rbegin(); it != swaps.rend(); ++it) {
                std::error_code error;
                if (it->installed) {
                    fs::rename(it->target, it->source, error);
                }
                if (!error && it->backed_up) {
                    fs::rename(it->backup, it->target, error);
                }
                if (error) {
                    discard_staging = false;
                    LOG_ERROR(SYSTEM, "Unable to restore device files; backup retained at {}: {}",
                        staging, error.message());
                }
            }
            common::invalidate_folded_directory_indexes();
        };

        for (auto &swap : swaps) {
            std::error_code error;
            fs::create_directories(swap.target.parent_path(), error);
            if (!error && fs::exists(swap.target, error)) {
                fs::rename(swap.target, swap.backup, error);
                swap.backed_up = !error;
            }
            if (!error) {
                fs::rename(swap.source, swap.target, error);
                swap.installed = !error;
            }
            if (error) {
                LOG_ERROR(SYSTEM, "Unable to replace device files: {}", error.message());
                rollback();
                return false;
            }
        }

        if (imported.isolated_drives) {
            std::error_code error;
            fs::create_directories(fs::path(storage) / device_isolated_drives_folder(firmware_code), error);
            if (error) {
                rollback();
                return false;
            }
        }

        if (existing != devices.end()) {
            *existing = std::move(imported);
        } else {
            devices.push_back(std::move(imported));
        }
        common::invalidate_folded_directory_indexes();
        manager.save_devices();
        guard.unlock();
        manager.set_current(firmware_code);
        return true;
    }
}

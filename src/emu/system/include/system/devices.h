/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <common/types.h>

#include <mutex>
#include <string>
#include <vector>

namespace eka2l1::config {
    struct state;
}

namespace eka2l1 {
    struct device {
    private:
        std::uint32_t cached_flags_;
        bool flag_inited_ = false;

        enum {
            DEVICE_FLAG_S80 = 1 << 0
        };

        void init_flags();

    public:
        epocver ver;
        std::string firmware_code;
        std::string manufacturer;
        std::string model;
        std::vector<int> languages;
        std::uint32_t machine_uid;
        int default_language_code;

        // Drives C, D and E live under drives/<firmware code>/ instead of the
        // folders every other device shares.
        bool isolated_drives;

        // Set by device_manager::mark_for_deletion().
        bool pending_deletion;

        explicit device(epocver ver, std::string firmware_code, std::string manufacturer, std::string model)
            : ver(ver)
            , firmware_code(firmware_code)
            , manufacturer(manufacturer)
            , model(model)
            , machine_uid(0)
            , default_language_code(-1)
            , isolated_drives(false)
            , pending_deletion(false) {
        }

        bool is_s80();
    };

    enum add_device_error {
        add_device_none = 0,
        add_device_existed
    };

    /*! \brief A manager for all installed devices on this emulator
    */
    class device_manager {
        std::vector<device> devices;
        config::state *conf;

        std::int32_t current_index;

    public:
        std::mutex lock;

        explicit device_manager(config::state *conf);
        ~device_manager();

        std::vector<device> &get_devices() {
            return devices;
        }

        std::size_t total() {
            return devices.size();
        }

        device *get_current() {
            if ((current_index < 0) || (current_index >= devices.size())) {
                return nullptr;
            }

            return &devices[current_index];
        }

        std::int32_t get_current_index() const {
            return current_index;
        }

        device *lastest() {
            if (devices.empty())
                return nullptr;

            return &devices.back();
        }

        void save_devices();
        void load_devices();
        void clear();

        bool set_current(const std::string &firmcode);
        bool set_current(const std::uint8_t idx);

        add_device_error add_new_device(const std::string &firmcode, const std::string &model, const std::string &manufacturer, const epocver ver, const std::uint32_t machine_uid,
            const bool isolated_drives);

        bool delete_device(const std::string &firmcode);

        // Delete a device and its storage on the next load, before anything can boot it. For
        // frontends that cannot stop the running device on the spot; they restart afterwards.
        bool mark_for_deletion(const std::string &firmcode);

        /*! \brief Get the device with the given firmware code.
         *
         * You should avoid method that involves comparing firmware code, since
         * the comparsion is case-sensitive. Use listing and index instead.
         * 
         * Not thread-safe.
         * 
         * \returns nullptr if the device can't be found
        */
        device *get(const std::string &firmcode);

        /*! \brief Get the device with the given index.
         *
         * Not thread-safe.
         * 
         * \returns nullptr if index out of range.
        */
        device *get(const std::uint8_t index);
    };

    /*! \brief drives/<lowercase firmware code>/, the folder holding a device's isolated drives.
    */
    std::string device_isolated_drives_folder(const std::string &firmware_code);

    /*! \brief The folder backing one of a device's writable drives (C, D or E).
     *
     * \returns A path relative to the emulator's data root, with a trailing separator:
     *          drives/<lowercase firmware code>/<drive>/ for a device with isolated
     *          drives, drives/<drive>/ otherwise.
    */
    std::string device_drive_folder(const std::string &firmware_code, const bool isolated_drives, const drive_number drv);

    /*! \brief Storage folders that belong to one device alone.
     *
     * Drive Z and the ROM image are per-device by construction, and so are C, D and
     * E of a device with isolated drives. The shared C, D and E still hold per-device
     * state too: the servers that keep it there name a folder after the firmware code.
     * All of it has to go when a device is deleted, else reinstalling the same device
     * inherits the old state - and one bad value in it survives every repair a user
     * can perform from the UI.
     *
     * \param   firmware_code   The device's firmware code, in any case.
     * \returns Folder paths relative to the emulator's data root, each with a
     *          trailing separator. They are not guaranteed to exist.
    */
    std::vector<std::string> per_device_storage_paths(const std::string &firmware_code);

    // Delete every folder per_device_storage_paths() lists under the given data root.
    void delete_device_storage(const std::string &storage, const std::string &firmware_code);
}

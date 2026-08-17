#pragma once

#include <common/types.h>
#include <system/installation/common.h>

#include <atomic>
#include <cstdint>
#include <string>

namespace eka2l1 {
    class io_system;
    class device_manager;

    namespace loader {
        struct rpkg_header {
            std::uint32_t magic[4];
            std::uint8_t major_rom;
            std::uint8_t minor_rom;
            std::uint16_t build_rom;
            std::uint32_t count;
            std::uint32_t header_size;
            std::uint32_t machine_uid;
        };

        struct rpkg_entry {
            std::uint64_t attrib;
            std::uint64_t time;
            std::uint64_t path_len;

            std::u16string path;

            std::uint64_t data_size;
        };

        bool should_install_requires_additional_rpkg(const std::string &path);

        device_installation_error install_rom(device_manager *dvc, const std::string &path, const std::string &rom_resident_path, const std::string &drives_z_resident_path, progress_changed_callback progress_cb, cancel_requested_callback cancel_cb);
        device_installation_error install_rpkg(device_manager *dvc, const std::string &path, const std::string &devices_rom_path, std::string &firmware_code, progress_changed_callback progress_cb, cancel_requested_callback cancel_cb);

        /**
         * @brief Install a device from a raw ROM image, plus the RPKG that goes with it if one is needed.
         *
         * Which of the two installers above applies is a property of the ROM: images that keep the device's
         * files in ROFS instead of ROM carry nothing to populate drive Z with, so they need the RPKG dump
         * alongside. This picks the right one and, on the RPKG path, drops the ROM image where the emulator
         * expects to find it afterwards.
         *
         * @param rpkg_path Path to the RPKG, or an empty string when the caller has none. A ROM that turns
         *                  out to need one then fails with device_installation_rpkg_missing.
         */
        device_installation_error install_rom_with_optional_rpkg(device_manager *dvc, const std::string &rom_path,
            const std::string &rpkg_path, const std::string &rom_resident_path, const std::string &drives_z_resident_path,
            progress_changed_callback progress_cb, cancel_requested_callback cancel_cb);
    }
}

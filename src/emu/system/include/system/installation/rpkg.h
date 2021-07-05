#pragma once

#include <system/installation/common.h>
#include <common/types.h>

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
    }
}

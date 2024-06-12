#pragma once

#include <common/types.h>
#include <common/bitfield.h>

#include <vfs/vfs.h>

#include <fat/fat.h>

#include <map>
#include <memory>
#include <string>

namespace eka2l1::vfs {
    struct ngage_mmc_image_info {
        void *context_;
        FILE *stream_;

        std::uint8_t *mapped_image_data_;
        std::unique_ptr<Fat::Image> image_;
        std::vector<std::uint64_t> loaded_pages_;

        std::uint64_t current_offset_;
        std::uint32_t sector_size_;
        std::uint32_t starting_partition_sector_;
        std::uint32_t ending_partition_sector_;
    };

    class aaru_ngage_mmc_file_system : abstract_file_system {
    private:
        std::map<drive_number, std::unique_ptr<ngage_mmc_image_info>> image_infos_;

    public:
        bool exists(const std::u16string &path) override;
        bool replace(const std::u16string &old_path, const std::u16string &new_path) override;

        /*! \brief Mount a drive with a physical host path.
         */
        bool mount_volume_from_path(const drive_number drv, const drive_media media, const std::uint32_t attrib,
            const std::u16string &physical_path) override;

        bool unmount(const drive_number drv) override;

        std::unique_ptr<file> open_file(const std::u16string &path, const int mode) override;
        std::unique_ptr<directory> open_directory(const std::u16string &path, epoc::uid_type type, const std::uint32_t attrib) override;

        std::optional<entry_info> get_entry_info(const std::u16string &path) override;

        bool delete_entry(const std::u16string &path) override;
        bool create_directory(const std::u16string &path) override;
        bool create_directories(const std::u16string &path) override;

        std::optional<drive> get_drive_entry(const drive_number drv) override;

        std::optional<std::u16string> get_raw_path(const std::u16string &path) override {
            return std::nullopt;
        }

        void validate_for_host() override {
        }
    };
}
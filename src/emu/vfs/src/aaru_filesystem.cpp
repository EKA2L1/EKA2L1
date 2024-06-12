#include "aaru_filesystem.h"
#include <common/log.h>
#include <common/virtualmem.h>
#include <aaruformat.h>

namespace eka2l1::vfs {
    static std::uint32_t aaru_fat_image_read(void *userdata, void *buffer, std::uint32_t bytes) {
        auto *info = reinterpret_cast<ngage_mmc_image_info*>(userdata);
        if (!info) {
            return 0;
        }

        auto page_size = common::get_host_page_size();
        auto total_sectors_needed_load_per_page = page_size / info->sector_size_;

        auto start_offset = info->current_offset_;
        auto end_offset = info->current_offset_ + bytes - 1;        // For page loading

        auto starting_page = start_offset / page_size;
        auto ending_page = end_offset / page_size;

        for (auto page_index = starting_page; page_index <= ending_page; page_index++) {
            if (info->loaded_pages_[page_index >> 6] & (1ULL << static_cast<std::uint8_t>(page_index & 63))) {
                common::commit(info->mapped_image_data_ + page_index * page_size, page_size, prot::prot_read_write);

                // Start loading
                auto sector_index = info->starting_partition_sector_ + page_index * total_sectors_needed_load_per_page;
                auto sector_to_load = common::min(total_sectors_needed_load_per_page, static_cast<std::uint32_t>(info->ending_partition_sector_ - sector_index + 1));

                for (auto i = 0; i < sector_to_load; i++) {
                    std::uint32_t temp_size = info->sector_size_;

                    auto result_read = aaruf_read_sector(info->context_, sector_index, info->mapped_image_data_ + page_index * page_size +
                        i * info->sector_size_, &temp_size);

                    if (result_read != AARUF_STATUS_OK) {
                        LOG_WARN(VFS, "Unable to read sector {} of aaru game dump!", sector_index + i);
                    }
                }

                common::commit(info->mapped_image_data_ + page_index * page_size, page_size, prot::prot_read);

                info->loaded_pages_[page_index >> 6] |= (1ULL << static_cast<std::uint8_t>(page_index & 63));
            }
        }

        auto end_file_offset = (info->ending_partition_sector_ - info->starting_partition_sector_ + 1) * info->sector_size_;
        auto actual_read = common::min(bytes, static_cast<std::uint32_t>(end_file_offset - info->current_offset_));

        std::memcpy(buffer, info->mapped_image_data_ + info->current_offset_, actual_read);
        return actual_read;
    }

    static std::uint32_t aaru_fat_image_seek(void *userdata, std::uint32_t offset, int mode) {
        auto *info = reinterpret_cast<ngage_mmc_image_info*>(userdata);
        if (!info) {
            return 0;
        }

        switch (mode) {
        case Fat::IMAGE_SEEK_MODE_BEG:
            info->current_offset_ = offset;
            break;

        case Fat::IMAGE_SEEK_MODE_CUR:
            info->current_offset_ += offset;
            break;

        case Fat::IMAGE_SEEK_MODE_END:
            info->current_offset_ = (info->ending_partition_sector_ - info->starting_partition_sector_ + 1) * info->sector_size_ + offset;
            break;

        default:
            break;
        }

        return info->current_offset_;
    }

    bool aaru_ngage_mmc_file_system::exists(const std::u16string &path) {
        return true;
    }

    bool aaru_ngage_mmc_file_system::replace(const std::u16string &old_path, const std::u16string &new_path) {
        LOG_ERROR(VFS, "N-Gage MMC is read-only! Can't not copy from {} to {}", common::ucs2_to_utf8(old_path),
            common::ucs2_to_utf8(new_path));

        return false;
    }

    /*! \brief Mount a drive with a physical host path.
     */
    bool aaru_ngage_mmc_file_system::mount_volume_from_path(const drive_number drv, const drive_media media,
        const std::uint32_t attrib, const std::u16string &physical_path) {
        if (image_infos_.contains(drv)) {
            LOG_ERROR(VFS, "Drive {} is already mounted!", drv);
            return false;
        }

        auto file_stream = common::open_c_file(common::ucs2_to_utf8(physical_path), "rb");

        if (!file_stream) {
            LOG_ERROR(VFS, "Unable to open AARU MMC dump: {}", common::ucs2_to_utf8(physical_path));
            return false;
        }

        auto context = aaruf_open_from_handle(file_stream);

        if (!context) {
            LOG_ERROR(VFS, "AARU unable to parse the dump: {}", common::ucs2_to_utf8(physical_path));
            fclose(file_stream);

            return false;
        }

        ImageInfo image_info;
        aaruf_get_image_info(context, &image_info);

        // Read master boot
        std::vector<std::uint8_t> first_sector_data;
        first_sector_data.resize(image_info.SectorSize);

        auto len = static_cast<std::uint32_t>(first_sector_data.size());
        auto result_read = aaruf_read_sector(context, 0, first_sector_data.data(), &len);

        if (result_read != AARUF_STATUS_OK) {
            LOG_ERROR(VFS, "AARU failed to read master boot record!");

            aaruf_close(context);
            fclose(file_stream);

            return false;
        }

        if (first_sector_data[254] != 0x55 || first_sector_data[255] != 0xAA) {
            LOG_ERROR(VFS, "The MMC data does not have valid signature, please recheck!");

            aaruf_close(context);
            fclose(file_stream);

            return false;
        }

        // Grab the first partition entry
        static constexpr const std::uint32_t FIRST_PARTITION_INFO_OFFSET = 0x1BE;

        std::uint32_t partition_first_sector_index = 0;
        std::uint32_t partition_last_sector_index = 0;

        std::memcpy(&partition_first_sector_index, first_sector_data.data() + FIRST_PARTITION_INFO_OFFSET + 1, 3);
        std::memcpy(&partition_last_sector_index, first_sector_data.data() + FIRST_PARTITION_INFO_OFFSET + 5, 3);

        if (partition_last_sector_index < partition_first_sector_index) {
            LOG_ERROR(VFS, "Invalid MMC: start partition sector index = {} > last partition sector index = {}",
                partition_first_sector_index, partition_last_sector_index);

            aaruf_close(context);
            fclose(file_stream);

            return false;
        }

        auto sector_count = partition_last_sector_index - partition_first_sector_index + 1;
        auto size_to_premap = ((sector_count * image_info.SectorSize + common::get_host_page_size() - 1) /
            common::get_host_page_size() * common::get_host_page_size());

        auto premapped_sector_ptr = common::map_memory(size_to_premap);

        std::unique_ptr<ngage_mmc_image_info> mmc_image_info = std::make_unique<ngage_mmc_image_info>();
        mmc_image_info->current_offset_ = 0;
        mmc_image_info->sector_size_ = image_info.SectorSize;
        mmc_image_info->starting_partition_sector_ = partition_first_sector_index;
        mmc_image_info->ending_partition_sector_ = partition_last_sector_index;
        mmc_image_info->context_ = context;
        mmc_image_info->stream_ = file_stream;
        mmc_image_info->mapped_image_data_ = reinterpret_cast<std::uint8_t*>(premapped_sector_ptr);
        mmc_image_info->loaded_pages_.resize(size_to_premap / common::get_host_page_size());

        std::fill(mmc_image_info->loaded_pages_.begin(), mmc_image_info->loaded_pages_.end(), 0);

        std::unique_ptr<Fat::Image> fat_image = std::make_unique<Fat::Image>(reinterpret_cast<void*>(mmc_image_info.get()),
            &aaru_fat_image_read,
            &aaru_fat_image_seek);

        mmc_image_info->image_ = std::move(fat_image);
        image_infos_.emplace(drv, std::move(mmc_image_info));

        return true;
    }

    bool aaru_ngage_mmc_file_system::unmount(const drive_number drv) {
        return true;
    }

    std::unique_ptr<file> aaru_ngage_mmc_file_system::open_file(const std::u16string &path, const int mode) {
        return nullptr;
    }

    std::unique_ptr<directory> aaru_ngage_mmc_file_system::open_directory(const std::u16string &path, epoc::uid_type type, const std::uint32_t attrib) {
        return nullptr;
    }

    std::optional<entry_info> aaru_ngage_mmc_file_system::get_entry_info(const std::u16string &path) {
        return std::nullopt;
    }

    bool aaru_ngage_mmc_file_system::delete_entry(const std::u16string &path) {
        LOG_ERROR(VFS, "N-Gage MMC is read-only! Can't delete file at path {}", common::ucs2_to_utf8(path));
        return false;
    }

    bool aaru_ngage_mmc_file_system::create_directory(const std::u16string &path) {
        LOG_ERROR(VFS, "N-Gage MMC is read-only! Can't create directory at path {}", common::ucs2_to_utf8(path));
        return false;
    }

    bool aaru_ngage_mmc_file_system::create_directories(const std::u16string &path) {
        LOG_ERROR(VFS, "N-Gage MMC is read-only! Can't create directories at path {}", common::ucs2_to_utf8(path));
        return false;
    }

    std::optional<drive> aaru_ngage_mmc_file_system::get_drive_entry(const drive_number drv) {
        return std::nullopt;
    }
}
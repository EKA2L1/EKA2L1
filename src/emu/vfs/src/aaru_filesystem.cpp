#include "aaru_filesystem.h"
#include "common/path.h"
#include <aaruformat.h>
#include <common/log.h>
#include <common/virtualmem.h>
#include <common/time.h>
#include <common/wildcard.h>

namespace eka2l1::vfs {
    static std::uint32_t chs_to_lba(std::uint32_t cylinder, std::uint32_t head, std::uint32_t sector, std::uint32_t max_head,
        std::uint32_t max_sectors) {
        if (max_head == 0 || max_sectors == 0) {
            return (cylinder * 16 + head) * 63 + sector - 1;
        } else {
            return (cylinder * max_head + head) * max_sectors + sector - 1;
        }
    }

    static std::uint64_t dos_time_to_0ad_time(const std::uint16_t last_modified_date, const std::uint16_t last_modified_time) {
        struct tm time_info = {};
        time_info.tm_year = 1980 + ((last_modified_date >> 9) & 0b1111111);
        time_info.tm_mon = ((last_modified_date >> 5) & 0b1111) - 1;
        time_info.tm_mday = last_modified_date & 0b11111;

        time_info.tm_hour = (last_modified_time >> 11) & 0b11111;
        time_info.tm_min = (last_modified_time >> 5) & 0b111111;
        time_info.tm_sec = (last_modified_time & 0b11111) * 2;

        auto epoch_time = mktime(&time_info);
        return common::convert_microsecs_epoch_to_0ad(epoch_time * common::microsecs_per_sec);
    }

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
            if ((info->loaded_pages_[page_index >> 6] & (1ULL << static_cast<std::uint8_t>(page_index & 63))) == 0) {
                common::commit(info->mapped_image_data_ + page_index * page_size, page_size, prot::prot_read_write);

                // Start loading
                auto sector_index = info->starting_partition_sector_ + page_index * total_sectors_needed_load_per_page;
                auto sector_to_load = common::min(total_sectors_needed_load_per_page, static_cast<std::uint32_t>(info->ending_partition_sector_ - sector_index + 1));

                for (auto i = 0U; i < sector_to_load; i++) {
                    std::uint32_t temp_size = info->sector_size_;

                    auto result_read = aaruf_read_sector(info->context_, sector_index + i, info->mapped_image_data_ + page_index * page_size +
                        i * info->sector_size_, &temp_size);

                    if (result_read != AARUF_STATUS_OK) {
                        LOG_WARN(VFS, "Unable to read sector {} of aaru game dump!", sector_index + i);
                    }
                }

                common::commit(info->mapped_image_data_ + page_index * page_size, page_size, prot::prot_read);

                info->loaded_pages_[page_index >> 6] |= (1ULL << static_cast<std::uint8_t>(page_index & 63));
            }
        }

        auto end_file_offset = info->get_image_size();
        auto actual_read = common::min(bytes, static_cast<std::uint32_t>(end_file_offset - info->current_offset_));

        std::memcpy(buffer, info->mapped_image_data_ + info->current_offset_, actual_read);
        info->current_offset_ += actual_read;

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

    static std::optional<Fat::Entry> find_entry(Fat::Image *image, const std::u16string &path) {
        if (path.empty()) {
            return std::nullopt;
        }

        eka2l1::path_iterator_16 iterator(path);
        std::int32_t size_stack = 0;

        // Skip the root
        iterator++;

        Fat::Entry current_entry;
        bool is_initial = true;

        for (; iterator; iterator++) {
            const std::u16string ite_value = *iterator;
            if (ite_value == u"..") {
                size_stack--;
            } else if (ite_value != u".") {
                size_stack++;
            }

            if (size_stack < 0) {
                return std::nullopt;
            }

            if (!is_initial) {
                if ((current_entry.entry.file_attributes & (int)Fat::EntryAttribute::DIRECTORY) == 0) {
                    return std::nullopt;
                } else {
                    Fat::Entry child_first_entry;

                    if (!image->get_first_entry_dir(current_entry, child_first_entry)) {
                        return std::nullopt;
                    }

                    current_entry = child_first_entry;
                }
            } else {
                is_initial = false;
            }

            do {
                if (!image->get_next_entry(current_entry)) {
                    return std::nullopt;
                }

                if (current_entry.get_filename().empty()) {
                    return std::nullopt;
                }
            } while (common::compare_ignore_case(ite_value, current_entry.get_filename()) != 0);
        }

        return current_entry;
    }

    aaru_ngage_mmc_file_system::aaru_ngage_mmc_file_system()
        : image_info_(nullptr) {
    }

    aaru_ngage_mmc_file_system::~aaru_ngage_mmc_file_system() {
        if (image_info_ != nullptr) {
            unmount(image_info_->mounted_drive_);
        }
    }

    bool aaru_ngage_mmc_file_system::check_if_mounted(const std::u16string &path) const {
        if (!image_info_ || path.empty()) {
            return false;
        }

        auto drive_of_path = char16_to_drive(path[0]);
        return drive_of_path == image_info_->mounted_drive_;
    }

    bool aaru_ngage_mmc_file_system::exists(const std::u16string &path) {
        if (!check_if_mounted(path)) {
            return false;
        }

        auto entry = find_entry(image_info_->image_.get(), path);
        return entry.has_value();
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
        if (image_info_ != nullptr) {
            LOG_ERROR(VFS, "Can only mount one N-Gage MMC!");
            return false;
        }

        if (media != drive_media::mmc) {
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

        if (first_sector_data[510] != 0x55 || first_sector_data[511] != 0xAA) {
            LOG_ERROR(VFS, "The MMC data does not have valid signature, please recheck!");

            aaruf_close(context);
            fclose(file_stream);

            return false;
        }

        // Grab the first partition entry
        static constexpr const std::uint32_t FIRST_PARTITION_INFO_OFFSET = 0x1BE;

        std::uint32_t partition_first_sector_index = 0;
        std::uint32_t partition_sector_count = 0;
        std::uint32_t partition_last_sector_index = 0;

        std::memcpy(&partition_first_sector_index, first_sector_data.data() + FIRST_PARTITION_INFO_OFFSET + 8, sizeof(std::uint32_t));
        std::memcpy(&partition_sector_count, first_sector_data.data() + FIRST_PARTITION_INFO_OFFSET + 12, sizeof(std::uint32_t));

        if (partition_first_sector_index == 0 && partition_sector_count == 0) {
            std::uint8_t start_head = first_sector_data[FIRST_PARTITION_INFO_OFFSET + 1];
            std::uint8_t start_sector = first_sector_data[FIRST_PARTITION_INFO_OFFSET + 2] & 0b111111;
            std::uint8_t start_cylinder = (static_cast<std::uint32_t>(first_sector_data[FIRST_PARTITION_INFO_OFFSET + 2] & 0xC0) << 2) | first_sector_data[FIRST_PARTITION_INFO_OFFSET + 3];

            std::uint32_t end_head = first_sector_data[FIRST_PARTITION_INFO_OFFSET + 5];
            std::uint32_t end_sector = first_sector_data[FIRST_PARTITION_INFO_OFFSET + 6] & 0b111111;
            std::uint32_t end_cylinder = (static_cast<std::uint32_t>(first_sector_data[FIRST_PARTITION_INFO_OFFSET + 6] & 0xC0) << 2) | first_sector_data[FIRST_PARTITION_INFO_OFFSET + 7];

            partition_first_sector_index = chs_to_lba(start_cylinder, start_head, start_sector, image_info.Heads,
                image_info.SectorsPerTrack);

            partition_last_sector_index = chs_to_lba(end_cylinder, end_head, end_sector, image_info.Heads,
                image_info.SectorsPerTrack);
        } else {
            partition_last_sector_index = partition_first_sector_index + partition_sector_count - 1;
        }

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
        mmc_image_info->mounted_drive_ = drv;

        std::fill(mmc_image_info->loaded_pages_.begin(), mmc_image_info->loaded_pages_.end(), 0);

        std::unique_ptr<Fat::Image> fat_image = std::make_unique<Fat::Image>(reinterpret_cast<void*>(mmc_image_info.get()),
            &aaru_fat_image_read,
            &aaru_fat_image_seek);

        mmc_image_info->image_ = std::move(fat_image);
        image_info_ = std::move(mmc_image_info);

        return true;
    }

    bool aaru_ngage_mmc_file_system::unmount(const drive_number drv) {
        if (!image_info_ || image_info_->mounted_drive_ != drv) {
            return false;
        }

        common::unmap_memory(image_info_->mapped_image_data_, image_info_->get_image_size());

        aaruf_close(image_info_->context_);
        fclose(image_info_->stream_);

        image_info_.reset();

        return true;
    }

    std::unique_ptr<file> aaru_ngage_mmc_file_system::open_file(const std::u16string &path, const int mode) {
        if (!check_if_mounted(path)) {
            return nullptr;
        }

        auto entry = find_entry(image_info_->image_.get(), path);
        if (!entry.has_value()) {
            return nullptr;
        }

        if (entry->entry.file_attributes & (int)Fat::EntryAttribute::DIRECTORY) {
            LOG_ERROR(VFS, "Can't open directory as file: {}", common::ucs2_to_utf8(path));
            return nullptr;
        }

        return std::make_unique<aaru_ngage_mmc_file>(image_info_->image_.get(), std::move(entry.value()), path);
    }

    std::unique_ptr<directory> aaru_ngage_mmc_file_system::open_directory(const std::u16string &path, epoc::uid_type type, const std::uint32_t attrib) {
        if (!check_if_mounted(path)) {
            return nullptr;
        }

        auto dir = eka2l1::file_directory(path);
        auto filename = eka2l1::filename(path);

        auto dir_entry = find_entry(image_info_->image_.get(), dir);
        if (!dir_entry.has_value()) {
            return nullptr;
        }

        Fat::Entry first_entry_of_dir;
        image_info_->image_->get_first_entry_dir(dir_entry.value(), first_entry_of_dir);

        if (filename.empty()) {
            filename = u"*";
        }

        return std::make_unique<aaru_ngage_mmc_directory>(image_info_->image_.get(), first_entry_of_dir, dir, filename,
            attrib);
    }

    static entry_info fat_entry_info_to_emulator(Fat::Entry &entry, const std::u16string &path) {
        entry_info info;
        info.size = entry.entry.file_size;
        info.type = (entry.entry.file_attributes & (int)Fat::EntryAttribute::DIRECTORY) ? io_component_type::dir : io_component_type::file;
        info.attribute = 0;

        if (entry.entry.file_attributes & (int)Fat::EntryAttribute::HIDDEN) {
            info.attribute |= io_attrib_hidden;
        }

        info.last_write = dos_time_to_0ad_time(entry.entry.last_modified_date, entry.entry.last_modified_time);

        // FAT attribute is same as Symbian
        info.has_raw_attribute = true;
        info.raw_attribute = entry.entry.file_attributes;
        info.name = common::ucs2_to_utf8(entry.get_filename());
        info.full_path = common::ucs2_to_utf8(path);

        return info;
    }

    std::optional<entry_info> aaru_ngage_mmc_file_system::get_entry_info(const std::u16string &path) {
        if (!check_if_mounted(path)) {
            return std::nullopt;
        }

        auto entry = find_entry(image_info_->image_.get(), path);
        if (!entry.has_value()) {
            return std::nullopt;
        }

        return fat_entry_info_to_emulator(entry.value(), path);
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
        if (!image_info_ || image_info_->mounted_drive_ != drv) {
            return std::nullopt;
        }

        drive drive_entry;
        drive_entry.drive_name = common::ucs2_to_utf8(std::u16string(1, drive_to_char16(drv)) + u":");
        drive_entry.media_type = drive_media::physical;
        drive_entry.attribute = io_attrib_removeable | io_attrib_write_protected;

        return drive_entry;
    }

    aaru_ngage_mmc_file::aaru_ngage_mmc_file(Fat::Image *image, Fat::Entry entry, const std::u16string &path)
        : file(0) {
        image_ = image;
        entry_ = std::move(entry);
        offset_ = 0;
        full_path_ = path;
    }

    aaru_ngage_mmc_file::~aaru_ngage_mmc_file() {
    }

    /*! \brief Write to the file.
     *
     * Write binary data to a file open with WRITE_MODE
     *
     * \param data Pointer to the binary data.
     * \param size The size of each element the binary data
     * \param count Total element count.
     *
     * \returns Total bytes wrote. -1 if fail.
     */
    size_t aaru_ngage_mmc_file::write_file(const void *data, uint32_t size, uint32_t count) {
        LOG_ERROR(VFS, "N-Gage MMC is read-only! Can't write to file {}", common::ucs2_to_utf8(entry_.get_filename()));
        return 0;
    }

    /*! \brief Read from the file.
     *
     * Read from the file to the specified pointer
     *
     * \param data Pointer to the destination.
     * \param size The size of each element to read
     * \param count Total element count.
     *
     * \returns Total bytes read. -1 if fail.
     */
    size_t aaru_ngage_mmc_file::read_file(void *data, uint32_t size, uint32_t count) {
        std::size_t total_size_want_read = size * count;
        std::size_t actual_size = common::min(total_size_want_read, static_cast<std::size_t>(entry_.entry.file_size - offset_));

        auto actual_read_size = image_->read_from_cluster(reinterpret_cast<std::uint8_t*>(data), static_cast<std::uint32_t>(tell()),
            entry_.entry.starting_cluster, static_cast<std::uint32_t>(actual_size));

        offset_ += actual_read_size;
        return actual_read_size;
    }

    /*! \brief Get the file mode which specified with VFS system.
         * \returns The file mode.
     */
    int aaru_ngage_mmc_file::file_mode() const {
        return READ_MODE | BIN_MODE;
    }

    /*! \brief Get the full path of the file.
     * \returns The full path of the file.
     */
    std::u16string aaru_ngage_mmc_file::file_name() const {
        return full_path_;
    }

    /*! \brief Size of the file.
         * \returns The size of the file.
     */
    uint64_t aaru_ngage_mmc_file::size() const {
        return entry_.entry.file_size;
    }

    /*! \brief Seek the file with specified mode.
     */
    uint64_t aaru_ngage_mmc_file::seek(std::int64_t seek_off, file_seek_mode where) {
        switch (where) {
        case file_seek_mode::beg:
            offset_ = static_cast<std::uint64_t>(common::max(0LL, seek_off));
            break;

        case file_seek_mode::crr:
            offset_ = static_cast<std::uint64_t>(common::max(0LL, static_cast<std::int64_t>(offset_) + seek_off));
            break;

        case file_seek_mode::end:
            offset_ = static_cast<std::uint64_t>(common::max(0LL, static_cast<std::int64_t>(entry_.entry.file_size) + seek_off));
            break;

        default:
            break;
        }

        return offset_;
    }

    /*! \brief Get the position of the seek cursor.
     * \returns The position of the seek cursor.
     */
    uint64_t aaru_ngage_mmc_file::tell() {
        return offset_;
    }

    /*! \brief Close the file.
         * \returns True if file is closed successfully.
     */
    bool aaru_ngage_mmc_file::close() {
        return true;
    }

    /*! \brief Please don't use this. */
    std::string aaru_ngage_mmc_file::get_error_descriptor() {
        return "";
    }

    bool aaru_ngage_mmc_file::resize(const std::size_t new_size) {
        LOG_ERROR(VFS, "N-Gage MMC is read-only! Can't resize file {}", common::ucs2_to_utf8(entry_.get_filename()));
        return false;
    }

    bool aaru_ngage_mmc_file::flush() {
        return true;
    }

    bool aaru_ngage_mmc_file::valid() {
        return offset_ < entry_.entry.file_size;
    }

    std::uint64_t aaru_ngage_mmc_file::last_modify_since_0ad() {
        return dos_time_to_0ad_time(entry_.entry.last_modified_date, entry_.entry.last_modified_time);
    }

    aaru_ngage_mmc_directory::aaru_ngage_mmc_directory(Fat::Image *image, Fat::Entry entry, const std::u16string &root_path,
        const std::u16string &match_regex, const std::uint32_t attrib)
        : directory(attrib)
        , image_(image)
        , dir_entry_found_cur_(std::move(entry))
        , has_peeked_(false)
        , root_path_(root_path)
        , filename_matcher_(common::wstr_to_utf8(common::wildcard_to_regex_string(
              common::ucs2_to_wstr(match_regex), false))) {
    }

    bool aaru_ngage_mmc_directory::iterate_to_next_entry() {
        while (true) {
            if (!image_->get_next_entry(dir_entry_found_cur_)) {
                return false;
            }

            if (dir_entry_found_cur_.get_filename().empty()) {
                return false;
            }

            if (attribute & io_attrib_include_dir) {
                if ((dir_entry_found_cur_.entry.file_attributes & (int)Fat::EntryAttribute::DIRECTORY) == 0) {
                    continue;
                }
            }

            if (attribute & io_attrib_include_file) {
                if ((dir_entry_found_cur_.entry.file_attributes & (int)Fat::EntryAttribute::ARCHIVE) == 0) {
                    continue;
                }
            }

            if (RE2::FullMatch(common::ucs2_to_utf8(dir_entry_found_cur_.get_filename()), filename_matcher_)) {
                return true;
            }
        }
    }

    std::optional<entry_info> aaru_ngage_mmc_directory::get_next_entry() {
        if (has_peeked_) {
            if (peeked_entry_.has_value()) {
                auto result = fat_entry_info_to_emulator(peeked_entry_.value(), eka2l1::add_path(root_path_, peeked_entry_->get_filename()));

                dir_entry_found_cur_ = std::move(peeked_entry_.value());

                has_peeked_ = false;
                peeked_entry_ = std::nullopt;

                return result;
            } else {
                return std::nullopt;
            }
        }

        if (!iterate_to_next_entry()) {
            return std::nullopt;
        }

        return fat_entry_info_to_emulator(dir_entry_found_cur_, eka2l1::add_path(root_path_, dir_entry_found_cur_.get_filename()));
    }

    std::optional<entry_info> aaru_ngage_mmc_directory::peek_next_entry() {
        if (has_peeked_) {
            if (peeked_entry_.has_value()) {
                return fat_entry_info_to_emulator(peeked_entry_.value(), eka2l1::add_path(root_path_, peeked_entry_->get_filename()));
            } else {
                return std::nullopt;
            }
        }

        auto previous_entry = dir_entry_found_cur_;
        peeked_entry_ = std::nullopt;
        has_peeked_ = true;

        if (!iterate_to_next_entry()) {
            return std::nullopt;
        }

        peeked_entry_ = dir_entry_found_cur_;
        dir_entry_found_cur_ = previous_entry;

        return fat_entry_info_to_emulator(peeked_entry_.value(), eka2l1::add_path(root_path_, peeked_entry_->get_filename()));
    }
}

namespace eka2l1 {
    std::shared_ptr<abstract_file_system> create_mmc_filesystem() {
        return std::make_shared<vfs::aaru_ngage_mmc_file_system>();
    }
}
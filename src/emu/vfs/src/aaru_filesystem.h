#pragma once

#include <common/types.h>
#include <common/bitfield.h>

#include <vfs/vfs.h>

#include <fat/fat.h>
#include <re2/re2.h>

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

        std::uint32_t current_offset_;
        std::uint32_t sector_size_;
        std::uint32_t starting_partition_sector_;
        std::uint32_t ending_partition_sector_;

        drive_number mounted_drive_;

        const std::size_t get_image_size() const {
            return (ending_partition_sector_ - starting_partition_sector_ + 1) * sector_size_;
        }
    };

    class aaru_ngage_mmc_file : public file {
    private:
        Fat::Image *image_;
        Fat::Entry entry_;

        std::uint64_t offset_;
        std::u16string full_path_;

    public:
        explicit aaru_ngage_mmc_file(Fat::Image *image, Fat::Entry entry, const std::u16string &path);
        ~aaru_ngage_mmc_file() override;

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
        size_t write_file(const void *data, uint32_t size, uint32_t count) override;

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
        size_t read_file(void *data, uint32_t size, uint32_t count) override;

        /*! \brief Get the file mode which specified with VFS system.
         * \returns The file mode.
         */
        int file_mode() const override;

        /*! \brief Get the full path of the file.
         * \returns The full path of the file.
         */
        std::u16string file_name() const override;

        /*! \brief Size of the file.
         * \returns The size of the file.
         */
        uint64_t size() const override;

        /*! \brief Seek the file with specified mode.
         */
        uint64_t seek(std::int64_t seek_off, file_seek_mode where) override;

        /*! \brief Get the position of the seek cursor.
         * \returns The position of the seek cursor.
         */
        uint64_t tell() override;

        /*! \brief Close the file.
         * \returns True if file is closed successfully.
         */
        bool close() override;

        /*! \brief Please don't use this. */
        std::string get_error_descriptor() override;

        /*! \brief Check if the file is in ROM or not.
         *
         * Notice that files in Z: drive is not always ROM file. Z: drive is a combination
         * of ROFS and ROM (please see EBootMagic.txt in sys/data).
         */
        bool is_in_rom() const override {
            return false;
        }

        /*! \brief Get the address of the file in the ROM.
        *
        * If the file is not in ROM, this return a null pointer.
         */
        address rom_address() const override {
            return 0;
        }

        bool resize(const std::size_t new_size) override;
        bool flush() override;
        bool valid() override;

        std::uint64_t last_modify_since_0ad() override;
    };

    class aaru_ngage_mmc_directory : public directory {
    private:
        Fat::Image *image_;

        Fat::Entry dir_entry_found_cur_;
        std::optional<Fat::Entry> peeked_entry_;
        bool has_peeked_;

        std::u16string root_path_;
        RE2 filename_matcher_;

        bool iterate_to_next_entry();

    public:
        explicit aaru_ngage_mmc_directory(Fat::Image *image, Fat::Entry entry, const std::u16string &root_path,
            const std::u16string &match_regex, const std::uint32_t attrib);

        std::optional<entry_info> get_next_entry() override;
        std::optional<entry_info> peek_next_entry() override;
    };

    class aaru_ngage_mmc_file_system : public abstract_file_system {
    private:
       std::unique_ptr<ngage_mmc_image_info> image_info_;

       bool check_if_mounted(const std::u16string &path) const;

    public:
        explicit aaru_ngage_mmc_file_system();
        ~aaru_ngage_mmc_file_system();

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
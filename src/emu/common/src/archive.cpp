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

#include <common/algorithm.h>
#include <common/archive.h>
#include <common/buffer.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <memory>

namespace eka2l1::common {
    static constexpr std::size_t ARCHIVE_READ_BLOCK_SIZE = 1 << 16;

    struct archive_closer {
        void operator()(struct archive *a) const {
            archive_read_close(a);
            archive_read_free(a);
        }
    };

    using archive_ptr = std::unique_ptr<struct archive, archive_closer>;

    static archive_ptr open_archive(const std::string &path) {
        archive_ptr reader(archive_read_new());

        if (!reader) {
            return nullptr;
        }

        // Let libarchive sniff the container instead of trusting the extension: people rename these
        // files, and a .7z that is really a zip should still install.
        archive_read_support_format_all(reader.get());
        archive_read_support_filter_all(reader.get());

        // Windows wants a wide path; elsewhere the UTF-8 one is what open(2) takes.
#ifdef _WIN32
        const std::u16string path_u16 = common::utf8_to_ucs2(path);
        const int result = archive_read_open_filename_w(reader.get(),
            reinterpret_cast<const wchar_t *>(path_u16.c_str()), ARCHIVE_READ_BLOCK_SIZE);
#else
        const int result = archive_read_open_filename(reader.get(), path.c_str(), ARCHIVE_READ_BLOCK_SIZE);
#endif

        if (result != ARCHIVE_OK) {
            LOG_ERROR(COMMON, "Unable to open archive {}: {}", path, archive_error_string(reader.get()));
            return nullptr;
        }

        return reader;
    }

    static archive_entry_info describe_entry(struct archive_entry *entry) {
        archive_entry_info info;

        // archive_entry_pathname_utf8 can be null for an entry whose name is not convertible; the plain
        // accessor still gives us the raw bytes, which beats dropping the entry entirely.
        const char *name = archive_entry_pathname_utf8(entry);

        if (!name) {
            name = archive_entry_pathname(entry);
        }

        info.path = name ? name : "";
        std::replace(info.path.begin(), info.path.end(), '\\', '/');

        info.is_directory = (archive_entry_filetype(entry) == AE_IFDIR);
        info.size = info.is_directory ? 0 : static_cast<std::uint64_t>(std::max<la_int64_t>(0,
            archive_entry_size(entry)));

        return info;
    }

    bool list_archive(const std::string &path, std::vector<archive_entry_info> &entries_out) {
        archive_ptr reader = open_archive(path);

        if (!reader) {
            return false;
        }

        struct archive_entry *entry = nullptr;
        int result = ARCHIVE_OK;

        while ((result = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
            entries_out.push_back(describe_entry(entry));
        }

        if (result != ARCHIVE_EOF) {
            LOG_ERROR(COMMON, "Unable to read the contents of archive {}: {}", path,
                archive_error_string(reader.get()));
            return false;
        }

        return true;
    }

    static bool write_entry_to_file(struct archive *reader, const std::string &dest_path) {
        const std::string dir = eka2l1::file_directory(dest_path);

        if (!dir.empty()) {
            common::create_directories(dir);
        }

        common::wo_std_file_stream out_file(dest_path, true);

        if (!out_file.valid()) {
            LOG_ERROR(COMMON, "Unable to write extracted file to {}", dest_path);
            return false;
        }

        // archive_read_data rather than archive_read_data_block: it hands back one contiguous stream with
        // the holes of a sparse entry already zero-filled, so the file can be written front to back. The
        // block form would report each run's offset and leave the seeking to us, which is more than this
        // needs.
        std::vector<char> buffer(ARCHIVE_READ_BLOCK_SIZE);

        for (;;) {
            const la_ssize_t read = archive_read_data(reader, buffer.data(), buffer.size());

            if (read == 0) {
                return true;
            }

            if (read < 0) {
                LOG_ERROR(COMMON, "Unable to decompress {}: {}", dest_path, archive_error_string(reader));
                return false;
            }

            if (out_file.write(buffer.data(), static_cast<std::uint64_t>(read))
                != static_cast<std::uint64_t>(read)) {
                LOG_ERROR(COMMON, "Unable to write extracted file to {}", dest_path);
                return false;
            }
        }
    }

    // Reads the data of the entry the reader is on. already_read is what the caller has read from this
    // archive before it, so that max_size can cap a whole archive as well as a single entry.
    static bool read_entry_content(struct archive *reader, const archive_entry_info &info,
        const std::string &path, const std::uint64_t already_read, const std::uint64_t max_size,
        std::vector<char> &content_out) {
        // Not every format fills the declared size in, so grow as it arrives.
        std::vector<char> buffer(ARCHIVE_READ_BLOCK_SIZE);

        for (;;) {
            const la_ssize_t read = archive_read_data(reader, buffer.data(), buffer.size());

            if (read == 0) {
                return true;
            }

            if (read < 0) {
                LOG_ERROR(COMMON, "Unable to decompress {} out of {}: {}", info.path, path,
                    archive_error_string(reader));
                return false;
            }

            if (max_size && (already_read + content_out.size() + static_cast<std::uint64_t>(read) > max_size)) {
                LOG_ERROR(COMMON, "Reading {} out of {} goes over the {} bytes allowed", info.path, path,
                    max_size);
                return false;
            }

            content_out.insert(content_out.end(), buffer.begin(), buffer.begin() + read);
        }
    }

    bool read_archive_entry(const std::string &path, const std::string &entry_path,
        std::vector<char> &content_out, const std::uint64_t max_size) {
        archive_ptr reader = open_archive(path);

        if (!reader) {
            return false;
        }

        struct archive_entry *entry = nullptr;
        int result = ARCHIVE_OK;

        while ((result = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
            const archive_entry_info info = describe_entry(entry);

            if (info.is_directory || (common::compare_ignore_case(info.path.c_str(), entry_path.c_str()) != 0)) {
                continue;
            }

            content_out.clear();
            return read_entry_content(reader.get(), info, path, 0, max_size, content_out);
        }

        if (result != ARCHIVE_EOF) {
            LOG_ERROR(COMMON, "Unable to read the contents of archive {}: {}", path,
                archive_error_string(reader.get()));
        }

        return false;
    }

    bool extract_archive(const std::string &path,
        const std::function<std::string(const archive_entry_info &)> &choose_destination,
        progress_changed_callback progress_cb, cancel_requested_callback cancel_cb) {
        archive_ptr reader = open_archive(path);

        if (!reader) {
            return false;
        }

        // Progress is reported against the whole archive rather than the selected subset: the caller may
        // skip entries, but the reader still has to stream past them, and that is where the time goes.
        std::uint64_t total_bytes = 0;

        if (progress_cb) {
            std::vector<archive_entry_info> all_entries;

            if (list_archive(path, all_entries)) {
                for (const archive_entry_info &entry : all_entries) {
                    total_bytes += entry.size;
                }
            }
        }

        struct archive_entry *entry = nullptr;
        int result = ARCHIVE_OK;
        std::uint64_t done_bytes = 0;

        while ((result = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
            if (cancel_cb && cancel_cb()) {
                return false;
            }

            const archive_entry_info info = describe_entry(entry);
            const std::string dest = choose_destination(info);

            if (info.is_directory) {
                if (!dest.empty()) {
                    common::create_directories(dest);
                }
            } else if (!dest.empty()) {
                if (!write_entry_to_file(reader.get(), dest)) {
                    return false;
                }
            }

            // Skipped entries count towards the bar as well: the reader still had to stream past them,
            // and leaving them out makes it stall for as long as that takes.
            done_bytes += info.size;

            if (progress_cb && total_bytes) {
                progress_cb(static_cast<std::size_t>(done_bytes), static_cast<std::size_t>(total_bytes));
            }
        }

        if (result != ARCHIVE_EOF) {
            LOG_ERROR(COMMON, "Unable to read the contents of archive {}: {}", path,
                archive_error_string(reader.get()));
            return false;
        }

        return true;
    }

    bool read_archive(const std::string &path, std::vector<archive_file> &files_out,
        const std::uint64_t max_size) {
        archive_ptr reader = open_archive(path);

        if (!reader) {
            return false;
        }

        struct archive_entry *entry = nullptr;
        int result = ARCHIVE_OK;
        std::uint64_t total_read = 0;

        files_out.clear();

        while ((result = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
            archive_file file;
            file.info = describe_entry(entry);

            if (!file.info.is_directory) {
                if (!read_entry_content(reader.get(), file.info, path, total_read, max_size, file.content)) {
                    return false;
                }

                total_read += file.content.size();
            }

            files_out.push_back(std::move(file));
        }

        if (result != ARCHIVE_EOF) {
            LOG_ERROR(COMMON, "Unable to read the contents of archive {}: {}", path,
                archive_error_string(reader.get()));
            return false;
        }

        return true;
    }

    struct archive_writer_freer {
        void operator()(struct archive *a) const {
            archive_write_free(a);
        }
    };

    struct archive_entry_freer {
        void operator()(struct archive_entry *entry) const {
            archive_entry_free(entry);
        }
    };

    bool write_zip_archive(const std::string &path, const std::vector<archive_file> &files) {
        std::unique_ptr<struct archive, archive_writer_freer> writer(archive_write_new());

        if (!writer) {
            return false;
        }

        // The last block is not padded: zero bytes after the end of central directory record are not
        // what readers that look for that record from the end of the file expect.
        if ((archive_write_set_format_zip(writer.get()) != ARCHIVE_OK)
            || (archive_write_set_options(writer.get(), "zip:compression=deflate") != ARCHIVE_OK)
            || (archive_write_set_bytes_in_last_block(writer.get(), 1) != ARCHIVE_OK)) {
            LOG_ERROR(COMMON, "Unable to set up a zip writer: {}", archive_error_string(writer.get()));
            return false;
        }

#ifdef _WIN32
        const std::u16string path_u16 = common::utf8_to_ucs2(path);
        const int result = archive_write_open_filename_w(writer.get(),
            reinterpret_cast<const wchar_t *>(path_u16.c_str()));
#else
        const int result = archive_write_open_filename(writer.get(), path.c_str());
#endif

        if (result != ARCHIVE_OK) {
            LOG_ERROR(COMMON, "Unable to create archive {}: {}", path, archive_error_string(writer.get()));
            return false;
        }

        for (const archive_file &file : files) {
            std::unique_ptr<struct archive_entry, archive_entry_freer> entry(archive_entry_new());

            if (!entry) {
                return false;
            }

            const la_int64_t size = file.info.is_directory ? 0 : static_cast<la_int64_t>(file.content.size());

            archive_entry_set_pathname_utf8(entry.get(), file.info.path.c_str());
            archive_entry_set_filetype(entry.get(), file.info.is_directory ? AE_IFDIR : AE_IFREG);
            archive_entry_set_perm(entry.get(), file.info.is_directory ? 0755 : 0644);
            archive_entry_set_size(entry.get(), size);

            if (archive_write_header(writer.get(), entry.get()) != ARCHIVE_OK) {
                LOG_ERROR(COMMON, "Unable to add {} to archive {}: {}", file.info.path, path,
                    archive_error_string(writer.get()));
                return false;
            }

            if (size && (archive_write_data(writer.get(), file.content.data(), file.content.size()) != size)) {
                LOG_ERROR(COMMON, "Unable to write {} into archive {}: {}", file.info.path, path,
                    archive_error_string(writer.get()));
                return false;
            }
        }

        // The central directory is written on close, so a failure there is a failed write too.
        if (archive_write_close(writer.get()) != ARCHIVE_OK) {
            LOG_ERROR(COMMON, "Unable to finish archive {}: {}", path, archive_error_string(writer.get()));
            return false;
        }

        return true;
    }
}

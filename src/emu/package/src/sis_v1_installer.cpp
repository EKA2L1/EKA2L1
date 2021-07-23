/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <loader/sis.h>
#include <loader/sis_old.h>

#include <package/sis_v1_installer.h>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/flate.h>
#include <common/log.h>
#include <common/path.h>

#include <cwctype>
#include <vfs/vfs.h>

namespace eka2l1::loader {
    bool install_sis_old(const std::u16string &path, io_system *io, drive_number drive, package::object &info, progress_changed_callback progress_cb, cancel_requested_callback cancel_cb) {
        std::optional<sis_old> res = *loader::parse_sis_old(common::ucs2_to_utf8(path));
        if (!res.has_value()) {
            return false;
        }

        info.drives = 1 << (drive - drive_a);
        info.uid = res->header.uid1;
        info.package_name = res->comp_names[0];
        info.vendor_name = u"Unknown";
        info.install_type = package::install_type_normal_install;

        info.supported_language_ids.resize(res->langs.size());
        for (std::size_t i = 0; i < info.supported_language_ids.size(); i++) {
            info.supported_language_ids[i] = static_cast<std::int32_t>(res->langs[i]);
        }

        info.current_drives = info.drives;
        info.is_removable = true;

        std::size_t total_size = 0;
        std::size_t decomped = 0;
        for (auto &file : res->files) {
            total_size += file.record.org_file_len;
        }

        if (res->files.empty()) {
            if (progress_cb)
                progress_cb(1, 1);
        }

        std::size_t processed = 0;
        bool canceled = false;

        for (; processed < res->files.size(); processed++) {
            loader::sis_old_file &file = res->files[processed];
            if (cancel_cb && cancel_cb()) {
                canceled = true;
                break;
            }

            std::u16string dest = file.dest;

            if (dest.find(u"!") != std::u16string::npos) {
                dest[0] = drive_to_char16(drive);
            }

            if (file.record.file_type != 1 && dest != u"") {
                std::string rp = eka2l1::file_directory(common::ucs2_to_utf8(dest));
                io->create_directories(common::utf8_to_ucs2(rp));
            } else {
                continue;
            }

            package::file_description desc;
            desc.operation = static_cast<std::int32_t>(loader::ss_op::install);
            desc.target = dest;

            symfile f = io->open_file(dest, WRITE_MODE | BIN_MODE);

            LOG_TRACE(PACKAGE, "Installing file {}", common::ucs2_to_utf8(dest));

            size_t left = file.record.len;
            size_t chunk = 0x2000;

            std::vector<char> temp;
            temp.resize(chunk);

            std::vector<char> inflated;
            inflated.resize(0x100000);

            FILE *sis_file = fopen(common::ucs2_to_utf8(path).data(), "rb");
            fseek(sis_file, file.record.ptr, SEEK_SET);

            mz_stream stream;

            stream.zalloc = nullptr;
            stream.zfree = nullptr;

            if (!(res->header.op & 0x8)) {
                if (inflateInit(&stream) != MZ_OK) {
                    return false;
                }
            }

            desc.uncompressed_length = left;
            desc.operation_options = 0;
            desc.index = static_cast<std::uint32_t>(info.file_descriptions.size());
            desc.sid = 0;

            while (left > 0) {
                if (cancel_cb && cancel_cb()) {
                    canceled = true;
                    break;
                }

                size_t took = left < chunk ? left : chunk;
                size_t readed = fread(temp.data(), 1, took, sis_file);

                if (readed != took) {
                    fclose(sis_file);
                    f->close();

                    canceled = true;

                    break;
                }

                if (res->header.op & 0x8) {
                    f->write_file(temp.data(), 1, static_cast<uint32_t>(took));
                    decomped += took;
                } else {
                    uint32_t inf;
                    flate::inflate_data(&stream, temp.data(), inflated.data(), static_cast<uint32_t>(took), &inf);

                    f->write_file(inflated.data(), 1, inf);
                    decomped += inf;
                }

                if (progress_cb) {
                    if (total_size != 0) {
                        progress_cb(decomped, total_size);
                    } else {
                        progress_cb(1, 1);
                    }
                }

                left -= took;
            }

            if (!(res->header.op & 0x8))
                inflateEnd(&stream);

            if (canceled) {
                common::remove(common::ucs2_to_utf8(dest));
                break;
            }

            info.file_descriptions.push_back(std::move(desc));
        }

        if (canceled) {
            for (std::size_t i = 0; i < processed; i++) {
                common::remove(common::ucs2_to_utf8(info.file_descriptions[i].target));
            }

            return false;
        }

        return true;
    }
}

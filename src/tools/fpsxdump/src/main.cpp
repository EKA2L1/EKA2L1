/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <loader/fpsx.h>

#include <common/buffer.h>
#include <common/cvt.h>
#include <common/log.h>

int main(int argc, char **argv) {
    eka2l1::log::setup_log(nullptr);

    if (argc <= 1) {
        LOG_ERROR(eka2l1::SYSTEM, "No file provided!");
        LOG_INFO(eka2l1::SYSTEM, "Usage: fpsxdump [filename].");

        return -1;
    }

    const char *target_gdr = argv[1];

    eka2l1::common::ro_std_file_stream stream(target_gdr, true);
    std::optional<eka2l1::loader::firmware::fpsx_header> header = eka2l1::loader::firmware::read_fpsx_header(stream);

    if (!header.has_value()) {
        LOG_ERROR(eka2l1::SYSTEM, "Error while parsing FPSX file!");
        return -2;
    }

    std::ofstream rom_stream("SYM.ROM", std::ios_base::binary);

    std::uint32_t ignore_bootstrap_left = 0xC00;

    for (auto &root: header->btree_.roots_) {
        if (root.myself_.btype_ == eka2l1::loader::firmware::BLOCK_TYPE_ROFS_HASH) {
            eka2l1::loader::firmware::block_header_rofs_hash &header_rofs = 
                static_cast<decltype(header_rofs)>(*root.myself_.header_);

            const std::string stt = header_rofs.description_;
            if (stt.find("CORE") != std::string::npos) {
                for (std::size_t i = 0; i < root.code_blocks_.size(); i++) {
                    std::uint32_t skip_taken = 0;

                    if (ignore_bootstrap_left != 0) {
                        if (root.code_blocks_[i].header_->data_size_ <= ignore_bootstrap_left) {
                            skip_taken = std::min<std::uint32_t>(ignore_bootstrap_left, root.code_blocks_[i].header_->data_size_);

                            if (skip_taken == root.code_blocks_[i].header_->data_size_)
                                continue;
                        } else {
                            skip_taken = ignore_bootstrap_left;
                        }
                    }

                    stream.seek(root.code_blocks_[i].data_offset_in_stream_ + skip_taken, eka2l1::common::seek_where::beg);

                    std::vector<char> buf;
                    buf.resize(root.code_blocks_[i].header_->data_size_ - skip_taken);

                    stream.read(buf.data(), buf.size());
                    rom_stream.write(buf.data(), buf.size());

                    ignore_bootstrap_left -= skip_taken;
                }
            }
        }
    }

    return 0;
}

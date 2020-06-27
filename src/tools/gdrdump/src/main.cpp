/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <loader/gdr.h>

#include <common/buffer.h>
#include <common/cvt.h>
#include <common/log.h>

void print_header_info(eka2l1::loader::gdr::header &the_header) {
    LOG_INFO("=============== HEADER INFO ===================");
    LOG_INFO("- UID1:               0x{:X}", the_header.store_write_once_layout_uid_);
    LOG_INFO("- UID2:               0x{:X}", the_header.font_store_file_uid_);
    LOG_INFO("- UID3:               0x{:X}", the_header.null_uid_);
    LOG_INFO("- UID Checksum:       0x{:X}", the_header.font_store_file_checksum_);
    LOG_INFO("- FNTTRAN version:    {}.{}.{}", static_cast<std::uint8_t>(the_header.fnt_tran_version_ >> 24), static_cast<std::uint8_t>(the_header.fnt_tran_version_ >> 16) & 0xFF,
        the_header.fnt_tran_version_ & 0xFFFF);

    LOG_INFO("- Pixel aspect ratio: {}", the_header.pixel_aspect_ratio_);
    LOG_INFO("- Copyright info:");

    for (std::size_t i = 0; i < the_header.copyright_strings_.size(); i++) {
        LOG_INFO("\t{}", eka2l1::common::ucs2_to_utf8(the_header.copyright_strings_[i]));
    }
}

void print_typeface_list_header_info(std::vector<eka2l1::loader::gdr::typeface> &typefaces) {
    LOG_INFO("=============== TYPEFACE LIST ======================");
    
    for (std::size_t i = 0; i < typefaces.size(); i++) {
        LOG_INFO("- {}:", eka2l1::common::ucs2_to_utf8(typefaces[i].header_.name_));
        LOG_INFO("\t+ Flags: {}", typefaces[i].header_.flags_);
        LOG_INFO("\t+ Font bitmap header:");
    }
}

int main(int argc, char **argv) {
    eka2l1::log::setup_log(nullptr);

    if (argc <= 1) {
        LOG_ERROR("No file provided!");
        LOG_INFO("Usage: gdrdump [filename].");

        return -1;
    }

    const char *target_gdr = argv[1];

    eka2l1::common::ro_std_file_stream stream(target_gdr, true);
    eka2l1::loader::gdr::file_store store;

    if (!eka2l1::loader::gdr::parse_store(reinterpret_cast<eka2l1::common::ro_stream*>(&stream), store)) {
        LOG_ERROR("Error while parsing GDR file!");
        return -2;
    }

    print_header_info(store.header_);
    print_typeface_list_header_info(store.typefaces_);

    return 0;
}

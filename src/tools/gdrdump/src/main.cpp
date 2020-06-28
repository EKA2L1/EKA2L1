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
        for (std::size_t j = 0; j < typefaces[i].header_.bitmap_headers_.size(); j++) {
            LOG_INFO("\t\t* UID: 0x{:X}", typefaces[i].header_.bitmap_headers_[j].uid_);
            LOG_INFO("\t\t* Factor: ({}, {})", typefaces[i].header_.bitmap_headers_[j].width_factor_,
                typefaces[i].header_.bitmap_headers_[j].height_factor_);
            LOG_INFO("");
        }
    }
}

void print_bitmap_font_info(std::vector<eka2l1::loader::gdr::font_bitmap> &fonts) {
    LOG_INFO("============ FONT BITMAP LIST =============");

    for (std::size_t i = 0; i < fonts.size(); i++) {
        LOG_INFO("- UID:                    0x{:X}", fonts[i].header_.uid_);
        LOG_INFO("- Ascent:                 {}", fonts[i].header_.ascent_in_pixels_);
        LOG_INFO("- Bitmap encoding:        {}", fonts[i].header_.bitmap_encoding_);
        LOG_INFO("- Cell height:            {}", fonts[i].header_.cell_height_in_pixels_);
        LOG_INFO("- Proportional:           {}", fonts[i].header_.is_proportional_);
        LOG_INFO("- Max char width:         {}", fonts[i].header_.max_char_width_in_pixels_);
        LOG_INFO("- Max normal char width:  {}", fonts[i].header_.max_normal_char_width_in_pixels_);
        LOG_INFO("- Posture:                {}", fonts[i].header_.posture_);
        LOG_INFO("- Stroke weight:          {}", fonts[i].header_.stroke_weight_);
        LOG_INFO("");
        
        for (std::size_t j = 0; j < fonts[i].code_sections_.size(); j++) {
            for (std::size_t k = 0; k < fonts[i].code_sections_[j].chars_.size(); k++) {
                LOG_INFO("- Character code {} bitmap:", fonts[i].code_sections_[j].header_.start_ + k);

                // Print this character
                eka2l1::loader::gdr::character &the_char = fonts[i].code_sections_[j].chars_[k];

                if (!the_char.metric_) {
                    LOG_INFO("\tNone");
                    continue;
                }

                const std::uint16_t target_width = the_char.metric_->move_in_pixels_ -
                    ((the_char.metric_->right_adjust_in_pixels_ == 0xFF) ? 0 : the_char.metric_->right_adjust_in_pixels_)
                    - the_char.metric_->left_adj_in_pixels_;

                for (std::uint16_t y = 0; y < the_char.metric_->height_in_pixels_; y++) { 
                    std::string buf;

                    for (std::uint16_t x = 0; x < target_width; x++) {
                        std::uint32_t idx = (y * target_width) + x;
                        buf += (the_char.data_[idx >> 5] & (1 << (idx & 31))) ? '5' : '-';
                    }

                    LOG_INFO("\t{}", buf);
                }

                LOG_INFO("");
            }    
        }

        LOG_INFO("");
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
    print_bitmap_font_info(store.font_bitmaps_);

    return 0;
}

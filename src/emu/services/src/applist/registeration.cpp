/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <services/applist/applist.h>
#include <services/fbs/fbs.h>

#include <common/benchmark.h>
#include <common/buffer.h>
#include <common/path.h>
#include <common/uid.h>

#include <utils/cardinality.h>
#include <utils/consts.h>

namespace eka2l1 {
    static bool read_str16_aligned(common::ro_stream *stream, std::u16string &dat) {
        char len = 0;

        // Read string length
        if (stream->read(&len, sizeof(len)) != sizeof(len)) {
            return false;
        }

        if (len == 0) {
            dat = u"";
            return true;
        }

        // Align if necessary. Required align by 2.
        if (stream->tell() % 2 != 0) {
            stream->seek(1, common::seek_where::cur);
        }

        // Read the string
        dat.resize(len);
        if (stream->read(&dat[0], 2 * dat.length()) != 2 * dat.length()) {
            return false;
        }

        return true;
    }

    static bool read_non_localisable_info(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive) {
        if (!read_str16_aligned(stream, reg.localised_info_rsc_path)) {
            return false;
        }

        if (stream->read(&reg.localised_info_rsc_id, 4) != 4) {
            return false;
        }

        if (stream->read(&reg.caps.is_hidden, 1) != 1) {
            return false;
        }

        if (stream->read(&reg.caps.ability, 1) != 1) {
            return false;
        }

        if (stream->read(&reg.caps.support_being_asked_to_create_new_file, 1) != 1) {
            return false;
        }

        if (stream->read(&reg.caps.launch_in_background, 1) != 1) {
            return false;
        }

        std::u16string group_name;
        if (!read_str16_aligned(stream, group_name)) {
            return false;
        }

        reg.caps.group_name = group_name;

        if (stream->read(&reg.default_screen_number, 1) != 1) {
            return false;
        }

        // TODO: Read MIME infos, and file ownership lists

        return true;
    }

    static bool read_mandatory_info(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive) {
        // Skip over reserved variables
        stream->seek(8, common::seek_where::beg);
        std::u16string app_file;

        if (!read_str16_aligned(stream, app_file)) {
            return false;
        }

        // Read attributes
        if (stream->read(&reg.caps.flags, sizeof(reg.caps.flags)) != sizeof(reg.caps.flags)) {
            return false;
        }

        std::u16string binary_name = eka2l1::filename(app_file);

        if (binary_name.back() == u'\0') {
            binary_name.pop_back();
        }

        if (reg.caps.flags & apa_capability::non_native) {
            reg.mandatory_info.app_path = std::u16string(1, drive_to_char16(land_drive)) + eka2l1::relative_path(app_file);
        } else if (reg.caps.flags & apa_capability::built_as_dll) {
            reg.mandatory_info.app_path = std::u16string(1, drive_to_char16(land_drive)) + u"\\:system\\programs\\"
                + binary_name + u".dll";
        } else {
            // Compatibility with old EKA1
            reg.mandatory_info.app_path = std::u16string(1, drive_to_char16(land_drive)) + u":\\system\\programs\\"
                + binary_name + u".exe";
        }

        return true;
    }

    bool read_registeration_info(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive) {
        if (!read_mandatory_info(stream, reg, land_drive)) {
            return false;
        }

        if (!read_non_localisable_info(stream, reg, land_drive)) {
            // Only mandatory is nnecessary Others are ok to missing.
            return true;
        }

        return true;
    }

    bool read_localised_registration_info(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive) {
        // Skip over reserved variables
        stream->seek(8, common::seek_where::beg);
        std::u16string cap;

        if (!read_str16_aligned(stream, cap)) {
            return false;
        }

        reg.mandatory_info.short_caption = cap;

        stream->seek(8, common::seek_where::cur);

        if (!read_str16_aligned(stream, cap)) {
            return false;
        }

        reg.mandatory_info.long_caption = cap;

        if (stream->read(&reg.icon_count, 2) != 2) {
            return false;
        }

        if (!read_str16_aligned(stream, cap)) {
            return false;
        }

        reg.icon_file_path = cap;

        // TODO: Read view list and localised group name

        return true;
    }
    
    bool read_registeration_info_aif(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive,
        const language lang) {
        // Read the first 3 UIDS
        epoc::uid_type uids;
        if (stream->read(&uids, sizeof(epoc::uid_type)) != sizeof(epoc::uid_type)) {
            return false;
        }

        static constexpr std::uint32_t DIRECT_STORE_UID = 0x10000037;
        static constexpr std::uint32_t AIF_UID_1 = 0x1000006A;
        static constexpr std::uint32_t AIF_UID_2 = 0x10003A38;

        if ((uids[0] != DIRECT_STORE_UID) || ((uids[1] != AIF_UID_1) && (uids[1] != AIF_UID_2))) {
            return false;
        }

        // Read the checksum. TODO: crc32
        std::uint32_t uid_checksum = 0;
        if (stream->read(&uid_checksum, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
            return false;
        }

        reg.mandatory_info.uid = uids[2];

        std::uint32_t header_pos = 0;
        
        if (stream->read(&header_pos, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
            return false;
        }

        stream->seek(header_pos, common::seek_where::beg);

        utils::cardinality count;

        auto read_caption_list_and_find_best = [&]() -> std::optional<std::u16string> {
            // Read caption offset table
            if (!count.internalize(*stream)) {
                return std::nullopt;
            }

            const std::size_t table1_pos = stream->tell();
            std::u16string the_caption;

            for (std::uint16_t i = 0; i < count.value(); i++) {
                std::uint32_t caption_string_offset = 0;
                if (stream->read(&caption_string_offset, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
                    return std::nullopt;
                }

                std::uint16_t lang_code = 0;
                if (stream->read(&lang_code, sizeof(std::uint16_t)) != sizeof(std::uint16_t)) {
                    return std::nullopt;
                }

                const std::size_t crr_pos = stream->tell();

                if ((the_caption.empty()) || (static_cast<language>(lang_code) == lang)) {
                    stream->seek(caption_string_offset, common::seek_where::beg);

                    if (!epoc::read_des_string(the_caption, stream, true)) {
                        return std::nullopt;
                    }

                    if (static_cast<language>(lang_code) == lang) {
                        break;
                    }
                }

                stream->seek(crr_pos, common::seek_where::beg);
            }

            stream->seek(table1_pos + count.value() * (sizeof(std::uint32_t) + sizeof(std::uint16_t)),
                common::seek_where::beg);

            return the_caption;
        };

        std::optional<std::u16string> cap = read_caption_list_and_find_best();
        
        if (!cap) {
            return false;
        }
        
        reg.mandatory_info.short_caption.assign(nullptr, cap.value());
        reg.mandatory_info.long_caption.assign(nullptr, cap.value());

        if (!count.internalize(*stream)) {
            return false;
        }

        // Skip the icon section, we will visit in in separate section
        stream->seek(count.value() * (sizeof(std::uint32_t) + sizeof(std::uint16_t)), common::seek_where::cur);

        if (!reg.caps.internalize(*stream)) {
            return false;
        }

        std::uint32_t version = 0;
        if (stream->read(&version, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
            // Pass this on, optional
            return true;
        }

        if (version == 0) {
            return true;
        }

        if (!count.internalize(*stream)) {
            return false;
        }

        for (std::uint32_t i = 0; i < count.value(); i++) {
            std::uint32_t data_type_string_offset = 0;
            if (stream->read(&data_type_string_offset, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
                return false;
            }

            std::int16_t priority = 0;
            if (stream->read(&priority, sizeof(std::int16_t)) != sizeof(std::int16_t)) {
                return false;
            }

            const std::size_t crr_pos = stream->tell();
            stream->seek(data_type_string_offset, common::seek_where::beg);

            data_type the_data_type;
            the_data_type.priority_ = priority;

            if (!epoc::read_des_string(the_data_type.type_, stream, false)) {
                return false;
            }

            stream->seek(crr_pos, common::seek_where::beg);
            reg.data_types.push_back(the_data_type);
        }

        if (version == 1) {
            return true;
        }

        if (!count.internalize(*stream)) {
            return false;
        }

        for (std::uint32_t i = 0; i < count.value(); i++) {
            std::uint32_t view_data_offset = 0;

            if (stream->read(&view_data_offset, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
                return false;
            }

            const std::size_t crr_pos = stream->tell();
            stream->seek(view_data_offset, common::seek_where::beg);

            view_data the_view_data;
            
            if (stream->read(&the_view_data.uid_, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
                return false;
            }

            if (stream->read(&the_view_data.screen_mode_, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
                return false;
            }

            if (stream->read(&the_view_data.icon_count_, sizeof(std::uint16_t)) != sizeof(std::uint16_t)) {
                return false;
            }

            std::optional<std::u16string> cap_string = read_caption_list_and_find_best();
            if (!cap_string) {
                return false;
            }
            
            the_view_data.caption_ = std::move(cap_string.value());
            
            stream->seek(crr_pos, common::seek_where::beg);
        }

        if (version == 2) {
            return true;
        }

        if (!count.internalize(*stream)) {
            return false;
        }

        reg.ownership_list.resize(count.value());

        for (std::uint32_t i = 0; i < count.value(); i++) {
            std::uint32_t file_ownership_string_offset = 0;

            if (stream->read(&file_ownership_string_offset, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
                return false;
            }
            
            const std::size_t crr_pos = stream->tell();
            stream->seek(file_ownership_string_offset, common::seek_where::beg);

            if (!epoc::read_des_string(reg.ownership_list[i], stream, true)) {
                return false;
            }

            stream->seek(crr_pos, common::seek_where::beg);
        }

        return true;
    }

    bool read_icon_data_aif(common::ro_stream *stream, fbs_server *serv, std::vector<apa_app_icon> &icon_list, const address rom_addr) {
        // Seek to header pos, over the UIDs
        stream->seek(4 * sizeof(std::uint32_t), common::seek_where::beg);
        std::uint32_t header_pos = 0;
        
        if (stream->read(&header_pos, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
            return false;
        }

        stream->seek(header_pos, common::seek_where::beg);

        utils::cardinality count;

        if (!count.internalize(*stream)) {
            return false;
        }

        // Skip these bytes to get to icon part
        stream->seek(count.value() * (sizeof(std::uint32_t) + sizeof(std::uint16_t)), common::seek_where::cur);

        // Read the data part
        if (!count.internalize(*stream)) {
            return false;
        }

        icon_list.resize(count.value() * 2);

        for (std::size_t i = 0; i < count.value(); i++) {
            std::uint32_t data_offset = 0;
            if (stream->read(&data_offset, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
                return false;
            }
            
            std::uint16_t num = 0;
            if (stream->read(&num, sizeof(std::uint16_t)) != sizeof(std::uint16_t)) {
                return false;
            }

            icon_list[i * 2].number_ = num;
            icon_list[i * 2 + 1].number_ = num;

            const std::size_t lastpos = stream->tell();
            stream->seek(data_offset, common::seek_where::beg);

            std::uint32_t assume_uid = 0;
            if (stream->read(&assume_uid, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
                return false;
            }

            stream->seek(sizeof(std::uint32_t) * -1, common::seek_where::cur);

            static constexpr std::uint32_t ROM_BITMAP_UID = 0x10000040;

            if (assume_uid == ROM_BITMAP_UID) {
                icon_list[i * 2].bmp_ = nullptr;
                icon_list[i * 2].bmp_rom_addr_ = rom_addr + data_offset;

                static constexpr std::size_t size_offset = offsetof(epoc::bitwise_bitmap, header_) +
                    offsetof(loader::sbm_header, bitmap_size);

                stream->seek(size_offset, common::seek_where::cur);
                std::uint32_t size_of_source = 0;

                if (stream->read(&size_of_source, 4) != 4ULL) {
                    return false;
                }

                icon_list[i * 2 + 1].bmp_ = nullptr;
                icon_list[i * 2 + 1].bmp_rom_addr_ = icon_list[i * 2].bmp_rom_addr_ + size_of_source;
            } else {
                auto read_and_create_bitmap = [&](const std::size_t index) { 
                    loader::sbm_header bmp_header;
                    if (!bmp_header.internalize(*stream)) {
                        return false;
                    }

                    icon_list[index].bmp_rom_addr_ = 0;

                    fbs_bitmap_data_info info;
                    info.size_ = bmp_header.size_pixels;
                    info.dpm_ = epoc::get_display_mode_from_bpp(bmp_header.bit_per_pixels);
                    info.comp_ = static_cast<epoc::bitmap_file_compression>(bmp_header.compression);

                    std::vector<std::uint8_t> data_to_read;
                    data_to_read.resize(bmp_header.bitmap_size - bmp_header.header_len);

                    if (stream->read(&data_to_read[0], data_to_read.size()) != data_to_read.size()) {
                        return false;
                    }

                    info.data_size_ = data_to_read.size();
                    info.data_ = data_to_read.data();

                    // Auto support dirty. TODO not hardcode
                    icon_list[index].bmp_ = serv->create_bitmap(info, true, false, false);
                    return true;
                };

                if (!read_and_create_bitmap(i * 2)) {
                    return false;
                }

                if (!read_and_create_bitmap(i * 2 + 1)) {
                    return false;
                }
            }

            stream->seek(lastpos, common::seek_where::beg);
        }

        return true;
    }
}

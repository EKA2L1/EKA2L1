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

        // Read caption offset table
        utils::cardinality count;
        if (!count.internalize(*stream)) {
            return false;
        }

        const std::size_t table1_pos = stream->tell();

        for (std::uint16_t i = 0; i < count.length(); i++) {
            std::uint32_t caption_string_offset = 0;
            if (stream->read(&caption_string_offset, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
                return false;
            }

            std::uint16_t lang_code = 0;
            if (stream->read(&lang_code, sizeof(std::uint16_t)) != sizeof(std::uint16_t)) {
                return false;
            }

            const std::size_t crr_pos = stream->tell();

            if ((reg.mandatory_info.short_caption.get_length() == 0) || (static_cast<language>(lang_code) == lang)) {
                stream->seek(caption_string_offset, common::seek_where::beg);
                
                std::u16string the_caption;
                if (!epoc::read_des_string(the_caption, stream, true)) {
                    return false;
                }
                
                reg.mandatory_info.short_caption.assign(nullptr, the_caption);
                reg.mandatory_info.long_caption.assign(nullptr, the_caption);

                if (static_cast<language>(lang_code) == lang) {
                    break;
                }
            }

            stream->seek(crr_pos, common::seek_where::beg);
        }

        stream->seek(table1_pos + count.length() * sizeof(std::uint16_t), common::seek_where::beg);

        return true;
    }
}

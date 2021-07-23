/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <common/buffer.h>
#include <common/cvt.h>

#include <loader/e32img.h>

#include <common/algorithm.h>
#include <common/buffer.h>
#include <common/bytepair.h>
#include <common/bytes.h>
#include <common/crypt.h>
#include <common/flate.h>
#include <common/log.h>

#include <utils/err.h>

#include <cstdio>
#include <miniz.h>
#include <sstream>

namespace eka2l1::loader {
    struct e32img_import_sec_header {
        uint32_t size;
        uint32_t num_reloc;
    };

    enum compress_type {
        byte_pair_c = 0x102822AA,
        deflate_c = 0x101F7AFC
    };

    static void dump_flag_info(const std::uint32_t flag) {
        const std::uint8_t fixed = static_cast<std::uint8_t>((flag >> 0x2) & 1);
        const std::uint16_t abi = static_cast<std::uint16_t>((flag >> 0x3) & 3);
        const std::uint16_t ept = static_cast<std::uint16_t>((flag >> 0x5) & 7);

        if (abi == 1) {
            LOG_INFO(LOADER, "ABI: EABI");
        } else {
            LOG_INFO(LOADER, "ABI: GCC98r2");
        }

        if (fixed) {
            LOG_INFO(LOADER, "Address type: Fixed");
        } else {
            LOG_INFO(LOADER, "Address type: None");
        }

        if (ept == 0) {
            LOG_INFO(LOADER, "Entry point type: EKA1");
        } else {
            LOG_INFO(LOADER, "Entry point type: EKA2");
        }
    }

    static void dump_image_info(const e32img &img) {
        LOG_INFO(LOADER, "Image dump info:");

        if (img.has_extended_header) {
            LOG_INFO(LOADER, "V-Format used, too tired \\_(-.-)_/");
        }

        dump_flag_info(img.header.flags);

        switch (img.header.cpu) {
        case e32_cpu::armv5:
            LOG_INFO(LOADER, "Detected: ARMv5");
            break;
        case e32_cpu::armv6:
            LOG_INFO(LOADER, "Detected: ARMv6");
            break;
        case e32_cpu::armv4:
            LOG_INFO(LOADER, "Detected: ARMv4");
            break;
        default:
            LOG_INFO(LOADER, "Invalid cpu specified in EKA2 Image Header. Maybe x86 or undetected");
            break;
        }

        LOG_INFO(LOADER, "Total dll count: {}", img.header.dll_ref_table_count);
        LOG_INFO(LOADER, "Import offsets: {}", img.header.import_offset);
    }

    static void read_relocations(common::ro_stream *stream, e32_reloc_section &section, uint32_t offset) {
        // No relocations
        if (offset == 0) {
            return;
        }

        stream->seek(offset, common::beg);

        stream->read(reinterpret_cast<void *>(&section.size), 4);
        stream->read(reinterpret_cast<void *>(&section.num_relocs), 4);

        if (section.size <= 8) {
            section.num_relocs = 0;
            return;
        }

        for (uint32_t i = 0; i < section.num_relocs; i++) {
            e32_reloc_entry reloc_entry;

            stream->read(reinterpret_cast<void *>(&reloc_entry.base), 4);
            stream->read(reinterpret_cast<void *>(&reloc_entry.size), 4);

            assert((reloc_entry.size) % 2 == 0);

            reloc_entry.rels_info.resize(((reloc_entry.size - 8) / 2));

            for (auto &rel_info : reloc_entry.rels_info) {
                stream->read(reinterpret_cast<void *>(&rel_info), 2);
            }

            i += static_cast<int>(reloc_entry.rels_info.size()) - 1;

            (!reloc_entry.rels_info.empty()) && (reloc_entry.rels_info.back() == 0) ? (i -= 1) : 0;
            section.entries.push_back(reloc_entry);
        }
    }

    static void parse_export_dir(e32img &img) {
        if (img.header.export_dir_offset == 0) {
            return;
        }

        uint32_t *exp = reinterpret_cast<uint32_t *>(img.data.data() + img.header.export_dir_offset);

        for (std::uint32_t i = 0; i < img.header.export_dir_count; i++) {
            img.ed.syms.push_back(*exp++);
        }
    }

    static void parse_iat(e32img &img) {
        uint32_t *imp_addr = reinterpret_cast<uint32_t *>(img.data.data() + img.header.code_offset + img.header.text_size);

        while (*imp_addr != 0) {
            img.iat.its.push_back(*imp_addr++);
        }
    }

    static constexpr std::uint32_t E32IMG_SIGNATURE = 0x434F5045;

    bool is_e32img(common::ro_stream *stream, std::uint32_t *uid_array) {
        std::uint32_t temp_arr[3];
        if (!uid_array) {
            uid_array = temp_arr;
        }

        std::uint32_t check = 0;
        std::uint32_t sig = 0;

        stream->read(&uid_array[0], 4);
        stream->read(&uid_array[1], 4);
        stream->read(&uid_array[2], 4);
        stream->read(&check, 4);
        stream->read(&sig, 4);

        const bool result = (sig == E32IMG_SIGNATURE);
        stream->seek(0, common::seek_where::beg);

        return result;
    }

    std::int32_t parse_e32img_header(common::ro_stream *stream, e32img_header &header, e32img_header_extended &extended,
        std::uint32_t &uncompressed_size, epocver &ver) {
        if (!stream) {
            return epoc::error_argument;
        }

        stream->seek(0, common::seek_where::beg);

        stream->read(&header.uid1, 4);
        stream->read(&header.uid2, 4);
        stream->read(&header.uid3, 4);
        stream->read(&header.check, 4);
        stream->read(&header.sig, 4);

        if (header.sig != E32IMG_SIGNATURE) {
            return epoc::error_corrupt;
        }

        std::uint32_t uid_type[3] = { static_cast<std::uint32_t>(header.uid1), header.uid2, header.uid3 };
        if (crypt::calculate_checked_uid_checksum(uid_type) != header.check) {
            return epoc::error_corrupt;
        }

        std::uint32_t temp = 0;
        stream->read(&temp, 4);

        if ((temp == 0x2000) || (temp == 0x1000)) {
            // Quick hack to determinate if this is an EKA1
            header.cpu = static_cast<loader::e32_cpu>(temp);
            ver = epocver::epoc6;

            stream->read(&temp, 4);
            stream->read(&header.compression_type, 4);

            stream->read(&header.petran_major, 1);
            stream->read(&header.petran_minor, 1);
            stream->read(&header.petran_build, 2);

            // NOTE (pent0): These 8 bytes are time.
            stream->read(&temp, 4);
            stream->read(&temp, 4);

            stream->read(&header.flags, 4);
            stream->read(&header.code_size, 4);
            stream->read(&header.data_size, 4);
            stream->read(&header.heap_size_min, 4);
            stream->read(&header.heap_size_max, 4);
            stream->read(&header.stack_size, 4);
            stream->read(&header.bss_size, 4);
            stream->read(&header.entry_point, 4);
            stream->read(&header.code_base, 4);
            stream->read(&header.data_base, 4);
            stream->read(&header.dll_ref_table_count, 4);
            stream->read(&header.export_dir_offset, 4);
            stream->read(&header.export_dir_count, 4);
            stream->read(&header.text_size, 4);
            stream->read(&header.code_offset, 4);
            stream->read(&header.data_offset, 4);
            stream->read(&header.import_offset, 4);
            stream->read(&header.code_reloc_offset, 4);
            stream->read(&header.data_reloc_offset, 4);

            std::uint32_t priority_val = 0;
            stream->read(&priority_val, 4);

            header.priority = static_cast<std::uint16_t>(priority_val);

            if (!(header.flags & 0xF000000)) {
                header.compression_type = 0;
            }
        } else {
            ver = epocver::epoc94;

            stream->seek(0, common::seek_where::beg);
            stream->read(&header, sizeof(e32img_header));
        }

        if (common::get_system_endian_type() == common::big_endian) {
            header.uid1 = static_cast<e32_img_type>(common::byte_swap(static_cast<std::uint32_t>(header.uid1)));
            header.uid2 = common::byte_swap(header.uid2);
            header.uid3 = common::byte_swap(header.uid3);
            header.check = common::byte_swap(header.check);
            header.sig = common::byte_swap(header.sig);
            header.petran_build = common::byte_swap(header.petran_build);
            header.flags = common::byte_swap(header.flags);
            header.code_size = common::byte_swap(header.code_size);
            header.data_size = common::byte_swap(header.data_size);
            header.heap_size_min = common::byte_swap(header.heap_size_min);
            header.stack_size = common::byte_swap(header.stack_size);
            header.bss_size = common::byte_swap(header.bss_size);
            header.entry_point = common::byte_swap(header.entry_point);
            header.code_base = common::byte_swap(header.code_base);
            header.data_base = common::byte_swap(header.data_base);
            header.dll_ref_table_count = common::byte_swap(header.dll_ref_table_count);
            header.export_dir_offset = common::byte_swap(header.export_dir_offset);
            header.export_dir_count = common::byte_swap(header.export_dir_count);
            header.text_size = common::byte_swap(header.text_size);
            header.code_offset = common::byte_swap(header.code_offset);
            header.data_offset = common::byte_swap(header.data_offset);
            header.code_reloc_offset = common::byte_swap(header.code_reloc_offset);
            header.data_reloc_offset = common::byte_swap(header.data_reloc_offset);
            header.priority = common::byte_swap(header.priority);
        }

        compress_type ctype = static_cast<compress_type>(header.compression_type);
        int header_format = (static_cast<int>(header.flags) >> 24) & 0xF;

        if (header_format > 0) {
            stream->read(&uncompressed_size, 4);

            if (header_format == 2) {
                stream->read(&extended.info, sizeof(e32img_vsec_info));
                stream->read(&extended.exception_des, 4);
                stream->read(&extended.spare2, 4);
                stream->read(&extended.export_desc_size, 2);
                stream->read(&extended.export_desc_type, 1);
                stream->read(&extended.export_desc, 1);

                if (common::get_system_endian_type() == common::big_endian) {
                    extended.exception_des = common::byte_swap(extended.exception_des);
                    extended.spare2 = common::byte_swap(extended.spare2);
                    extended.export_desc_size = common::byte_swap(extended.export_desc_size);
                    extended.info.cap1 = common::byte_swap(extended.info.cap1);
                    extended.info.cap2 = common::byte_swap(extended.info.cap2);
                    extended.info.secure_id = common::byte_swap(extended.info.secure_id);
                    extended.info.vendor_id = common::byte_swap(extended.info.vendor_id);
                }
            }

            if (common::get_system_endian_type() == common::big_endian) {
                uncompressed_size = common::byte_swap(uncompressed_size);
            }
        }

        return epoc::error_none;
    }

    std::optional<e32img> parse_e32img(common::ro_stream *stream, bool read_reloc) {
        if (!stream) {
            return std::nullopt;
        }

        e32img img;
        const std::size_t file_size = stream->size();

        if (parse_e32img_header(stream, img.header, img.header_extended, img.uncompressed_size, img.epoc_ver) != epoc::error_none) {
            return std::nullopt;
        }

        int header_format = (static_cast<int>(img.header.flags) >> 24) & 0xF;
        if (header_format > 0) {
            img.has_extended_header = true;
        }

        compress_type ctype = static_cast<compress_type>(img.header.compression_type);

        if (img.header.compression_type > 0) {
            img.data.resize(img.uncompressed_size + img.header.code_offset);

            std::uint32_t start_compress = img.header.code_offset;

            if (img.epoc_ver <= epocver::eka2) {
                // The code offset includes size
                start_compress += 4;
            }

            stream->seek(0, common::seek_where::beg);
            stream->read(img.data.data(), img.header.code_offset);

            std::vector<char> temp_buf(file_size - start_compress);

            stream->seek(start_compress, common::seek_where::beg);
            size_t bytes_read = stream->read(temp_buf.data(), static_cast<uint32_t>(temp_buf.size()));

            if (bytes_read != temp_buf.size()) {
                LOG_ERROR(LOADER, "File reading improperly");
            }

            if (ctype == compress_type::deflate_c) {
                flate::bit_input input(reinterpret_cast<uint8_t *>(temp_buf.data()),
                    static_cast<int>(temp_buf.size() * 8));

                flate::inflater inflate_machine(input);

                inflate_machine.init();
                auto read = inflate_machine.read(reinterpret_cast<uint8_t *>(&img.data[img.header.code_offset]),
                    img.uncompressed_size);
            } else if (ctype == compress_type::byte_pair_c) {
                auto crr_pos = stream->tell();

                std::vector<char> temp(stream->size() - img.header.code_offset);
                stream->seek(img.header.code_offset, common::seek_where::beg);
                stream->read(temp.data(), static_cast<uint32_t>(temp.size()));

                common::ro_buf_stream raw_bp_stream(reinterpret_cast<std::uint8_t *>(&temp[0]), temp.size());
                common::ibytepair_stream bpstream(reinterpret_cast<common::ro_stream *>(&raw_bp_stream));

                auto codesize = bpstream.read_pages(&img.data[img.header.code_offset], img.header.code_size);
                auto restsize = bpstream.read_pages(&img.data[img.header.code_offset + img.header.code_size], img.uncompressed_size);
            }
        } else {
            img.uncompressed_size = static_cast<uint32_t>(file_size);

            img.data.resize(file_size);
            stream->seek(0, common::seek_where::beg);
            stream->read(img.data.data(), static_cast<uint32_t>(img.data.size()));
        }

        const std::uint32_t import_export_table_size = img.header.code_size - img.header.text_size;

        parse_export_dir(img);
        parse_iat(img);

        // dump_image_info(img);

        common::ro_buf_stream decompressed_stream(reinterpret_cast<std::uint8_t *>(&img.data[0]),
            img.data.size());

        decompressed_stream.seek(img.header.import_offset, common::seek_where::beg);
        decompressed_stream.read(reinterpret_cast<void *>(&img.import_section.size), 4);

        img.import_section.imports.resize(img.header.dll_ref_table_count);

        for (auto &import : img.import_section.imports) {
            decompressed_stream.read(reinterpret_cast<void *>(&import.dll_name_offset), 4);
            decompressed_stream.read(reinterpret_cast<void *>(&import.number_of_imports), 4);

            img.dll_names.push_back(import.dll_name);

            if (import.number_of_imports == 0) {
                continue;
            }

            const auto crr_size = decompressed_stream.tell();
            decompressed_stream.seek(img.header.import_offset + import.dll_name_offset, common::beg);

            char temp = 1;

            while (temp != 0) {
                decompressed_stream.read(&temp, 1);

                if (temp != 0) {
                    import.dll_name += temp;
                }
            }

            decompressed_stream.seek(static_cast<uint32_t>(crr_size), common::beg);

            import.ordinals.resize(import.number_of_imports);

            for (auto &oridinal : import.ordinals) {
                decompressed_stream.read(reinterpret_cast<void *>(&oridinal), 4);
            }
        }

        if (read_reloc) {
            read_relocations(reinterpret_cast<common::ro_stream *>(&decompressed_stream),
                img.code_reloc_section, img.header.code_reloc_offset);
            read_relocations(reinterpret_cast<common::ro_stream *>(&decompressed_stream),
                img.data_reloc_section, img.header.data_reloc_offset);
        }

        return img;
    }
}

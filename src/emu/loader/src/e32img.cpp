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
#include <common/flate.h>
#include <common/log.h>

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
            LOG_INFO("ABI: EABI");
        } else {
            LOG_INFO("ABI: GCC98r2");
        }

        if (fixed) {
            LOG_INFO("Address type: Fixed");
        } else {
            LOG_INFO("Address type: None");
        }

        if (ept == 0) {
            LOG_INFO("Entry point type: EKA1");
        } else {
            LOG_INFO("Entry point type: EKA2");
        }
    }

    static void dump_image_info(const e32img &img) {
        LOG_INFO("Image dump info:");

        if (img.has_extended_header) {
            LOG_INFO("V-Format used, too tired \\_(-.-)_/");
        }

        dump_flag_info(img.header.flags);

        switch (img.header.cpu) {
        case e32_cpu::armv5:
            LOG_INFO("Detected: ARMv5");
            break;
        case e32_cpu::armv6:
            LOG_INFO("Detected: ARMv6");
            break;
        case e32_cpu::armv4:
            LOG_INFO("Detected: ARMv4");
            break;
        default:
            LOG_INFO("Invalid cpu specified in EKA2 Image Header. Maybe x86 or undetected");
            break;
        }

        LOG_INFO("Total dll count: {}", img.header.dll_ref_table_count);
        LOG_INFO("Import offsets: {}", img.header.import_offset);
    }

    static void read_relocations(common::ro_stream *stream, e32_reloc_section &section, uint32_t offset) {
        // No relocations
        if (offset == 0) {
            return;
        }

        stream->seek(offset, common::beg);

        stream->read(reinterpret_cast<void *>(&section.size), 4);
        stream->read(reinterpret_cast<void *>(&section.num_relocs), 4);

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

    std::optional<e32img> parse_e32img(common::ro_stream *stream, bool read_reloc) {
        if (!stream) {
            return std::nullopt;
        }

        e32img img;
        const std::size_t file_size = stream->size();

        stream->read(&img.header.uid1, 4);
        stream->read(&img.header.uid2, 4);
        stream->read(&img.header.uid3, 4);
        stream->read(&img.header.check, 4);
        stream->read(&img.header.sig, 4);

        if (img.header.sig != E32IMG_SIGNATURE) {
            return std::nullopt;
        }

        std::uint32_t temp = 0;
        stream->read(&temp, 4);

        if ((temp == 0x2000) || (temp == 0x1000)) {
            // Quick hack to determinate if this is an EKA1
            img.header.cpu = static_cast<loader::e32_cpu>(temp);
            img.epoc_ver = epocver::epoc6;

            stream->read(&temp, 4);
            stream->read(&temp, 4);
            stream->read(&img.header.petran_major, 1);
            stream->read(&img.header.petran_minor, 1);
            stream->read(&img.header.petran_build, 2);
            stream->read(&img.header.flags, 4);
            stream->read(&img.header.code_size, 4);
            stream->read(&img.header.data_size, 4);
            stream->read(&img.header.heap_size_min, 4);
            stream->read(&img.header.heap_size_max, 4);
            stream->read(&img.header.stack_size, 4);
            stream->read(&img.header.bss_size, 4);
            stream->read(&img.header.entry_point, 4);
            stream->read(&img.header.code_base, 4);
            stream->read(&img.header.data_base, 4);
            stream->read(&img.header.dll_ref_table_count, 4);
            stream->read(&img.header.export_dir_offset, 4);
            stream->read(&img.header.export_dir_count, 4);
            stream->read(&img.header.text_size, 4);
            stream->read(&img.header.code_offset, 4);
            stream->read(&img.header.data_offset, 4);
            stream->read(&img.header.import_offset, 4);
            stream->read(&img.header.code_reloc_offset, 4);
            stream->read(&img.header.data_reloc_offset, 4);
            stream->read(&img.header.priority, 2);

            img.header.compression_type = 1;
        } else {
            img.epoc_ver = epocver::epoc94;

            stream->seek(0, common::seek_where::beg);
            stream->read(&img.header, sizeof(e32img_header));
        }

        if (common::get_system_endian_type() == common::big_endian) {
            img.header.uid1 = static_cast<e32_img_type>(common::byte_swap(static_cast<std::uint32_t>(img.header.uid1)));
            img.header.uid2 = common::byte_swap(img.header.uid2);
            img.header.uid3 = common::byte_swap(img.header.uid3);
            img.header.check = common::byte_swap(img.header.check);
            img.header.sig = common::byte_swap(img.header.sig);
            img.header.petran_build = common::byte_swap(img.header.petran_build);
            img.header.flags = common::byte_swap(img.header.flags);
            img.header.code_size = common::byte_swap(img.header.code_size);
            img.header.data_size = common::byte_swap(img.header.data_size);
            img.header.heap_size_min = common::byte_swap(img.header.heap_size_min);
            img.header.stack_size = common::byte_swap(img.header.stack_size);
            img.header.bss_size = common::byte_swap(img.header.bss_size);
            img.header.entry_point = common::byte_swap(img.header.entry_point);
            img.header.code_base = common::byte_swap(img.header.code_base);
            img.header.data_base = common::byte_swap(img.header.data_base);
            img.header.dll_ref_table_count = common::byte_swap(img.header.dll_ref_table_count);
            img.header.export_dir_offset = common::byte_swap(img.header.export_dir_offset);
            img.header.export_dir_count = common::byte_swap(img.header.export_dir_count);
            img.header.text_size = common::byte_swap(img.header.text_size);
            img.header.code_offset = common::byte_swap(img.header.code_offset);
            img.header.data_offset = common::byte_swap(img.header.data_offset);
            img.header.code_reloc_offset = common::byte_swap(img.header.code_reloc_offset);
            img.header.data_reloc_offset = common::byte_swap(img.header.data_reloc_offset);
            img.header.priority = common::byte_swap(img.header.priority);
        }

        compress_type ctype = static_cast<compress_type>(img.header.compression_type);

        if (img.header.compression_type > 0) {
            int header_format = (static_cast<int>(img.header.flags) >> 24) & 0xF;
            stream->read(&img.uncompressed_size, 4);

            if (header_format == 2) {
                img.has_extended_header = true;

                stream->read(&img.header_extended.info, sizeof(e32img_vsec_info));
                stream->read(&img.header_extended.exception_des, 4);
                stream->read(&img.header_extended.spare2, 4);
                stream->read(&img.header_extended.export_desc_size, 2);
                stream->read(&img.header_extended.export_desc_type, 1);
                stream->read(&img.header_extended.export_desc, 1);

                if (common::get_system_endian_type() == common::big_endian) {
                    img.header_extended.exception_des = common::byte_swap(img.header_extended.exception_des);
                    img.header_extended.spare2 = common::byte_swap(img.header_extended.spare2);
                    img.header_extended.export_desc_size = common::byte_swap(img.header_extended.export_desc_size);
                    img.header_extended.info.cap1 = common::byte_swap(img.header_extended.info.cap1);
                    img.header_extended.info.cap2 = common::byte_swap(img.header_extended.info.cap2);
                    img.header_extended.info.secure_id = common::byte_swap(img.header_extended.info.secure_id);
                    img.header_extended.info.vendor_id = common::byte_swap(img.header_extended.info.vendor_id);
                }
            }

            if (common::get_system_endian_type() == common::big_endian) {
                img.uncompressed_size = common::byte_swap(img.uncompressed_size);
            }

            img.data.resize(img.uncompressed_size + img.header.code_offset);

            stream->seek(0, common::seek_where::beg);
            stream->read(img.data.data(), img.header.code_offset);

            std::vector<char> temp_buf(file_size - img.header.code_offset);

            stream->seek(img.header.code_offset, common::seek_where::beg);
            size_t bytes_read = stream->read(temp_buf.data(), static_cast<uint32_t>(temp_buf.size()));

            if (bytes_read != temp_buf.size()) {
                LOG_ERROR("File reading improperly");
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

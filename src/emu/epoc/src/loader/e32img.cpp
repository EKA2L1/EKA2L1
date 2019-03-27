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

#include <common/cvt.h>
#include <common/buffer.h>

#include <epoc/loader/e32img.h>

#include <common/algorithm.h>
#include <common/buffer.h>
#include <common/bytepair.h>
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

    static void dump_flag_info(int flag) {
        int fixed = (uint8_t)((flag >> 0x2) & 1);
        int abi = (uint16_t)((flag >> 0x3) & 3);
        int ept = (uint16_t)((flag >> 0x5) & 7);

        LOG_INFO("Image dump info");

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

    std::optional<e32img> parse_e32img(common::ro_stream *stream, bool read_reloc) {
        if (!stream) {
            return std::nullopt;
        }

        e32img img;

        auto file_size = stream->size();

        stream->read(&img.header.uid1, 4);
        stream->read(&img.header.uid2, 4);
        stream->read(&img.header.uid3, 4);
        stream->read(&img.header.check, 4);
        stream->read(&img.header.sig, 4);

        if (img.header.sig != 0x434F5045) {
            return std::nullopt;
        }

        uint32_t temp = 0;
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

        compress_type ctype = (compress_type)(img.header.compression_type);
        dump_flag_info((int)img.header.flags);

        if (img.header.compression_type > 0) {
            int header_format = ((int)img.header.flags >> 24) & 0xF;
            stream->read(&img.uncompressed_size, 4);

            if (header_format == 2) {
                img.has_extended_header = true;
                LOG_INFO("V-Format used, load more (too tired) \\_(-.-)_/");

                stream->read(&img.header_extended.info, sizeof(e32img_vsec_info));
                stream->read(&img.header_extended.exception_des, 4);
                stream->read(&img.header_extended.spare2, 4);
                stream->read(&img.header_extended.export_desc_size, 2);
                stream->read(&img.header_extended.export_desc_type, 1);
                stream->read(&img.header_extended.export_desc, 1);
            }

            img.data.resize(img.uncompressed_size + img.header.code_offset);

            stream->seek(0, common::seek_where::beg);
            stream->read(img.data.data(), img.header.code_offset);

            std::vector<char> temp_buf(file_size - img.header.code_offset);

            stream->seek(img.header.code_offset, common::seek_where::beg);
            size_t bytes_read = stream->read(temp_buf.data(), static_cast<uint32_t>(temp_buf.size()));

            if (bytes_read != temp_buf.size()) {
                LOG_ERROR("File reading unproperly");
            }

            if (ctype == compress_type::deflate_c) {
                flate::bit_input input(reinterpret_cast<uint8_t *>(temp_buf.data()),
                    static_cast<int>(temp_buf.size() * 8));

                flate::inflater inflate_machine(input);

                inflate_machine.init();
                auto readed = inflate_machine.read(reinterpret_cast<uint8_t *>(&img.data[img.header.code_offset]),
                    img.uncompressed_size);

                LOG_INFO("Readed compress, size: {}", readed);
            } else if (ctype == compress_type::byte_pair_c) {
                auto crr_pos = stream->tell();

                std::vector<char> temp(stream->size() - img.header.code_offset);
                stream->seek(img.header.code_offset, common::seek_where::beg);
                stream->read(temp.data(), static_cast<uint32_t>(temp.size()));
                stream->seek(crr_pos, common::seek_where::beg);

                common::ro_buf_stream raw_bp_stream(reinterpret_cast<std::uint8_t*>(&temp[0]), temp.size());
                common::ibytepair_stream bpstream(reinterpret_cast<common::ro_stream*>(&raw_bp_stream));

                auto codesize = bpstream.read_pages(&img.data[img.header.code_offset], img.header.code_size);
                auto restsize = bpstream.read_pages(&img.data[img.header.code_offset + img.header.code_size], img.uncompressed_size);
            }
        } else {
            img.uncompressed_size = static_cast<uint32_t>(file_size);

            img.data.resize(file_size);
            stream->seek(0, common::seek_where::beg);
            stream->read(img.data.data(), static_cast<uint32_t>(img.data.size()));
        }

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

        uint32_t import_export_table_size = img.header.code_size - img.header.text_size;
        LOG_TRACE("Import + export size: 0x{:x}", import_export_table_size);

        parse_export_dir(img);
        parse_iat(img);

        stream->seek(img.header.import_offset, common::seek_where::beg);
        stream->read(reinterpret_cast<void *>(&img.import_section.size), 4);

        img.import_section.imports.resize(img.header.dll_ref_table_count);

        LOG_INFO("Total dll count: {}", img.header.dll_ref_table_count);
        LOG_INFO("Import offsets: {}", img.header.import_offset);

        for (auto &import : img.import_section.imports) {
            stream->read(reinterpret_cast<void *>(&import.dll_name_offset), 4);
            stream->read(reinterpret_cast<void *>(&import.number_of_imports), 4);

            img.dll_names.push_back(import.dll_name);

            if (import.number_of_imports == 0) {
                continue;
            }

            auto crr_size = stream->tell();
            stream->seek(img.header.import_offset + import.dll_name_offset, common::beg);

            char temp = 1;

            while (temp != 0) {
                stream->read(&temp, 1);

                if (temp != 0) {
                    import.dll_name += temp;
                }
            }

            LOG_TRACE("Find dll import: {}, total import: {}.", import.dll_name.c_str(), import.number_of_imports);

            stream->seek(static_cast<uint32_t>(crr_size), common::beg);

            import.ordinals.resize(import.number_of_imports);

            for (auto &oridinal : import.ordinals) {
                stream->read(reinterpret_cast<void *>(&oridinal), 4);
            }
        }

        if (read_reloc) {
            read_relocations(stream,
                img.code_reloc_section, img.header.code_reloc_offset);

            read_relocations(stream,
                img.data_reloc_section, img.header.data_reloc_offset);
        }

        return img;
    }
}

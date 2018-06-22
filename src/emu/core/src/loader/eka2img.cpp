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

#include <loader/eka2img.h>
#include <loader/romimage.h>
#include <vfs.h>

#include <ptr.h>

#include <common/algorithm.h>
#include <common/buffer.h>
#include <common/bytepair.h>
#include <common/data_displayer.h>
#include <common/flate.h>
#include <common/log.h>

#include <core_kernel.h>

#include <cstdio>
#include <miniz.h>
#include <sstream>

namespace eka2l1 {
    namespace loader {
        struct eka2img_import_sec_header {
            uint32_t size;
            uint32_t num_reloc;
        };

        enum class relocation_type : uint16_t {
            reserved = 0x0000,
            text = 0x1000,
            data = 0x2000,
            inffered = 0x3000
        };

        enum compress_type {
            byte_pair_c = 0x102822AA,
            deflate_c = 0x101F7AFC
        };

        // Write simple relocation
        // Symbian only used this, as found on IDA
        bool write(uint32_t *data, uint32_t sym) {
            *data = sym;
            return true;
        }

        bool relocate(uint32_t *dest_ptr, relocation_type type, uint32_t code_delta, uint32_t data_delta) {
            if (type == relocation_type::reserved) {
                LOG_ERROR("Invalid relocation type: 0");

                return false;
            }

            // What is in it ?? :))
            uint32_t reloc_offset = *dest_ptr;

            switch (type) {
            case relocation_type::text:
                write(dest_ptr, reloc_offset + code_delta);
                break;
            case relocation_type::data:
                write(dest_ptr, reloc_offset + data_delta);
                break;
            case relocation_type::inffered:
            default:
                LOG_WARN("Relocation not properly handle: {}", (int)type);
                break;
            }

            return true;
        }

        // Given relocation entries, relocate the code and data
        bool relocate(std::vector<eka2_reloc_entry> entries,
            uint8_t *dest_addr,
            uint32_t code_delta,
            uint32_t data_delta) {
            for (uint32_t i = 0; i < entries.size(); i++) {
                auto entry = entries[i];

                for (auto &rel_info : entry.rels_info) {
                    // Get the lower 12 bit for virtual_address
                    uint32_t virtual_addr = entry.base + (rel_info & 0x0FFF);
                    uint8_t *dest_ptr = virtual_addr + dest_addr;

                    relocation_type rel_type = (relocation_type)(rel_info & 0xF000);

                    if (!relocate(reinterpret_cast<uint32_t *>(dest_ptr), rel_type, code_delta, data_delta)) {
                        LOG_WARN("Relocate fail at page: {}", i);
                    }
                }
            }

            return true;
        }

        std::string get_real_dll_name(std::string dll_name) {
            size_t dll_name_end_pos = dll_name.find_first_of("{");

            if (FOUND_STR(dll_name_end_pos)) {
                dll_name = dll_name.substr(0, dll_name_end_pos);
            } else {
                dll_name_end_pos = dll_name.find_last_of(".");

                if (FOUND_STR(dll_name_end_pos)) {
                    dll_name = dll_name.substr(0, dll_name_end_pos);
                }
            }

            return dll_name;
        }

        bool pe_fix_up_iat(memory_system *mem, hle::lib_manager &mngr, loader::eka2img &me, eka2img_import_block &import_block, eka2img_iat &iat, uint32_t &crr_idx) {
            const std::string dll_name8 = get_real_dll_name(import_block.dll_name);
            const std::u16string dll_name = std::u16string(dll_name8.begin(), dll_name8.end());

            loader::e32img_ptr img = mngr.load_e32img(dll_name);
            loader::romimg_ptr rimg = mngr.load_romimg(dll_name);

            uint32_t *imdir = &(import_block.ordinals[0]);
            uint32_t *expdir;

            uint32_t code_start;
            uint32_t code_end;
            uint32_t data_start;
            uint32_t data_end;

            uint32_t code_delta;
            uint32_t data_delta;

            if (!img && !rimg) {
                return false;
            } else {
                if (img) {
                    mngr.open_e32img(img);

                    code_start = img->rt_code_addr;
                    data_start = img->rt_data_addr;
                    code_end = code_start + img->header.code_size;
                    data_end = data_start + img->header.data_size;

                    code_delta = img->rt_code_addr - img->header.code_base;
                    data_delta = img->rt_data_addr - img->header.data_base;

                    expdir = img->ed.syms.data();

                    for (auto &exp : img->ed.syms) {
                        if (exp > img->header.data_offset && exp < img->header.data_offset + img->header.data_size)
                            exp += data_delta;
                        else
                            exp += code_delta;
                    }
                } else {
                    mngr.open_romimg(rimg);

                    code_start = rimg->header.code_address;
                    code_end = code_start + rimg->header.code_size;
                    data_start = rimg->header.data_address;
                    data_end = data_start + rimg->header.data_size + rimg->header.bss_size;

                    code_delta = 0;
                    data_delta = 0;

                    expdir = rimg->exports.data();
                }
            }

            for (uint32_t i = crr_idx, j = 0; i < crr_idx + import_block.ordinals.size(), j < import_block.ordinals.size(); i++, j++) {
                uint32_t iat_off = img->header.code_offset + img->header.code_size;
                img->data[iat_off + i * 4] = expdir[import_block.ordinals[j] - 1];
            }

            crr_idx += import_block.ordinals.size();

            return true;
        }

        bool elf_fix_up_import_dir(memory_system *mem, hle::lib_manager &mngr, loader::eka2img &me, eka2img_import_block &import_block) {
            LOG_INFO("Fixup for: {}", import_block.dll_name);

            const std::string dll_name8 = get_real_dll_name(import_block.dll_name);
            const std::u16string dll_name = std::u16string(dll_name8.begin(), dll_name8.end());

            loader::romimg_ptr rimg = mngr.load_romimg(dll_name);
            loader::e32img_ptr img;

            if (!rimg) {
                img = mngr.load_e32img(dll_name);
            }

            uint32_t *imdir = &(import_block.ordinals[0]);
            uint32_t *expdir;

            uint32_t code_start;
            uint32_t code_end;
            uint32_t data_start;
            uint32_t data_end;

            uint32_t code_delta;
            uint32_t data_delta;

            if (!img && !rimg) {
                LOG_WARN("Can't find image or rom image for: {}", dll_name8);
                return false;
            } else {
                if (img) {
                    mngr.open_e32img(img);

                    code_start = img->rt_code_addr;
                    data_start = img->rt_data_addr;
                    code_end = code_start + img->header.code_size;
                    data_end = data_start + img->header.data_size;

                    code_delta = img->rt_code_addr - img->header.code_base;
                    data_delta = img->rt_data_addr - img->header.data_base;

                    expdir = img->ed.syms.data();

                    for (auto &exp : img->ed.syms) {
                        if (exp > img->header.data_offset && exp < img->header.data_offset + img->header.data_size)
                            exp += data_delta;
                        else
                            exp += code_delta;
                    }

                } else {
                    mngr.open_romimg(rimg);

                    code_start = rimg->header.code_address;
                    code_end = code_start + rimg->header.code_size;
                    data_start = rimg->header.data_address;
                    data_end = data_start + rimg->header.data_size + rimg->header.bss_size;

                    code_delta = 0;
                    data_delta = 0;

                    expdir = rimg->exports.data();
                }
            }

            for (uint32_t i = 0; i < import_block.ordinals.size(); i++) {
                uint32_t off = imdir[i];
                uint32_t *code_ptr = ptr<uint32_t>(me.rt_code_addr + off).get(mem);

                uint32_t import_inf = *code_ptr;
                uint32_t ord = import_inf & 0xffff;
                uint32_t adj = import_inf >> 16;

                uint32_t export_addr;
                uint32_t section_delta;
                uint32_t val = 0;

                if (ord > 0) {
                    export_addr = expdir[ord - 1];

                    auto sid = mngr.get_sid(export_addr);

                    if (sid) {
                        LOG_INFO("Importing export addr: 0x{:x}, sid: 0x{:x}, function: {}, writing at: 0x{:x}, ord: {}",
                            val, sid.value(), mngr.get_func_name(sid.value()).value(), me.rt_code_addr + off, ord);

                        uint32_t addr = mngr.get_stub(*sid).ptr_address();
                        write(code_ptr, addr);

                        continue;
                    }

                    if (export_addr >= code_start && export_addr <= code_end) {
                        section_delta = code_delta;
                    } else {
                        section_delta = data_delta;
                    }

                    val = export_addr + section_delta + adj;
                }

                write(code_ptr, val);
            }

            return true;
        }

        bool import_exe_image(eka2img *img, memory_system *mem, kernel_system *kern, hle::lib_manager &mngr) {
            // Create the code + static data chunk
            img->code_chunk = kern->create_chunk("", 0, common::align(img->header.code_size, mem->get_page_size()), common::align(img->header.code_size, mem->get_page_size()), prot::read_write,
                kernel::chunk_type::normal, kernel::chunk_access::code, kernel::chunk_attrib::none, kernel::owner_type::process);

            img->data_chunk = kern->create_chunk("", 0, common::align(img->header.data_size, mem->get_page_size()), common::align(img->header.data_size, mem->get_page_size()), prot::read_write,
                kernel::chunk_type::normal, kernel::chunk_access::code, kernel::chunk_attrib::none, kernel::owner_type::process);

            uint32_t rtcode_addr = img->code_chunk->base().ptr_address();
            uint32_t rtdata_addr = img->data_chunk ? img->data_chunk->base().ptr_address() : 0;

            LOG_INFO("Runtime code: 0x{:x}", rtcode_addr);

            img->rt_code_addr = rtcode_addr;
            img->rt_data_addr = rtdata_addr;

            // Ram code address is the run time address of the code
            uint32_t code_delta = rtcode_addr - img->header.code_base;
            uint32_t data_delta = rtdata_addr - img->header.data_base;

            relocate(img->code_reloc_section.entries, reinterpret_cast<uint8_t *>(img->data.data() + img->header.code_offset), code_delta, data_delta);

            if (img->header.data_size)
                relocate(img->data_reloc_section.entries, reinterpret_cast<uint8_t *>(img->data.data() + img->header.data_offset), code_delta, data_delta);

            if (img->epoc_ver == epocver::epoc6) {
                uint32_t track = 0;

                for (auto &ib : img->import_section.imports) {
                    pe_fix_up_iat(mem, mngr, *img, ib, img->iat, track);
                }
            }

            memcpy(ptr<void>(rtcode_addr).get(mem), img->data.data() + img->header.code_offset, img->header.code_size);

            if (img->header.data_size)
                memcpy(ptr<uint32_t>(rtdata_addr).get(mem), img->data.data() + img->header.data_offset, img->header.data_size);

            if (img->epoc_ver == epocver::epoc9) {
                for (auto &ib : img->import_section.imports) {
                    elf_fix_up_import_dir(mem, mngr, *img, ib);
                }
            }

            LOG_INFO("Load executable success");

            return true;
        }

        void peek_t(void *buf, size_t element_count, size_t element_size, std::istream *file) {
            size_t crr = file->tellg();

            file->read(static_cast<char *>(buf), element_count * element_size);
            file->seekg(crr);
        }

        void dump_flag_info(int flag) {
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

        void read_relocations(common::ro_buf_stream *stream,
            eka2_reloc_section &section,
            uint32_t offset) {
            // No relocations
            if (offset == 0) {
                return;
            }

            stream->seek(offset, common::beg);

            stream->read(reinterpret_cast<void *>(&section.size), 4);
            stream->read(reinterpret_cast<void *>(&section.num_relocs), 4);

            for (int i = 0; i < section.num_relocs; i++) {
                eka2_reloc_entry reloc_entry;

                stream->read(reinterpret_cast<void *>(&reloc_entry.base), 4);
                stream->read(reinterpret_cast<void *>(&reloc_entry.size), 4);

                assert((reloc_entry.size) % 2 == 0);

                reloc_entry.rels_info.resize(((reloc_entry.size - 8) / 2));

                for (auto &rel_info : reloc_entry.rels_info) {
                    stream->read(reinterpret_cast<void *>(&rel_info), 2);
                }

                i += reloc_entry.rels_info.size();

                section.entries.push_back(reloc_entry);
            }
        }

        bool dump_buf_data(std::string path, std::vector<char> vec) {
            FILE *file = fopen(path.c_str(), "wb");

            if (!file) {
                return false;
            }

            auto write_bytes = fwrite(vec.data(), 1, vec.size(), file);

            if (write_bytes != vec.size()) {
                fclose(file);
                return false;
            }

            fclose(file);
            return true;
        }

        void parse_export_dir(eka2img &img) {
            if (img.header.export_dir_offset == 0) {
                return;
            }

            uint32_t *exp = reinterpret_cast<uint32_t *>(img.data.data() + img.header.export_dir_offset);

            for (auto i = 0; i < img.header.export_dir_count; i++) {
                img.ed.syms.push_back(*exp++);
            }
        }

        void parse_iat(eka2img &img) {
            uint32_t *imp_addr = reinterpret_cast<uint32_t *>(img.data.data() + img.header.code_offset + img.header.text_size);

            while (*imp_addr != 0) {
                img.iat.its.push_back(*imp_addr++);
            }
        }

        std::optional<eka2img> parse_eka2img(symfile ef, bool read_reloc) {
            if (!ef) {
                return std::optional<eka2img>{};
            }

            eka2img img;

            auto file_size = ef->size();

            ef->read_file(&img.header.uid1, 1, 4);
            ef->read_file(&img.header.uid2, 1, 4);
            ef->read_file(&img.header.uid3, 1, 4);
            ef->read_file(&img.header.check, 1, 4);
            ef->read_file(&img.header.sig, 1, 4);

            if (img.header.sig != 0x434F5045) {
                LOG_ERROR("Undefined EKA Image type");
                return std::optional<eka2img>{};
            }

            uint32_t temp = 0;
            ef->read_file(&temp, 1, 4);

            if ((temp == 0x2000) || (temp == 0x1000)) {
                // Quick hack to determinate if this is an EKA1
                img.header.cpu = static_cast<loader::eka2_cpu>(temp);
                img.epoc_ver = epocver::epoc6;

                ef->read_file(&temp, 1, 4);
                ef->read_file(&temp, 1, 4);
                ef->read_file(&img.header.petran_major, 1, 1);
                ef->read_file(&img.header.petran_minor, 1, 1);
                ef->read_file(&img.header.petran_build, 1, 2);
                ef->read_file(&img.header.flags, 1, 4);
                ef->read_file(&img.header.code_size, 1, 4);
                ef->read_file(&img.header.data_size, 1, 4);
                ef->read_file(&img.header.heap_size_min, 1, 4);
                ef->read_file(&img.header.heap_size_max, 1, 4);
                ef->read_file(&img.header.stack_size, 1, 4);
                ef->read_file(&img.header.bss_size, 1, 4);
                ef->read_file(&img.header.entry_point, 1, 4);
                ef->read_file(&img.header.code_base, 1, 4);
                ef->read_file(&img.header.data_base, 1, 4);
                ef->read_file(&img.header.dll_ref_table_count, 1, 4);
                ef->read_file(&img.header.export_dir_offset, 1, 4);
                ef->read_file(&img.header.export_dir_count, 1, 4);
                ef->read_file(&img.header.text_size, 1, 4);
                ef->read_file(&img.header.code_offset, 1, 4);
                ef->read_file(&img.header.data_offset, 1, 4);
                ef->read_file(&img.header.import_offset, 1, 4);
                ef->read_file(&img.header.code_reloc_offset, 1, 4);
                ef->read_file(&img.header.data_reloc_offset, 1, 4);
                ef->read_file(&img.header.priority, 1, 2);

                img.header.compression_type = 1;
            } else {
                img.epoc_ver = epocver::epoc9;

                ef->seek(0, file_seek_mode::beg);
                ef->read_file(&img.header, 1, sizeof(eka2img_header));
            }

            compress_type ctype = (compress_type)(img.header.compression_type);
            dump_flag_info((int)img.header.flags);

            if (img.header.compression_type > 0) {
                int header_format = ((int)img.header.flags >> 24) & 0xF;
                ef->read_file(&img.uncompressed_size, 1, 4);

                if (header_format == 2) {
                    img.has_extended_header = true;
                    LOG_INFO("V-Format used, load more (too tired) \\_(-.-)_/");

                    ef->read_file(&img.header_extended.info, 1, sizeof(eka2img_vsec_info));
                    ef->read_file(&img.header_extended.exception_des, 1, 4);
                    ef->read_file(&img.header_extended.spare2, 1, 4);
                    ef->read_file(&img.header_extended.export_desc_size, 1, 2);
                    ef->read_file(&img.header_extended.export_desc_type, 1, 1);
                    ef->read_file(&img.header_extended.export_desc, 1, 1);
                }

                img.data.resize(img.uncompressed_size + img.header.code_offset);

                ef->seek(0, file_seek_mode::beg);
                ef->read_file(img.data.data(), 1, img.header.code_offset);

                std::vector<char> temp_buf(file_size - img.header.code_offset);

                ef->seek(img.header.code_offset, file_seek_mode::beg);
                auto bytes_read = ef->read_file(temp_buf.data(), 1, temp_buf.size());

                if (bytes_read != temp_buf.size()) {
                    LOG_ERROR("File reading unproperly");
                }

                if (ctype == compress_type::deflate_c) {
                    flate::bit_input input(reinterpret_cast<uint8_t *>(temp_buf.data()), temp_buf.size() * 8);
                    flate::inflater inflate_machine(input);

                    inflate_machine.init();
                    auto readed = inflate_machine.read(reinterpret_cast<uint8_t *>(&img.data[img.header.code_offset]),
                        img.uncompressed_size);

                    LOG_INFO("Readed compress, size: {}", readed);

                    FILE *tempfile = fopen("nokiaDefaltedTemp.seg", "wb");
                    fwrite(img.data.data(), 1, img.data.size(), tempfile);
                    fclose(tempfile);

                } else if (ctype == compress_type::byte_pair_c) {
                    auto crr_pos = ef->tell();

                    std::vector<char> temp(ef->size() - img.header.code_offset);
                    ef->seek(img.header.code_offset, file_seek_mode::beg);

                    ef->read_file(temp.data(), 1, temp.size());

                    FILE *tempfile = fopen("bytepairTemp.seg", "wb");
                    fwrite(temp.data(), 1, temp.size(), tempfile);
                    fclose(tempfile);

                    common::ibytepair_stream bpstream("bytepairTemp.seg", 0);

                    auto codesize = bpstream.read_pages(&img.data[img.header.code_offset], img.header.code_size);
                    auto restsize = bpstream.read_pages(&img.data[img.header.code_offset + img.header.code_size], img.uncompressed_size);
                }

            } else {
                img.uncompressed_size = file_size;

                img.data.resize(file_size);
                ef->seek(0, file_seek_mode::beg);
                ef->read_file(img.data.data(), 1, img.data.size());
            }

            switch (img.header.cpu) {
            case eka2_cpu::armv5:
                LOG_INFO("Detected: ARMv5");
                break;
            case eka2_cpu::armv6:
                LOG_INFO("Detected: ARMv6");
                break;
            case eka2_cpu::armv4:
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

            // Read the import section
            common::ro_buf_stream stream(reinterpret_cast<uint8_t *>(img.data.data()), img.data.size());

            stream.seek(img.header.import_offset, common::beg);
            stream.read(reinterpret_cast<void *>(&img.import_section.size), 4);

            img.import_section.imports.resize(img.header.dll_ref_table_count);

            LOG_INFO("Total dll count: {}", img.header.dll_ref_table_count);
            LOG_INFO("Import offsets: {}", img.header.import_offset);

            for (auto &import : img.import_section.imports) {
                stream.read(reinterpret_cast<void *>(&import.dll_name_offset), 4);
                stream.read(reinterpret_cast<void *>(&import.number_of_imports), 4);

                if (import.number_of_imports == 0) {
                    continue;
                }

                auto crr_size = stream.tell();
                stream.seek(img.header.import_offset + import.dll_name_offset, common::beg);

                char temp = 1;

                while (temp != 0) {
                    stream.read(&temp, 1);
                    import.dll_name += temp;
                }

                LOG_TRACE("Find dll import: {}, total import: {}.", import.dll_name.c_str(), import.number_of_imports);

                stream.seek(crr_size, common::beg);

                import.ordinals.resize(import.number_of_imports);

                for (auto &oridinal : import.ordinals) {
                    stream.read(reinterpret_cast<void *>(&oridinal), 4);
                }
            }

            if (read_reloc) {
                read_relocations(&stream,
                    img.code_reloc_section, img.header.code_reloc_offset);

                read_relocations(&stream,
                    img.data_reloc_section, img.header.data_reloc_offset);
            }

            return img;
        }

        bool load_eka2img(eka2img &img, memory_system *mem, kernel_system *kern, hle::lib_manager &mngr) {
            if (img.header.uid1 == loader::eka2_img_type::dll) {
            }
            return import_exe_image(&img, mem, kern, mngr);
        }
    }
}

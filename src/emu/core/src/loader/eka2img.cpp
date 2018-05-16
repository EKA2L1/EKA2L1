#include <loader/eka2img.h>

#include <ptr.h>

#include <common/buffer.h>
#include <common/bytepair.h>
#include <common/data_displayer.h>
#include <common/flate.h>
#include <common/log.h>

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
            LOG_TRACE("Relocation original data: 0x{:x}, new data: 0x{:x}", *data, sym);
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
                        LOG_TRACE("Relocate fail at page: {}", i);
                        return false;
                    }
                }
            }

            return true;
        }

    /*
        bool import_func(ptr<uint32_t> stub_ptr, uint32_t sym) {
            uint32_t *stub = stub_ptr.get();

            if (stub == nullptr) {
                return false;
            }

            LOG_TRACE("Stubbing import: 0x{:x}", sym);

            stub[0] = 0xef000000; // swi #0
            stub[1] = 0xe1a0f00e; // mov pc, lr
            stub[2] = sym;

            return true;
        }
    */
        uint32_t import_libs(eka2img *img, memory* mem, uint32_t rtcode_addr, hle::lib_manager& mngr) {
            uint32_t stub_size = 0;

            for (auto &import_entry : img->import_section.imports) {
				auto ids = mngr.get_sids(import_entry.dll_name);

				if (!ids) {
					LOG_CRITICAL("No SIDS provided for: {}", import_entry.dll_name);
					continue;
				}

				std::vector<uint32_t> stub_resides;

				for (uint32_t i = 0; i < import_entry.number_of_imports; i++) {
                    //import_func(ptr<uint32_t>(stub_ptr), ids.value()[i]);
                }
            }

            return stub_size;
        }

        bool import_exe_image(eka2img *img, memory* mem, hle::lib_manager& mngr) {
            // Map the memory to store the text, data and import section
            ptr<void> asmdata = mem->alloc_ime(img->header.code_size + 0x1000);

            LOG_INFO("Code dest: 0x{:x}", (long)(img->header.code_size + img->header.code_offset + img->data.data()));
            LOG_INFO("Code size: 0x{:x}", img->header.code_size);

            uint32_t rtcode_addr = asmdata.ptr_address();
            uint32_t rtdata_addr = 0;

            //rtdata_addr = import_libs(img, mem, rtcode_addr, mngr);
            rtdata_addr += rtcode_addr + img->header.code_size;

            img->rt_code_addr = rtcode_addr;
            img->rt_data_addr = rtdata_addr;

            LOG_INFO("Writing data at offset: 0x{:x}", rtdata_addr);

            // Ram code address is the run time address of the code
            uint32_t code_delta = rtcode_addr - img->header.code_base;
            uint32_t data_delta = rtdata_addr - img->header.data_base;

            relocate(img->code_reloc_section.entries, reinterpret_cast<uint8_t *>(img->data.data() + img->header.code_offset), code_delta, data_delta);

            relocate(img->data_reloc_section.entries, reinterpret_cast<uint8_t *>(img->data.data() + img->header.data_offset), code_delta, data_delta);

            memcpy(ptr<uint32_t>(rtcode_addr).get(mem), img->data.data() + img->header.code_offset, img->header.code_size);
            memcpy(ptr<uint32_t>(rtdata_addr).get(mem), img->data.data() + img->header.data_offset, img->header.data_size);

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

            // There is no document on this anywhere. The code you see online is not right.
            // Here is the part i tell you the truth: The size is including both the base and itself
            // An entry contains the target offset and relocate size. After that, there is list of relocation
            // code (uint16_t).
            // Since I saw repeated pattern and also saw some code from elf2e32 reloaded, i just subtract the
            // seek with 8, and actually works. Now i feel like i has wasted 4 hours of my life figuring out this :P
            for (int i = 0; i < section.num_relocs; i++) {
                eka2_reloc_entry reloc_entry;

                stream->read(reinterpret_cast<void *>(&reloc_entry.base), 4);
                stream->read(reinterpret_cast<void *>(&reloc_entry.size), 4);

                assert((reloc_entry.size - 8) % 2 == 0);

                reloc_entry.rels_info.resize(((reloc_entry.size - 8) / 2));

                for (auto &rel_info : reloc_entry.rels_info) {
                    stream->read(reinterpret_cast<void *>(&rel_info), 2);
                }

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

        std::optional<eka2img> parse_eka2img(const std::string &path, bool read_reloc) {
            LOG_TRACE("Loading image: {}", path);

            eka2img img;

            FILE *f = fopen(path.c_str(), "rb");

            fseek(f, 0, SEEK_END);
            auto file_size = ftell(f);
            fseek(f, 0, SEEK_SET);

            fread(&img.header.uid1, 1, 4, f);
            fread(&img.header.uid2, 1, 4, f);
            fread(&img.header.uid3, 1, 4, f);
            fread(&img.header.check, 1, 4, f);
            fread(&img.header.sig, 1, 4, f);

            if (img.header.sig != 0x434F5045) {
                LOG_ERROR("Undefined EKA Image type");
                fclose(f);
                return std::optional<eka2img>{};
            }

            uint32_t temp = 0;
            fread(&temp, 1, 4, f);

            if ((temp == 0x2000) || (temp == 0x1000)) {
                // Quick hack to determinate if this is an EKA1
                img.header.cpu = static_cast<loader::eka2_cpu>(temp);

                fread(&temp, 1, 4, f);
                fread(&temp, 1, 4, f);
                fread(&img.header.petran_major, 1, 1, f);
                fread(&img.header.petran_minor, 1, 1, f);
                fread(&img.header.petran_build, 1, 2, f);
                fread(&img.header.flags, 1, 4, f);
                fread(&img.header.code_size, 1, 4, f);
                fread(&img.header.data_size, 1, 4, f);
                fread(&img.header.heap_size_min, 1, 4, f);
                fread(&img.header.heap_size_max, 1, 4, f);
                fread(&img.header.stack_size, 1, 4, f);
                fread(&img.header.bss_size, 1, 4, f);
                fread(&img.header.entry_point, 1, 4, f);
                fread(&img.header.code_base, 1, 4, f);
                fread(&img.header.data_base, 1, 4, f);
                fread(&img.header.dll_ref_table_count, 1, 4, f);
                fread(&img.header.export_dir_offset, 1, 4, f);
                fread(&img.header.export_dir_count, 1, 4, f);
                fread(&img.header.text_size, 1, 4, f);
                fread(&img.header.code_offset, 1, 4, f);
                fread(&img.header.data_offset, 1, 4, f);
                fread(&img.header.import_offset, 1, 4, f);
                fread(&img.header.code_reloc_offset, 1, 4, f);
                fread(&img.header.data_reloc_offset, 1, 4, f);
                fread(&img.header.priority, 1, 2, f);

                img.header.compression_type = 1;
            } else {
                fseek(f, 0, SEEK_SET);
                fread(&img.header, 1, sizeof(eka2img_header), f);
            }

            compress_type ctype = (compress_type)(img.header.compression_type);
            dump_flag_info((int)img.header.flags);

            if (img.header.compression_type > 0) {
                int header_format = ((int)img.header.flags >> 24) & 0xF;

                fread(&img.uncompressed_size, 1, 4, f);

                std::vector<char> temp_buf(file_size);
                img.data.resize(img.uncompressed_size + img.header.code_offset);

                if (header_format == 2) {
                    img.has_extended_header = true;
                    LOG_INFO("V-Format used, load more (too tired) \\_(-.-)_/");

                    fread(&img.header_extended.info, 1, sizeof(eka2img_vsec_info), f);
                    fread(&img.header_extended.exception_des, 1, 4, f);
                    fread(&img.header_extended.spare2, 1, 4, f);
                    fread(&img.header_extended.export_desc_size, 1, 2, f);
                    fread(&img.header_extended.export_desc_type, 1, 1, f);
                    fread(&img.header_extended.export_desc, 1, 1, f);
                }

                fseek(f, 0, SEEK_SET);
                fread(img.data.data(), 1, sizeof(eka2img_header) + 4 + (img.has_extended_header ? sizeof(eka2img_header_extended) : 0), f);

                fseek(f, img.header.code_offset, SEEK_SET);
                fread(temp_buf.data(), 1, temp_buf.size(), f);

                if (ctype == compress_type::deflate_c) {
                    // INFLATE IT!
                    // Weird behavior, this is my way
                    img.data[img.header.code_offset] = 12;

                    flate::bit_input input(reinterpret_cast<uint8_t *>(temp_buf.data()), temp_buf.size() * 8);
                    flate::inflater inflate_machine(input);

                    inflate_machine.init();
                    auto readed = inflate_machine.read(reinterpret_cast<uint8_t *>(&img.data[img.header.code_offset]),
                        img.uncompressed_size);

                    LOG_INFO("Readed compress, size: {}", readed);
                } else if (ctype == compress_type::byte_pair_c) {
                    auto temp_stream = std::make_shared<std::ifstream>(path);
                    temp_stream->seekg(img.header.code_offset, std::ios::beg);

                    common::ibytepair_stream bpstream(path, img.header.code_offset);

                    auto codesize = bpstream.read_pages(&img.data[img.header.code_offset], img.header.code_size);
                    auto restsize = bpstream.read_pages(&img.data[img.header.code_offset + img.header.code_size], img.uncompressed_size);
                }

            } else {
                img.uncompressed_size = file_size;

                img.data.resize(file_size);
                fseek(f, SEEK_SET, 0);
                fread(img.data.data(), 1, img.data.size(), f);
            }

            dump_buf_data(path.substr(0, path.find_last_of(".")) + ".dedat", img.data);

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

            fclose(f);

            return img;
        }

        bool load_eka2img(eka2img &img, memory* mem, hle::lib_manager& mngr) {
            return import_exe_image(&img, mem, mngr);
        }
    }
}

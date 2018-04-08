#include <loader/eka2img.h>
#include <ptr.h>
#include <common/log.h>
#include <common/bytepair.h>
#include <common/data_displayer.h>
#include <miniz.h>
#include <cstdio>
#include <sstream>

namespace eka2l1 {
    namespace loader {
        struct eka2img_import_sec_header {
            uint32_t size;
            uint32_t num_reloc;
        };

        enum class relocation_type: uint16_t {
            reserved = 0x0000,
            text = 0x1000,
            data = 0x2000,
            inffered = 0x3000
        };

        enum compress_type {
            byte_pair_c = 0x101F7AFC,
            deflate_c = 0x102822AA
        };

        // Write simple relocation
        // Symbian only used this, as found on IDA
        bool write(uint32_t* data, uint32_t sym) {
            *data = sym;
            return true;
        }

        bool relocate(uint32_t* dest_ptr, relocation_type type, uint32_t code_delta, uint32_t data_delta) {
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
            case relocation_type::inffered: default:
                LOG_WARN("Relocation not properly handle: {}", (int)type);
                break;
            }

            return true;
        }

        // Given relocation entries, relocate the code and data
        bool relocate(std::vector<eka2_reloc_entry> entries,
                      uint32_t* dest_addr,
                      uint32_t code_delta,
                      uint32_t data_delta) {
            for (uint32_t i = 0; i < entries.size(); i++) {
                auto entry = entries[i];

                for (auto& rel_info: entry.rels_info) {
                    // Get the lower 12 bit for virtual_address
                    uint32_t virtual_addr = entry.base + (rel_info & 0x0FFF);
                    uint32_t* dest_ptr = virtual_addr + dest_addr;

                    relocation_type rel_type = (relocation_type)(rel_info & 0xF000);

                    if (!relocate(dest_ptr, rel_type, code_delta, data_delta)) {
                        LOG_TRACE("Relocate fail at page: {}", i);
                        return false;
                    }
                }
            }

            return true;
        }

        bool import_func(ptr<uint32_t> stub_ptr, uint32_t sym) {
            uint32_t* stub = stub_ptr.get();

            if (stub == nullptr) {
                return false;
            }

            stub[0] = 0xef000000; // swi #0
            stub[1] = 0xe1a0f00e; // mov pc, lr
            stub[2] = sym;
            
            return true;
        }

        // TODO1: Move this along with io and manager to core
        // TODO2: Write a system interruption and write a mov pc instruction with the symbol
        // the interrupt hook will read the symbol and call it.
        // Eg.
        // swi #0 (Empty)
        // mov pc, lr
        // [your symbol here]
        // When there is an interuppt, it calls the interuppt hook. Here we can read the
        // symbol and call the right function.
        // First, add the stub at the end of text section
        // Next, in import address table, specified the address of each stub
        // In each stub, write an intteruption and a mov instruction
        bool import_libs() {
            return true;
        }

        void peek_t(void* buf, size_t element_count, size_t element_size, std::istream* file) {
             size_t crr = file->tellg();

             file->read(static_cast<char*>(buf), element_count * element_size);
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

        void read_relocations(std::istringstream* stream,
                              eka2_reloc_section& section,
                              uint32_t offset) {

            stream->seekg(offset, std::ios::beg);

            stream->read(reinterpret_cast<char*>(&section.size), 4);
            stream->read(reinterpret_cast<char*>(&section.num_relocs), 4);

            // There is no document on this anywhere. The code you see online is not right.
            // Here is the part i tell you the truth: The size is including both the base and itself
            // An entry contains the target offset and relocate size. After that, there is list of relocation
            // code (uint16_t).
            // Since I saw repeated pattern and also saw some code from elf2e32 reloaded, i just subtract the
            // seek with 8, and actually works. Now i feel like i has wasted 4 hours of my life figuring out this :P
            while ((uint32_t)stream->tellg() - offset < section.size) {
                eka2_reloc_entry reloc_entry;

                stream->read(reinterpret_cast<char*>(&reloc_entry.base), 4);
                stream->read(reinterpret_cast<char*>(&reloc_entry.size), 4);

                assert((reloc_entry.size - 8) % 2 == 0);

                reloc_entry.rels_info.resize(((reloc_entry.size - 8) / 2)-1);

                for (auto& rel_info: reloc_entry.rels_info) {
                    stream->read(reinterpret_cast<char*>(&rel_info), 2);
                }

                uint16_t temp_padding = 0;
                stream->read(reinterpret_cast<char*>(&temp_padding), 2);

                // If it's zero, maybe it's padding
                if (temp_padding != 0) {
                    reloc_entry.rels_info.push_back(temp_padding);
                }

                section.entries.push_back(reloc_entry);
            }
        }

        eka2img load_eka2img(const std::string& path) {
            eka2img img;

            FILE* f = fopen(path.c_str(), "rb");

            fseek(f, 0, SEEK_END);
            auto file_size = ftell(f);
            fseek(f, 0, SEEK_SET);

            fread(&img.header, 1, sizeof(eka2img_header), f);

            assert(img.header.sig == 0x434F5045);

            compress_type ctype = (compress_type)(img.header.compression_type);
            dump_flag_info((int)img.header.flags);

            if (img.header.compression_type > 0) {
                LOG_WARN("Image that compressed is not properly supported rn. Try"
                         "other image until you find one that does not emit this warning");

                int header_format = ((int)img.header.flags >> 24) & 0xF;

                fread(&img.uncompressed_size, 1, 4, f);

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

                std::vector<char> temp_buf(file_size - sizeof(eka2img_header) - 4);
                img.data.resize(img.uncompressed_size);

                fread(temp_buf.data(), 1, temp_buf.size(), f);

                if (ctype == compress_type::deflate_c) {
                    // INFLATE IT!
                    mz_stream stream;
                    stream.avail_in = 0;
                    stream.next_in = 0;
                    stream.zalloc = nullptr;
                    stream.zfree = nullptr;

                    if (inflateInit(&stream) != MZ_OK) {
                        LOG_ERROR("Can not intialize inflate stream");
                    }

                    stream.avail_in = temp_buf.size();
                    stream.next_in = reinterpret_cast<const unsigned char*>(temp_buf.data());
                    stream.next_out = reinterpret_cast<unsigned char*>(&img.data[sizeof(eka2img_header) + 4]);

                    auto res = inflate(&stream,Z_NO_FLUSH);

                    if (res != MZ_OK) {
                        LOG_ERROR("Inflate chunk failed!");
                    };

                    inflateEnd(&stream);
                } else {
                    common::nokia_bytepair_decompress(img.data.data(), img.data.size(),
                                                      temp_buf.data(), temp_buf.size());
                }

            } else {
                img.data.resize(file_size);
                fseek(f, SEEK_SET, 0);
                fread(img.data.data(), 1, img.data.size(), f);
            }

            eka2l1::dump_data("Image data", std::vector<uint8_t>(img.data.begin(), img.data.end()));

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


            //LOG_TRACE("Code size: 0x{:x}, Text size: 0x{:x}.", img.header.code_size, img.header.text_size);

            uint32_t import_export_table_size = img.header.code_size - img.header.text_size;
            LOG_TRACE("Import + export size: 0x{:x}", import_export_table_size);

            // Read the import section

            std::istringstream strstream;
            strstream.rdbuf()->pubsetbuf(img.data.data(), img.data.size());

            strstream.seekg(img.header.import_offset, std::ios_base::beg);
            strstream.read(reinterpret_cast<char*>(&img.import_section.size), 4);

            img.import_section.imports.resize(img.header.dll_ref_table_count);

            for (auto& import: img.import_section.imports) {
                strstream.read(reinterpret_cast<char*>(&import.dll_name_offset), 4);
                strstream.read(reinterpret_cast<char*>(&import.number_of_imports), 4);

                auto crr_size = strstream.tellg();
                strstream.seekg(img.header.import_offset + import.dll_name_offset, std::ios_base::beg);

                char temp = 1;
                while (temp != 0) {
                    strstream.read(&temp, 1);
                    import.dll_name += temp;
                }

                LOG_TRACE("Find dll import: {}, total import: {}.", import.dll_name.c_str(), import.number_of_imports);

                strstream.seekg(crr_size, std::ios_base::beg);

                import.ordinals.resize(import.number_of_imports);

                for (auto& oridinal: import.ordinals) {
                    strstream.read(reinterpret_cast<char*>(&oridinal), 4);
                }
            }

            read_relocations(&strstream,
                             img.code_reloc_section, img.header.code_reloc_offset);

            read_relocations(&strstream,
                             img.data_reloc_section, img.header.data_reloc_offset);
            // Ram code address is the run time address of the code

            uint32_t runtime_code_addr = RAM_CODE_ADDR;
            uint32_t runtime_data_addr = runtime_code_addr + img.header.code_size;

            uint32_t code_delta = runtime_code_addr - img.header.code_base;
            uint32_t data_delta = runtime_data_addr - img.header.data_base;

            relocate(img.code_reloc_section.entries, reinterpret_cast<uint32_t*>
                     (img.data.data() + img.header.code_offset), code_delta, data_delta);

            // Map the memory to store the text, data and import section
            // ptr<void> asmdata =
            //        core_mem::map(runtime_code_addr, img.uncompressed_size, core_mem::prot::read_write_exec);

            fclose(f);

            return img;
        }
    }
}

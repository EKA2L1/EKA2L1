#include <loader/eka2img.h>
#include <common/log.h>
#include <miniz.h>

#include <cstdio>
#include <sstream>

namespace eka2l1 {
    namespace loader {
        struct eka2img_import_sec_header {
            uint32_t size;
            uint32_t num_reloc;
        };

        enum relocation_code {
            abs32 = 0x02,
            rel32 = 0x03,
            ldr_pc_g0 = 0x04,
            abs16 = 0x05,
            abs12 = 0x06,
            thm_abs5 = 0x07,
            abs8 = 0x08
        };

        // This is copying from Vita3K
        // It generates instruction binary.

        void write_mov_abs(void* data, uint16_t sym) {
            struct inst {
                uint32_t imm12: 12;
                uint32_t ignored1: 4;
                uint32_t imm4: 4;
                uint32_t ignored2: 12;
            };

            inst* instruction = reinterpret_cast<inst*>(data);

            instruction->imm12 = sym;
            instruction->imm4 = sym >> 12;
        }

        void write(void* data, uint16_t sym) {
            memcpy(data, &sym, 2);
        }

        // Given a relocation code with the pointer to the target, s, p, a param
        // Relocate the code/data to the position its supposed to be
        // Todo: write some relocation here
        bool relocate(void* data, relocation_code code, uint32_t s, uint32_t p, uint32_t a) {
            switch (code) {
            case abs32:
                break;
            default:
                LOG_INFO("Unimplemented relocation code: {}", (int)code);
                break;
            }

            return true;
        }

        // Given relocation entries, relocate the code and data
        bool relocate(std::vector<eka2_reloc_entry> entries, uint32_t base_all_off, size_t size) {
            return true;
        }

        // TODO: Write a system interruption and write a mov pc instruction with the symbol
        // the interrupt hook will read the symbol and call it.
        // Eg.
        // svc #0 (Empty)
        // mov pc, lr
        // [your symbol here]
        // When there is an interuppt, it calls the interuppt hook. Here we can read the
        // symbol and call the right function.
        // This still needs more reversing
        bool import_libs() {
            return true;
        }

        void peek_t(void* buf, size_t element_count, size_t element_size, std::istream* file) {
             size_t crr = file->tellg();

             file->read(static_cast<char*>(buf), element_count * element_size);
             file->seekg(crr);
        }

        eka2img load_eka2img(const std::string& path) {
            eka2img img;

            FILE* f = fopen(path.c_str(), "rb");

            fseek(f, 0, SEEK_END);
            auto file_size = ftell(f);
            fseek(f, 0, SEEK_SET);

            fread(&img.header, 1, sizeof(eka2img_header), f);

            assert(img.header.sig == 0x434F5045);

            if (img.header.compression_type != 0) {
                fread(&img.uncompressed_size, 1, 4, f);
                std::vector<char> temp_buf(file_size - sizeof(eka2img_header) - 4);

                img.data.resize(img.uncompressed_size);

                memcpy(img.data.data(), &img.header, sizeof(eka2img_header));
                memcpy(img.data.data() + sizeof(eka2img_header), &img.uncompressed_size, 4);

                fread(temp_buf.data(), 1, temp_buf.size(), f);

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
                img.data.resize(file_size);
                fseek(f, SEEK_SET, 0);
                fread(img.data.data(), 1, img.data.size(), f);
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

            strstream.seekg(img.header.code_reloc_offset, std::ios::beg);

            LOG_TRACE("Code reloc offset: 0x{:x}, Data reloc offset: 0x{:x}", img.header.code_reloc_offset,
                      img.header.data_offset);

            strstream.read(reinterpret_cast<char*>(&img.code_reloc_section.size), 4);
            strstream.read(reinterpret_cast<char*>(&img.code_reloc_section.num_relocs), 4);

            LOG_TRACE("Code relocation size: {}, code total relocations: {}", img.code_reloc_section.size,
                      img.code_reloc_section.num_relocs);

            // There is no document on this anywhere. The code you see online is not right.
            // Here is the part i tell you the truth: The size is including both the base and itself
            // An entry contains the target offset and relocate size. After that, there is list of relocation
            // code (uint16_t).
            // Since I saw repeated pattern and also saw some code from elf2e32 reloaded, i just subtract the
            // seek with 8, and actually works. Now i feel like i has wasted 4 hours of my life figuring out this :P
            while ((uint32_t)strstream.tellg() - img.header.code_reloc_offset < img.code_reloc_section.size) {
                eka2_reloc_entry reloc_entry;

                strstream.read(reinterpret_cast<char*>(&reloc_entry.base), 4);
                strstream.read(reinterpret_cast<char*>(&reloc_entry.size), 4);

                img.code_reloc_section.entries.push_back(reloc_entry);

                strstream.seekg(reloc_entry.size - 8, std::ios::cur);
            }

            fclose(f);

            return img;
        }
    }
}

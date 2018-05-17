#include <loader/rom.h>

namespace eka2l1 {
    namespace loader {
        uint32_t rom_to_offset(address romstart, address off) {
            return off - romstart;
        }
        
        rom_header read_rom_header(FILE* file) {
            rom_header header;
            
            fread(header.jump, 1, sizeof(header.jump), file);
            fread(&header.restart_vector, 1, 4, file);
            fread(&header.time, 1, 8, file);
            fread(&header.time_high, 1, 4, file);
            fread(&header.rom_base, 1, 4, file);
            fread(&header.rom_size, 1, 4, file);
            fread(&header.rom_root_dir_list, 1, 4, file);
            fread(&header.kern_data_address, 1, 4, file);
            fread(&header.kern_limit, 1, 4, file);
            fread(&header.primary_file, 1, 4, file);
            fread(&header.secondary_file, 1, 4, file);
            fread(&header.checksum, 1, 4, file);
            fread(&header.hardware, 1, 4, file);
            fread(&header.lang, 1, 8, file);
            fread(&header.kern_config_flags, 1, 4, file);
            fread(&header.rom_exception_search_tab, 1, 4, file);
            fread(&header.rom_header_size, 1, 4, file);

            fread(&header.rom_section_header, 1, 4, file);
            fread(&header.total_sv_data_size, 1, 4, file);
            fread(&header.variant_file, 1, 4, file);
            fread(&header.extension_file, 1, 4, file);
            fread(&header.reloc_info, 1, 4, file);

            fread(&header.old_trace_mask, 1, 4, file);
            fread(&header.user_data_addr, 1, 4, file);
            fread(&header.total_user_data_size, 1, 4, file);

            fread(&header.debug_port, 1, 4, file);
            fread(&header.major, 1, 1, file);
            fread(&header.minor, 1, 1, file);
            fread(&header.build, 1, 2, file);

            fread(&header.compress_type, 1, 4, file);
            fread(&header.compress_size, 1, 4, file);
            fread(&header.uncompress_size, 1, 4, file);
            fread(&header.disabled_caps, 4, 2, file);
            fread(&header.trace_mask, 4, 8, file);
            fread(&header.initial_btrace_filter, 1, 4, file);

            fread(&header.initial_btrace_buf, 1, 4, file);
            fread(&header.initial_btrace_mode, 1, 4, file);

            fread(&header.pageable_rom_start, 1, 4, file);
            fread(&header.pageable_rom_size, 1, 4, file);

            fread(&header.rom_page_idx, 1, 4, file);
            fread(&header.compressed_unpaged_start, 1, 4, file);

            fread(&header.unpaged_compressed_size, 1, 4, file);
            fread(&header.hcr_file_addr, 1, 4, file);
            fread(&header.spare, 1, 36, file);
        }

        rom_entry read_rom_entry(FILE* file) {
            rom_entry entry;

            fread(&entry.size, 1, 4, file);
            fread(&entry.address_lin, 1, 4, file);
            fread(&entry.attrib, 1, 1, file);
            fread(&entry.name_len, 1, 4, file);

            entry.name.resize(entry.name_len);

            fread(entry.name.data(), 2, entry.name_len, file);

            return entry;
        }

        rom_dir read_rom_dir(FILE* file) {
            rom_dir dir;

            auto old_off = ftell(file);

            fread(&dir.size, 1, 4, file);
        
            while (ftell(file) - old_off < dir.size) {
                dir.entries.push_back(read_rom_entry(file));
                fseek(file, dir.entries.back().size, SEEK_CUR);
            }

            return dir;
        }

        // Loading rom supports only uncompressed rn
        std::optional<rom> load_rom(const std::string& path) {
            rom romf;
            romf.handler = fopen(path.c_str(), "wb");
            romf.header = read_rom_header(romf.handler);

            // Seek to the first entry
            fseek(romf.handler, romf.header.primary_file, SEEK_SET);

            return romf;
        }   
    }
}
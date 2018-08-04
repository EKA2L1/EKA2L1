#include <core/loader/rpkg.h>
#include <core/vfs.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>

#include <array>
#include <vector>

namespace eka2l1 {
    namespace loader {
        bool extract_file(io_system *io, FILE *parent, rpkg_entry &ent) {
            std::string real_path = io->get(common::ucs2_to_utf8(ent.path));

            std::string dir = eka2l1::file_directory(real_path);
            create_directories(dir);

            FILE *wf
                = fopen(real_path.c_str(), "wb");

            if (!wf) {
                LOG_INFO("Skipping with real path: {}, dir: {}", real_path, dir);
                return false;
            }

            int64_t left = ent.data_size;
            int64_t take_def = 0x10000;

            std::array<char, 0x10000> temp;

            while (left) {
                int64_t take = left < take_def ? left : take_def;
                fread(temp.data(), 1, take, parent);
                fwrite(temp.data(), 1, take, wf);

                left -= take;
            }

            fclose(wf);

            return true;
        }

        bool install_rpkg(io_system *io, const std::string &path, std::atomic<int> &res) {
            FILE *f = fopen(path.data(), "rb");

            if (!f) {
                return false;
            }

            rpkg_header header;

            fread(&header.magic, 4, 4, f);

            if (header.magic[0] != 'R' || header.magic[1] != 'P' || header.magic[2] != 'K' || header.magic[3] != 'G') {
                fclose(f);
                return false;
            }

            fread(&header.major_rom, 1, 1, f);
            fread(&header.minor_rom, 1, 1, f);
            fread(&header.build_rom, 1, 2, f);
            fread(&header.count, 1, 4, f);

            while (!feof(f)) {
                rpkg_entry entry;

                fread(&entry.attrib, 1, 8, f);
                fread(&entry.time, 1, 8, f);
                fread(&entry.path_len, 1, 8, f);

                entry.path.resize(entry.path_len);

                fread(entry.path.data(), 1, entry.path_len * 2, f);
                fread(&entry.data_size, 1, 8, f);

                LOG_INFO("Extracting: {}", common::ucs2_to_utf8(entry.path));

                if (!extract_file(io, f, entry)) {
                    fclose(f);
                    return false;
                }

                res += (int)(100 / header.count);
            }

            fclose(f);

            return true;
        }
    }
}
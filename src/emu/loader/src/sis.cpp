#include <loader/sis.h>

#include <cstdio>
#include <cassert>

#include <vector>

namespace eka2l1 {
    namespace loader {
        enum {
            uid1_cst = 0x10201A7a
        };

        bool install_sis(std::string path) {
            FILE* sis_file = fopen(path.c_str(), "rb");

            sis_header header;
            fread(&header,1 ,sizeof(sis_header), sis_file);

            assert(header.uid1 == uid1_cst);

            std::vector<uint16_t> langs;
            langs.resize(header.lang_count);

            if (header.lang_count > 0) {
                fseek(sis_file, header.langs_offset, SEEK_SET);
                fread(langs.data(), header.lang_count, 2, sis_file);
            }

            return true;

            std::vector<sis_requisite_metadata> reqs_metadata;

            if (header.req_count > 0) {
                fseek(sis_file, header.reqs_offset, SEEK_SET);

                for (uint32_t i = 0; i < header.req_count; i++) {
                     fread(&reqs_metadata[i].uid, 1, 4, sis_file);
                     fread(&reqs_metadata[i].major, 1, 2, sis_file);
                     fread(&reqs_metadata[i].minor, 1, 2, sis_file);

                     fread(&reqs_metadata[i].name_len, 1, 4, sis_file);
                }
            }
        }
    }
}

#include <common/log.h>
#include <cstdio>
#include <loader/romimage.h>

namespace eka2l1 {
    namespace loader {
        // Unstable
        std::optional<romimg> parse_romimg(const std::string &path) {
            FILE *dear_imrom = fopen(path.c_str(), "rb");
            romimg img;

            fread(&img.header, 1, sizeof(rom_header), dear_imrom);
            fseek(dear_imrom, img.header.export_dir_address - img.header.code_address + sizeof(rom_header), SEEK_SET);

            fclose(dear_imrom);

            return img;
        }
    }
}

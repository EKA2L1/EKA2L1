#include <loader/eka2img.h>
#include <common/log.h>

#include <cstdio>

namespace eka2l1 {
    namespace loader {
        void try_load_header(const char* file) {
            FILE* f = fopen(file, "rb");
            eka2img_header header;

            fread(&header, 1, sizeof(eka2img_header), f);

            switch (header.cpu) {
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

            fclose(f);
        }
    }
}

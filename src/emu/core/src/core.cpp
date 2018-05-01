#include <core.h>
#include <process.h>

#include <common/log.h>
#include <disasm/disasm.h>
#include <loader/eka2img.h>

namespace eka2l1 {
    namespace core {
        std::shared_ptr<process> crr_process;

        void init() {
            core_timing::init();
            core_kernel::init();
            core_mem::init();

            disasm::init();
        }

        void load(const std::string &name, uint64_t id, const std::string &path) {
            auto img = loader::parse_eka2img(path);

            if (!img) {
                LOG_CRITICAL("This is not what i expected! Fake E32Image!");
                return;
            }

            loader::eka2img &real_img = img.value();
            bool succ = loader::load_eka2img(real_img);

            if (!succ) {
                LOG_CRITICAL("Unable to load EKA2Img!");
                return;
            }

            crr_process = std::make_shared<process>(id, name, real_img.rt_code_addr + real_img.header.entry_point,
                real_img.header.heap_size_min, real_img.header.heap_size_max,
                real_img.header.stack_size);

            crr_process->run();
        }

        int loop() {
            if (core_kernel::crr_running_thread() == nullptr) {
                core_timing::idle();
                core_timing::advance();
            } else {
                core_timing::advance();
            }

            return 1;
        }

        void shutdown() {
            core_timing::shutdown();
            core_kernel::shutdown();
            core_mem::shutdown();
            disasm::shutdown();
        }
    }
}

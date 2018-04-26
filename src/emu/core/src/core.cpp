#include <core.h>
#include <process.h>

#include <loader/eka2img.h>
#include <disasm/disasm.h>

namespace eka2l1 {
    namespace core {
        std::shared_ptr<process> crr_process;

        void init() {
            core_timing::init();
            core_kernel::init();
            core_mem::init();

            disasm::init();
        }

        void load(const std::string& name, uint64_t id, const std::string& path) {
            loader::eka2img img = loader::load_eka2img(path);

            crr_process = std::make_shared<process>(id,name, img.rt_code_addr + img.header.entry_point,
                                  img.header.heap_size_min, img.header.heap_size_max,
                                  img.header.stack_size);

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

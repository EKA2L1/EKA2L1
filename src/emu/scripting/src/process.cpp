#include <scripting/process.h>

namespace eka2l1::scripting {
    process::process(uint64_t handle)
        : process_handle(*reinterpret_cast<eka2l1::process_ptr *>(handle)) {
    }

    bool process::read_process_memory(const size_t addr, std::vector<char> &buffer, const size_t size) {
        void *ptr = process_handle->get_ptr_on_addr_space(addr);

        if (!ptr) {
            return false;
        }

        buffer.resize(size);
        memcpy(&buffer[0], ptr, size);

        return true;
    }

    bool process::write_process_memory(const size_t addr, std::vector<char> buffer) {
        void *ptr = process_handle->get_ptr_on_addr_space(addr);

        if (!ptr) {
            return false;
        }

        memcpy(ptr, buffer.data(), buffer.size());

        return true;
    }

    //std::unique_ptr<eka2l1::page> process::get_page_info(size_t addr) {
    //}
}
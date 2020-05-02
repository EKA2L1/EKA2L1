#include <epoc/kernel/btrace.h>
#include <common/log.h>

namespace eka2l1::kernel {
    btrace::btrace(kernel_system *kern, io_system *io)
        : io_(io)
        , kern_(kern)
        , trace_(nullptr) {
    }

    btrace::~btrace() {
        close_trace_session();
    }

    bool btrace::start_trace_session(const std::u16string &trace_path) {
        if (trace_) {
            return false;
        }

        trace_ = io_->open_file(trace_path, WRITE_MODE);
        return (trace_ != nullptr);
    }

    bool btrace::close_trace_session() {
        if (!trace_) {
            return false;
        }

        return trace_->close();
    }

    static const std::u16string DEFAULT_TRACE_FILE = u"c:\\btrace.txt";

    bool btrace::out(const std::uint32_t a0, const std::uint32_t a1, const std::uint32_t a2,
        const std::uint32_t a3) {
        if (!trace_ && !start_trace_session(DEFAULT_TRACE_FILE)) {
            return false;
        }

        // Check out the header
        const std::uint32_t args_size = (a0 >> (btrace_header_size_index * 8)) & 0xFF;
        const std::uint32_t flags = (a0 >> (btrace_header_flag_index * 8)) & 0xFF;
        const std::uint32_t category = (a0 >> (btrace_header_category_index * 8)) & 0xFF;
        const std::uint32_t subcategory = (a0 >> (btrace_header_subcategory_index * 8)) & 0xFF;

        if (trace_) {
            std::string message = "Trace out (data size = {}, flags = {}, category = {}, subcategory = {}):\n"
                "\ta1 = 0x{:X}\n"
                "\ta2 = 0x{:X}\n"
                "\ta3 = 0x{:X}\n"
                "\n";

            message = fmt::format(message, args_size, flags, category, subcategory, a1, a2, a3);
            trace_->write_file(&message[0], static_cast<std::uint32_t>(message.size()), 1);

            trace_->flush();
        }

        return true;
    }
}
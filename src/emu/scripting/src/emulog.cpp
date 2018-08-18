#include <common/log.h>
#include <scripting/emulog.h>

namespace py = pybind11;

namespace eka2l1::scripting {
    void emulog(const std::string &format, py::args vargs) {
        std::string format_res = format;

        // Since we know all the variable type, it's best that we process all of it
        // This will be O(n^2), where m is number of parantheses pair
        size_t ppos = format_res.find("{}");
        size_t arg_counter = 0;

        while (ppos != std::string::npos) {
            if (arg_counter >= vargs.size()) {
                break;
            }

            const auto &arg = vargs[arg_counter];
            py::str replacement(arg);

            if (!replacement.is_none()) {
                const std::string replacement_std = replacement.cast<std::string>();
                format_res.replace(format_res.begin() + ppos, format_res.begin() + ppos + 2, replacement_std.data());
                ppos = format_res.find("{}");
            } 
              
            arg_counter++;
        }

        LOG_INFO("{}", format_res);
    }
}
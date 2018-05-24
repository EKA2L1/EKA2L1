#include <abi/eabi.h>
#include <cxxabi.h>
#include <memory>

namespace eka2l1 {
    // Global abi
    namespace eabi {
        std::string demangle(std::string target) {
            char* out_buf = reinterpret_cast<char*>(std::malloc(target.length() * 3));
            int status = 0;
            char* res = abi::__cxa_demangle(target.data(), out_buf, nullptr, &status);

            if (status == 0) {
                return res;
            } 

            return target;
        }

        std::string mangle(std::string target) {
            return target;
        }
    }
}
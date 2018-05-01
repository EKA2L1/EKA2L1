#include <common/cvt.h>

#include <codecvt>
#include <locale>

namespace eka2l1 {
    namespace common {
        std::string ucs2_to_utf8(std::u16string str) {
            std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert;
            std::string new_str = convert.to_bytes(str);

            return new_str;
        }
    }
}

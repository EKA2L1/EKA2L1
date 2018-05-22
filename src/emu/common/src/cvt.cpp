#include <common/cvt.h>

#include <codecvt>
#include <locale>

namespace eka2l1 {
    namespace common {
		// VS2017 bug: https://stackoverflow.com/questions/32055357/visual-studio-c-2015-stdcodecvt-with-char16-t-or-char32-t
        std::string ucs2_to_utf8(std::u16string str) {
			std::wstring_convert<std::codecvt_utf8_utf16<int16_t>, int16_t> convert;
            auto p = reinterpret_cast<const int16_t*>(str.data());

            return convert.to_bytes(p, p + str.size());
        }
    }
}

#pragma once

#include <common/types.h>
#include <iostream>
#include <sstream>

namespace eka2l1 {
    namespace common {
        std::string ucs2_to_utf8(std::u16string str);

        template <class T>
        std::string to_string(T t, std::ios_base & (*f)(std::ios_base&))
        {
          std::ostringstream oss;
          oss << f << t;
          return oss.str();
        }
    }
}

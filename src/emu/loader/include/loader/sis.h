#pragma once 

#include <loader/sis_fields.h>

#include <string>
#include <vector>

#include <cstdint>

namespace eka2l1 {
	namespace loader {
        bool install_sis(std::string path, sis_drive drv);
	}
}

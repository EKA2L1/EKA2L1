#pragma once

#include <__cxxabi_config.h>

namespace  __cxxabiv1 {
	extern "C" {
		// 3.4 Demangler API
		extern _LIBCXXABI_FUNC_VIS char *__cxa_demangle(const char *mangled_name,
			char *output_buffer,
			size_t *length, int *status);
	}
}

namespace abi = __cxxabiv1;
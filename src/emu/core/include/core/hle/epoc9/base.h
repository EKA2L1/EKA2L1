#pragma once

#include <epoc9/types.h>

namespace eka2l1 {
	namespace hle {
		namespace epoc9 {
			struct CBase {
				void* vtable;

				void Delete(CBase* base);
			};
		}
	}
}
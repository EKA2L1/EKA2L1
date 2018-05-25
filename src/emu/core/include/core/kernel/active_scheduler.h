#pragma once

#include <kernel/active_obj.h>
#include <memory>
#include <vector>

namespace eka2l1 {
	namespace kernel {
		using active_obj_ptr = std::shared_ptr<active_obj>;

		class active_scheduler {
			std::vector<active_obj_ptr> objs;
		};
	}
}
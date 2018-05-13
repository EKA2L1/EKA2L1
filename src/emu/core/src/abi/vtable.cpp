#include <abi/vtable.h>

namespace eka2l1 {
	namespace abi {
		void vtable::init() {
			this_indices.parent = this;
		}

		deexport& choose_function(vtable_indices& my_indices, deexport them) {
			auto res = std::find_if(my_indices.indices.begin(), my_indices.indices.end(), [them](auto idx) -> bool {
				return idx.name == them.name;
			});

			// No one can replace my boy
			if (res == my_indices.indices.end()) {
				return them;
			}

			// Check attribute
			if (res->at >= them.at) {
				return *res;
			}

			return them;
		}

		void vtable::resolve_once() {
			auto my_indices_copy = this_indices;

			for (auto index : vtabs[0]->indices[0].indices) {
				if (index.is_destructor) {
					continue;
				}
				
				deexport &final_dec = choose_function(my_indices_copy, index);

				if (final_dec.parent == this) {
					my_indices_copy.remove_stub(final_dec.get_sid());
				}

				indices[0].add_stub(final_dec);
			}

			for (auto index_left : my_indices_copy.indices) {
				indices[0].add_stub(index_left);
			}
		}

		void vtable::resolve_multiple() {

		}
	}
}
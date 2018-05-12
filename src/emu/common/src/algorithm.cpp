#include <common/algorithm.h>

namespace eka2l1 {
    namespace common {
        size_t find_nth(std::string targ, std::string str, size_t idx, size_t pos) {
			size_t found_pos = targ.find(str, pos);

			if (1 == idx || found_pos == std::string::npos) {
				return found_pos;
			}

			return find_nth(targ, str, idx - 1, found_pos + 1);
		}

		void remove(std::string& inp, std::string to_remove) {
			size_t pos = 0;

			do {
				pos = inp.find(to_remove, pos);

				if (pos == std::string::npos) {
					break;
				}
				else {
					inp.erase(pos, to_remove.length());
				}
			} while (true);
		}
    }
}
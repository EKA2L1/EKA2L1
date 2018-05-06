#pragma once

#include <common/types.h>

#include <cstdint>
#include <cstring>

namespace eka2l1 {
    namespace common {
        template <typename T>
        constexpr T max(T a, T b) {
            return a > b ? a : b;
        }

        template <typename T>
        constexpr T min(T a, T b) {
            return a > b ? b : a;
        }

        constexpr size_t KB(size_t kb) {
            return kb * 1024;
        }

        constexpr size_t MB(size_t mb) {
            return mb * KB(1024);
        }

        constexpr size_t GB(size_t gb) {
            return gb * MB(1024);
        }

		size_t find_nth(std::string targ, std::string str, size_t idx, size_t pos = 0) {
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

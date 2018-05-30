#pragma once

#include <common/types.h>

#include <cstdint>
#include <cstring>
#include <string>

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

		size_t find_nth(std::string targ, std::string str, size_t idx, size_t pos = 0);
		void remove(std::string& inp, std::string to_remove);
   
		// https://stackoverflow.com/questions/12200486/how-to-remove-duplicates-from-unsorted-stdvector-while-keeping-the-original-or
		struct target_less
		{
			template<class It>
			bool operator()(It const &a, It const &b) const { return *a < *b; }
		};

		struct target_equal
		{
			template<class It>
			bool operator()(It const &a, It const &b) const { return *a == *b; }
		};

		template<class It> It uniquify(It begin, It const end)
		{
			std::vector<It> v;
			v.reserve(static_cast<size_t>(std::distance(begin, end)));
			for (It i = begin; i != end; ++i)
			{
				v.push_back(i);
			}
			std::sort(v.begin(), v.end(), target_less());
			v.erase(std::unique(v.begin(), v.end(), target_equal()), v.end());
			std::sort(v.begin(), v.end());
			size_t j = 0;
			for (It i = begin; i != end && j != v.size(); ++i)
			{
				if (i == v[j])
				{
					using std::iter_swap; iter_swap(i, begin);
					++j;
					++begin;
				}
			}
			return begin;
		}
	}
}

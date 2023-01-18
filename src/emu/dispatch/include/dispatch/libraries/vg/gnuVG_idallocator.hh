/*
 * gnuVG - a free Vector Graphics library
 * Copyright (C) 2014 by Anton Persson
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, either version 3 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <set>
#include <exception>

#include <dispatch/libraries/vg/gnuVG_error.hh>

namespace gnuVG {
	template <typename T>
	class IDAllocator {
	private:
		std::set<T> available_ids;

		T total_amount;
	public:
		class NoIDAvailable : public std::runtime_error {
		public:
			NoIDAvailable() : runtime_error("Not enough IDs available.")
				{}
			virtual ~NoIDAvailable() {}
		};

		class IDFreedTwice : public std::runtime_error {
		public:
			IDFreedTwice() : runtime_error("Tried to free an ID twice.")
				{}
			virtual ~IDFreedTwice() {}
		};

		class IDNotAllocated : public std::runtime_error {
		public:
			IDNotAllocated() : runtime_error("Tried to free a non-allocated ID.")
				{}
			virtual ~IDNotAllocated() {}
		};

		IDAllocator(T reserve_values = 0, T initial_size = 32) : total_amount(initial_size) {
			for(T x = reserve_values; x < initial_size; x++) {
				available_ids.insert(x);
			}
		}

		T get_id() {
			T retval = total_amount;

			if(available_ids.size() > 0) {
				auto iter = available_ids.begin();
				retval = *iter;
				available_ids.erase(iter);
			} else if(total_amount == (T)~0)
				throw NoIDAvailable();
			else ++total_amount;

			return retval;
		}

		void free_id(T id) {
			if(id >= total_amount)
				throw IDNotAllocated();
			if(available_ids.find(id) != available_ids.end())
				throw IDFreedTwice();

			available_ids.insert(id);
		}
	};
};

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

#include <dispatch/libraries/vg/gnuVG_memclasses.hh>

//#define __DO_GNUVG_DEBUG
#include <dispatch/libraries/vg/gnuVG_debug.hh>

namespace gnuVG {
	std::map<void*, GvgArray*> GvgAllocator::active_arrays;
	std::vector<GvgArray*> GvgAllocator::unused_arrays;

	void* GvgAllocator::gvg_alloc(void* /*ignored*/, unsigned int size) {
		GvgArray* gvga;
		if(unused_arrays.size() == 0) {
			gvga = new GvgArray(size);
			GNUVG_DEBUG("Created new GvgArray(), %p\n", fmt::ptr(gvga));
		} else {
			gvga = unused_arrays.back();
			unused_arrays.pop_back();
			gvga->resize(size);
		}
		active_arrays[gvga->data] = gvga;
		return gvga->data;
	}

	void* GvgAllocator::gvg_realloc(void* /*ignored*/, void* ptr, unsigned int new_size) {
		auto i = active_arrays.find(ptr);

		if(i != active_arrays.end()) {
			auto gvga = (*i).second;
			gvga->resize(new_size);
			if(gvga->data != (*i).first) {
				GNUVG_DEBUG("GvgArray() %p was actually resized.\n", fmt::ptr(gvga));
				// ptr changed - update map
				active_arrays.erase(i);
				active_arrays[gvga->data] = gvga;
			}

			return gvga->data;
		}
		return NULL;
	}

	void GvgAllocator::gvg_free(void* /*ignored*/, void* ptr) {
		auto i = active_arrays.find(ptr);

		if(i != active_arrays.end()) {
			auto gvga = (*i).second;
			active_arrays.erase(i);
			unused_arrays.push_back(gvga);
		} else {
			GNUVG_DEBUG("WTF");
        }
	}

};

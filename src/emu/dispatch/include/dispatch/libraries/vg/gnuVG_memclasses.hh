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

#ifndef __GNUVG_MEMCLASSES_HH
#define __GNUVG_MEMCLASSES_HH

#include <stdlib.h>
#include <string.h>
#include <map>
#include <vector>

namespace gnuVG {

	template <typename T>
	class GvgVector {
	private:
		T* __data;
		size_t total_size;
		size_t in_use;
		size_t iterate_head;

		void reallocate(size_t new_size) {
			if(new_size <= total_size) return;

			T* new_data = new T[new_size];
			for(size_t k = 0; k < total_size; k++)
				new_data[k] = __data[k];
			total_size = new_size;
			delete __data;
			__data = new_data;
		}
	public:
		typedef T* iterator;
		typedef const T* const_iterator;

		GvgVector() : total_size(4), in_use(0) {
			__data = new T[total_size];
		}

		~GvgVector() {
			delete[] __data;
		}

		size_t capacity() {
			return total_size;
		}

		void double_capacity() {
			reallocate(2 * total_size);
		}

		void push_back(T value) {
			if(total_size == in_use)
				reallocate(2 * total_size);
			__data[in_use++] = value;
		}

		void append(const T* data, size_t element_count) {
			if(in_use + element_count >= total_size)
				reallocate(2 * (in_use + element_count));
			memcpy(&__data[in_use], data, element_count * sizeof(T));
			in_use += element_count;
		}

		iterator begin() {
			return &__data[0];
		}

		const_iterator begin() const {
			return &__data[0];
		}

		iterator end() {
			return &__data[in_use];
		}

		const_iterator end() const {
			return &__data[in_use];
		}

		T* data() {
			return __data;
		}

		const T* data() const {
			return __data;
		}

		T& operator[](std::size_t element_index) {
			return __data[element_index];
		}

		T* last() { return &__data[in_use - 1]; }

		void clear() {
			in_use = 0;
		}

		size_t size() {
			return in_use;
		}
	};

	struct GvgArray {
		void* data;
		size_t size;

		GvgArray(size_t first_size) : size(first_size) {
			data = (void*)malloc(size);
		}

		~GvgArray() {
			free(data);
		}

		void resize(size_t new_size) {
			if(new_size <= size) return;

			void* new_data = (void*)realloc(data, new_size);
			data = new_data;
			size = new_size;
		}
	};

	class GvgAllocator {
	private:
		static std::map<void*, GvgArray*> active_arrays;
		static std::vector<GvgArray*> unused_arrays;

	public:
		static void* gvg_alloc(void* /*ignored*/, unsigned int size);
		static void* gvg_realloc(void* /*ignored*/, void* ptr, unsigned int new_size);
		static void gvg_free(void* /*ignored*/, void* ptr);
	};

};

#endif

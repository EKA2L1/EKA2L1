/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <functional>

namespace eka2l1 {
    namespace common {
	    /*! \brief Represents a resource.
		 *
		 * A resource can have custom destructor. 
		*/
        template <typename T>
        struct resource {
            using deleter = std::function<void(T)>;

            T res;
            deleter del;

        public:
            resource(T res, deleter del)
                : res(res)
                , del(del) {}

            ~resource() {
                if (del) {
                    del(res);
                }
            }

			/*! \brief Get the actual object.
			 *
			 * \returns A copy of the actual object
			*/
            T get() const {
                return res;
            }
        };
    }
}


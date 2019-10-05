/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project.
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

namespace eka2l1 {
    namespace kernel {
        class process;
    }
}

namespace eka2l1::service {
    class faker;

    class faker_mentor {
    };

    /**
     * \brief Allow manipulation of raw Symbian's native code.
     * 
     * This allow native code to run as a normal Symbian process, also provide a callback and
     * chain like mechanism when a native code function are done executing.
     * 
     * Because this is revolving around Symbian, not everything will work out of the box and be
     * happy about this.
     * 
     * For example, memory allocation will be a burden because this fake thread doesn't have a heap
     * available and a heap allocator with it. Also the exception mechanism (trap) will fail. Which
     * is why this class also does some workaround with specific kernel version in order to make
     * Symbian thought this is like a real thread running.
     */
    class faker {
        kernel::process *process_;

    public:
    };
}
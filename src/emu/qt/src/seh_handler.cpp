/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <common/configure.h>
#include <common/cvt.h>
#include <common/platform.h>

#if EKA2L1_PLATFORM(WIN32) && ENABLE_SEH_HANDLER
#include <Windows.h>
#include <cstdint>
#include <string>

void seh_handler_translator_func(unsigned int u, EXCEPTION_POINTERS *excp) {
    bool should_throw = true;
    std::string description;

    switch (excp->ExceptionRecord->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION: {
        description = "Access violation: Trying to ";

        if (excp->ExceptionRecord->ExceptionInformation[0] == 0) {
            description += "read";
        } else {
            description += "write";
        }

        description += " to invalid address (0x";
        description += eka2l1::common::to_string(excp->ExceptionRecord->ExceptionInformation[1], std::hex);
        description += ")";

        break;
    }

    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: {
        description = "Trying to access out of bounds element!";
        break;
    }

    case EXCEPTION_ILLEGAL_INSTRUCTION: {
        description = "The process trying to executes illegal instruction!";
        break;
    }

    case EXCEPTION_STACK_OVERFLOW: {
        description = "Current thread stack's overflow!";
        break;
    }

    case EXCEPTION_FLT_DIVIDE_BY_ZERO: {
        description = "Divide by zero!";
        break;
    }

    default: {
        should_throw = false;
        break;
    }
    }

    if (should_throw) {
        throw std::runtime_error(description);
    }
}
#endif

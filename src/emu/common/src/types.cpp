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
#include <common/platform.h>
#include <common/types.h>

#if EKA2L1_PLATFORM(UNIX)
#include <sys/mman.h>
#include <unistd.h>
#elif EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#endif

#include <cassert>
#include <cwctype>

int translate_protection(prot cprot) {
    int tprot = 0;

    // TODO: Remove horrible ifelse and replace with switchs :(
    if (cprot == prot::none) {
#if EKA2L1_PLATFORM(POSIX)
        tprot = PROT_NONE;
#else
        tprot = PAGE_NOACCESS;
#endif
    } else if (cprot == prot::read) {
#if EKA2L1_PLATFORM(POSIX)
        tprot = PROT_READ;
#else
        tprot = PAGE_READONLY;
#endif
    } else if (cprot == prot::exec) {
#if EKA2L1_PLATFORM(POSIX)
        tprot = PROT_EXEC;
#else
        tprot = PAGE_EXECUTE;
#endif
    } else if (cprot == prot::read_write) {
#if EKA2L1_PLATFORM(POSIX)
        tprot = PROT_READ | PROT_WRITE;
#else
        tprot = PAGE_READWRITE;
#endif
    } else if (cprot == prot::read_exec) {
#if EKA2L1_PLATFORM(POSIX)
        tprot = PROT_READ | PROT_EXEC;
#else
        tprot = PAGE_EXECUTE_READ;
#endif
    } else if (cprot == prot::read_write_exec) {
#if EKA2L1_PLATFORM(POSIX)
        tprot = PROT_READ | PROT_WRITE | PROT_EXEC;
#else
        tprot = PAGE_EXECUTE_READWRITE;
#endif
    } else {
        tprot = -1;
    }

    return tprot;
}

char16_t drive_to_char16(const drive_number drv) {
    return static_cast<char16_t>(drv) + u'A';
}

drive_number char16_to_drive(const char16_t c) {
    const char16_t cl = std::towlower(c);

    if ((cl < u'a') || (cl > u'z')) {
        assert(false && "Invalid drive character");
    }

    return static_cast<drive_number>(cl - 'a');
}

const char *num_to_lang(const int num) {
    switch (static_cast<language>(num)) {
    #define LANG_DECL(x, y) case language::x: return #y;
        #include <common/lang.def>
    #undef LANG_DECL
    default:
        break;
    }

    return nullptr;
}

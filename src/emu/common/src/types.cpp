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

#if EKA2L1_PLATFORM(UNIX) || EKA2L1_PLATFORM(DARWIN)
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
#define LANG_DECL(x, y) \
    case language::x:   \
        return #y;
#include <common/lang.def>
#undef LANG_DECL
    default:
        break;
    }

    return nullptr;
}

const char *epocver_to_string(const epocver ver) {
    switch (ver) {
    case epocver::epocu6:
        return "epocu6";

    case epocver::epoc6:
        return "epoc6";

    case epocver::epoc80:
        return "epoc80";

    case epocver::epoc93:
        return "epoc93";

    case epocver::epoc94:
        return "epoc94";

    case epocver::epoc95:
        return "epoc95";

    case epocver::epoc10:
        return "epoc100";

    default:
        break;
    }

    return nullptr;
}

const epocver string_to_epocver(const char *str) {
    std::string str_std(str);

    if (str_std == "epocu6") {
        return epocver::epocu6;
    }

    if (str_std == "epoc6") {
        return epocver::epoc6;
    }

    if (str_std == "epoc80") {
        return epocver::epoc80;
    }

    if (str_std == "epoc93") {
        return epocver::epoc93;
    }
    
    if (str_std == "epoc94") {
        return epocver::epoc94;
    }
    
    if (str_std == "epoc95") {
        return epocver::epoc95;
    }
    
    if (str_std == "epoc10") {
        return epocver::epoc10;
    }

    return epocver::epoc94;
}
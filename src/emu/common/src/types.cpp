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
#include <common/types.h>

#ifndef WIN32
#include <sys/mman.h>
#include <unistd.h>
#else
#include <Windows.h>
#endif

int translate_protection(prot cprot) {
    int tprot = 0;

    if (cprot == prot::none) {
#ifndef WIN32
        tprot = PROT_NONE;
#else
        tprot = PAGE_NOACCESS;
#endif
    } else if (cprot == prot::read) {
#ifndef WIN32
        tprot = PROT_READ;
#else
        tprot = PAGE_READONLY;
#endif
    } else if (cprot == prot::exec) {
#ifndef WIN32
        tprot = PROT_EXEC;
#else
        tprot = PAGE_EXECUTE;
#endif
    } else if (cprot == prot::read_write) {
#ifndef WIN32
        tprot = PROT_READ | PROT_WRITE;
#else
        tprot = PAGE_READWRITE;
#endif
    } else if (cprot == prot::read_exec) {
#ifndef WIN32
        tprot = PROT_READ | PROT_EXEC;
#else
        tprot = PAGE_EXECUTE_READ;
#endif
    } else if (cprot == prot::read_write_exec) {
#ifndef WIN32
        tprot = PROT_READ | PROT_WRITE | PROT_EXEC;
#else
        tprot = PAGE_EXECUTE_READWRITE;
#endif
    } else {
        tprot = -1;
    }

    return tprot;
}


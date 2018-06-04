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

#include <cstdint>
#include <ptr.h>

typedef char TInt8;
typedef short TInt16;
typedef int32_t TInt32;
typedef int64_t TInt64;

typedef int TInt;
typedef unsigned int TUint;
typedef bool TBool;

typedef unsigned char TUint8;
typedef unsigned short TUint16;
typedef uint32_t TUint32;
typedef uint64_t TUint64;

using TAny = void;

enum TOwnerType {
    EOwnerProcess,
    EOwnerThread
};

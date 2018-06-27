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

#include <hle/bridge.h>

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

using TUid = uint32_t;

struct TUidType {
    TUid uid1;
    TUid uid2;
    TUid uid3;
};

const TUint KLocaleLanguageKey = 0x10208903;
const TUint KLocaleDataKey = 0x10208904;
const TUint KLocaleDataExtraKey = 0x10208905;
const TUint KLocaleTimeDateFormatKey = 0x10208907;
const TUint KLocaleDefaultCharSetKey = 0x10208908;
const TUint KLocalePreferredCharSetKey = 0x10208909;
/**
@publishedPartner
@released
File system UID value 16.
*/
const TInt KFileSystemUidValue16 = 0x100039df;

/**
@publishedPartner
@released
File system UID value 8.
*/
const TInt KFileSystemUidValue8 = 0x1000008f;

/**
@publishedPartner
@released
File server UID value 16.
*/
const TInt KFileServerUidValue16 = 0x100039e3;

/**
@publishedPartner
@released
File server UID value 8.
*/
const TInt KFileServerUidValue8 = 0x100000bb;

/**
@publishedPartner
@released
File server DLL UID value 16.
*/
const TInt KFileServerDllUidValue16 = 0x100039e4;

/**
@publishedPartner
@released
File server DLL UID value 8.
*/
const TInt KFileServerDllUidValue8 = 0x100000bd;

/**
@publishedPartner
@released
Local file system UID value.
*/
const TInt KLocalFileSystemUidValue = 0x100000d6;

/**
@publishedPartner
@released
Estart component UID value.
*/
const TInt KEstartUidValue = 0x10272C04;

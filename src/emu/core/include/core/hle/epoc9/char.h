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

#include <epoc9/err.h>
#include <epoc9/types.h>

#include <hle/bridge.h>
#include <ptr.h>

using namespace eka2l1;

struct TChar {
    TUint iChar;
};

BRIDGE_FUNC(void, TCharConstructor1, ptr<TChar> aThis);
BRIDGE_FUNC(void, TCharConstructor2, ptr<TChar> aThis, TUint aMyChar);
BRIDGE_FUNC(TBool, TCharEos, ptr<TChar> aThis);
BRIDGE_FUNC(TUint, TCharGetLowerCase, ptr<TChar> aThis);
BRIDGE_FUNC(TUint, TCharGetNumericValue, ptr<TChar> aThis);
BRIDGE_FUNC(TUint, TCharGetUpperCase, ptr<TChar> aThis);
BRIDGE_FUNC(TUint, TCharGetTitleCase, ptr<TChar> aThis);
BRIDGE_FUNC(TBool, TCharIsAssigned, ptr<TChar> aThis);
BRIDGE_FUNC(TBool, TCharIsDigit, ptr<TChar> aThis);
BRIDGE_FUNC(TBool, TCharIsHexDigit, ptr<TChar> aThis);
BRIDGE_FUNC(TBool, TCharIsLower, ptr<TChar> aThis);
BRIDGE_FUNC(TBool, TCharIsUpper, ptr<TChar> aThis);
BRIDGE_FUNC(TBool, TCharIsPunctuation, ptr<TChar> aThis);
BRIDGE_FUNC(TBool, TCharIsSpace, ptr<TChar> aThis);
BRIDGE_FUNC(void, TCharLowercase, ptr<TChar> aThis);
BRIDGE_FUNC(void, TCharUppercase, ptr<TChar> aThis);
BRIDGE_FUNC(void, TCharTitlecase, ptr<TChar> aThis);

extern const eka2l1::hle::func_map char_register_funcs;
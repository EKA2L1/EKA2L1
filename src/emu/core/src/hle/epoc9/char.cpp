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

#include <epoc9/char.h>

BRIDGE_FUNC(void, TCharConstructor1, ptr<TChar> aThis) {
    // No problem here.
    TChar *symchar = aThis.get(sys->get_memory_system());
    symchar->iChar = -1;
}

BRIDGE_FUNC(void, TCharConstructor2, ptr<TChar> aThis, TUint aMyChar) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    symchar->iChar = aMyChar;
}

BRIDGE_FUNC(TBool, TCharEos, ptr<TChar> aThis) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    return symchar->iChar == 0;
}

BRIDGE_FUNC(TUint, TCharGetLowerCase, ptr<TChar> aThis) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    return std::tolower(symchar->iChar);
}
BRIDGE_FUNC(TUint, TCharGetNumericValue, ptr<TChar> aThis) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    return std::atoi((char*)(&symchar->iChar));
}

BRIDGE_FUNC(TUint, TCharGetUpperCase, ptr<TChar> aThis) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    return std::toupper(symchar->iChar);
}

BRIDGE_FUNC(TUint, TCharGetTitleCase, ptr<TChar> aThis) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    return std::toupper(symchar->iChar);
}

BRIDGE_FUNC(TBool, TCharIsDigit, ptr<TChar> aThis) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    return std::isdigit(symchar->iChar);
}

BRIDGE_FUNC(TBool, TCharIsAssigned, ptr<TChar> aThis) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    return symchar->iChar != -1;
}

BRIDGE_FUNC(TBool, TCharIsHexDigit, ptr<TChar> aThis) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    return std::isxdigit(symchar->iChar);
}
BRIDGE_FUNC(TBool, TCharIsLower, ptr<TChar> aThis) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    return std::islower(symchar->iChar);
}

BRIDGE_FUNC(TBool, TCharIsUpper, ptr<TChar> aThis) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    return std::isupper(symchar->iChar);
}

BRIDGE_FUNC(TBool, TCharIsPunctuation, ptr<TChar> aThis) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    return symchar->iChar == '(' || symchar->iChar == ')';
}

BRIDGE_FUNC(TBool, TCharIsSpace, ptr<TChar> aThis) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    return std::isspace(symchar->iChar);
}

BRIDGE_FUNC(void, TCharLowercase, ptr<TChar> aThis) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    symchar->iChar = std::tolower(symchar->iChar);
}

BRIDGE_FUNC(void, TCharUppercase, ptr<TChar> aThis) {
    TChar *symchar = aThis.get(sys->get_memory_system());
    symchar->iChar = std::toupper(symchar->iChar);
}

BRIDGE_FUNC(void, TCharTitleCase, ptr<TChar> aThis) {
    TCharUppercase(sys, aThis);
}

const eka2l1::hle::func_map char_register_funcs = {
    BRIDGE_REGISTER(3998146052, TCharIsAssigned),
    BRIDGE_REGISTER(756491180, TCharIsHexDigit),
    BRIDGE_REGISTER(5426689, TCharGetLowerCase),
    BRIDGE_REGISTER(1117485722, TCharGetTitleCase),
    BRIDGE_REGISTER(2235082596, TCharGetUpperCase),
    BRIDGE_REGISTER(996587635, TCharIsPunctuation),
    BRIDGE_REGISTER(4288010511, TCharGetNumericValue),
    BRIDGE_REGISTER(2521522205, TCharIsDigit),
    BRIDGE_REGISTER(222005237, TCharIsLower),
    BRIDGE_REGISTER(685305528, TCharIsSpace),
    BRIDGE_REGISTER(3286116312, TCharIsUpper)
};
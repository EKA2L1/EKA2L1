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

#include <epoc9/base.h>
#include <epoc9/des.h>

#include <hle/bridge.h>

struct CConsoleBase : public CBase {

};

struct CColorConsoleBase : public CConsoleBase {
};

struct TSize {
    TInt iWidth;
    TInt iHeight;
};

struct CConsoleBaseAdvance : public CConsoleBase {
    TInt iNameLen;
    eka2l1::ptr<TUint16> iName;
    TSize iConsoleSize;
    std::vector<std::string> iPrintList;

    TUint32 iCurrentX;
    TUint32 iCurrentY;
};

BRIDGE_FUNC(eka2l1::ptr<CConsoleBase>, ConsoleNewL, eka2l1::ptr<TLit> aName, TSize aSize);
BRIDGE_FUNC(void, CConsoleBaseAdvanceWrite, eka2l1::ptr<CConsoleBase> aConsole, eka2l1::ptr<TDesC> aDes);

extern const eka2l1::hle::func_map cons_register_funcs;
extern const eka2l1::hle::func_map cons_custom_register_funcs;
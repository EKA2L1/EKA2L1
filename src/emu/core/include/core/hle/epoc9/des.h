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

#include <string>

using namespace eka2l1;

const TInt KShiftDes8Type = 28;
const TInt KShiftDes16Type = 28;

enum TDesType {
    EBufC,
    EPtrC,
    EPtr,
    EBuf,
    EBufCPtr
};

struct TDesC8 {
    TUint iLength;

    TUint8 *Ptr(eka2l1::system *sys);
    TUint Length() const;
    TDesType Type() const;

    bool Compare(eka2l1::system *sys, TDesC8 &aRhs);
    std::string StdString(eka2l1::system *sys);

    void SetLength(eka2l1::system *sys, TUint32 iNewLength);
    void Assign(eka2l1::system *sys, std::string iNewString);
};

struct TDesC16 {
    TUint iLength;

    TUint16 *Ptr(eka2l1::system *sys);
    TUint Length() const;
    TDesType Type() const;

    bool Compare(eka2l1::system *sys, TDesC16 &aRhs);
    std::u16string StdString(eka2l1::system *sys);

    void SetLength(eka2l1::system *sys, TUint32 iNewLength);
    void Assign(eka2l1::system *sys, std::string iNewString);
};

struct TDes8 : public TDesC8 {};
struct TDes16 : public TDesC16 {};

struct TPtrC8 : public TDesC8 {
    ptr<TUint8> iPtr;
};

struct TPtr8 : public TDes8 {
    TInt iMaxLength;
    ptr<TUint8> iPtr;
};

struct TPtrC16 : public TDesC16 {
    ptr<TUint16> iPtr;
};

struct TPtr16 : public TDes16 {
    ptr<TUint16> iPtr;
};

struct TBufCBase8 : public TDesC8 {};
struct TBufCBase16 : public TDesC16 {};

struct TBufC8 : public TBufCBase8 {
    ptr<TUint8> iBuf;
};

struct TBufCPtr8 {
    TInt iLength;
    TInt iMaxLength;
    ptr<TBufC8> iPtr;
};

struct TBufC16 {
    TInt iLength;
    ptr<TUint16> iBuf;
};

struct TBufCPtr16 {
    TInt iLength;
    TInt iMaxLength;
    ptr<TBufC16> iPtr;
};

struct TBuf16 {
    TInt iLength;
    TInt iMaxLength;
    ptr<TUint16> iBuf;
};

struct TBuf8 {
    TInt iLength;
    TInt iMaxLength;
    ptr<TUint8> iBuf;
};

using TDesC = TDesC16;
using TDes = TDes16;
using TBufC = TBufC16;
using TPtrC = TPtrC16;
using TPtr = TPtr16;

// TDes Utility

TInt GetTDesC8Type(const TDesC8 *aDes8);
ptr<TUint8> GetTDes8HLEPtr(eka2l1::system *sys, TDesC8 *aDes8);
TUint8 *GetTDes8Ptr(eka2l1::system *sys, TDesC8 *aDes8);

TInt GetTDesC16Type(const TDesC16 *aDes16);
ptr<TUint16> GetTDes16HLEPtr(eka2l1::system *sys, TDesC16 *aDes16);
TUint16 *GetTDes16Ptr(eka2l1::system *sys, TDesC16 *aDes16);

struct TLit8 {
    TUint iTypeLength;
};

struct TLit16 : public TLit8 {};

TUint8 *GetLit8Ptr(memory_system *mem, eka2l1::ptr<TLit8> aLit);
TUint16 *GetLit16Ptr(memory_system *mem, eka2l1::ptr<TLit16> aLit);

void SetLengthDes(eka2l1::system *sys, TDesC8 *des, uint32_t len);
void SetLengthDes(eka2l1::system *sys, TDesC16 *des, uint32_t len);

uint32_t ExtractDesLength(uint32_t len);

using TLit = TLit16;

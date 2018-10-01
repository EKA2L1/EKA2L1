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

#include <core/epoc/des.h>
#include <core/core.h>

#include <core/kernel/process.h>

namespace eka2l1::epoc {
    TInt GetTDesC8Type(const TDesC8 *aDes8) {
        if (!aDes8) {
            return -1;
        }

        return aDes8->iLength >> KShiftDes8Type;
    }

    ptr<TUint8> GetTDes8HLEPtr(eka2l1::process_ptr sys, TDesC8 *aDes8) {
        if (!aDes8) {
            return ptr<TUint8>(0);
        }

        // Symbian wants to keep it as safe as possible, so they turn them
        // into this mess
        switch (static_cast<TDesType>(GetTDesC8Type(aDes8))) {
        case EPtrC:
            return (reinterpret_cast<TPtrC8 *>(aDes8))->iPtr;

        case EPtr:
            return (reinterpret_cast<TPtr8 *>(aDes8))->iPtr;

        default:
            break;
        }

        return eka2l1::ptr<TUint8>(0);
    }

    TUint8 *GetTDes8Ptr(eka2l1::process_ptr sys, TDesC8 *aDes8) {
        TDesType destype = static_cast<TDesType>(GetTDesC8Type(aDes8));

        if (destype == EBufC) {
            return reinterpret_cast<TUint8 *>(aDes8) + 4;
        } else if (destype == EBuf) {
            return reinterpret_cast<TUint8 *>(aDes8) + 8;
        } else if (destype == EBufCPtr) {
            ptr<TBufC8> buf = reinterpret_cast<TBufCPtr8 *>(aDes8)->iPtr;
            TBufC8 *ptr = static_cast<TBufC8*>(sys->get_ptr_on_addr_space(buf.ptr_address()));

            return reinterpret_cast<TUint8 *>(ptr) + 4;
        }

        ptr<TUint8> des_ptr = GetTDes8HLEPtr(sys, aDes8);
        return static_cast<TUint8*>(sys->get_ptr_on_addr_space(des_ptr.ptr_address()));
    }

    TInt GetTDesC16Type(const TDesC16 *aDes16) {
        return GetTDesC8Type(reinterpret_cast<const TDesC8 *>(aDes16));
    }

    ptr<TUint16> GetTDes16HLEPtr(eka2l1::process_ptr sys, TDesC16 *aDes16) {
        if (!aDes16) {
            return ptr<TUint16>(0);
        }

        TDesType destype = static_cast<TDesType>(GetTDesC16Type(aDes16));

        // Symbian wants to keep it as safe as possible, so they turn them
        // into this mess
        switch (destype) {
        case EPtrC:
            return (reinterpret_cast<TPtrC16 *>(aDes16))->iPtr;

        case EPtr:
            return (reinterpret_cast<TPtr16 *>(aDes16))->iPtr;

        default:
            break;
        }

        return ptr<TUint16>(0);
    }

    ptr<TUint16> GetTDes16HLEPtr(eka2l1::process_ptr sys, ptr<TDesC16> aDes16) {
        if (!aDes16) {
            return ptr<TUint16>(0);
        }

        TDesC16 *des = static_cast<TDesC16*>(sys->get_ptr_on_addr_space(aDes16.ptr_address()));

        TDesType destype = static_cast<TDesType>(GetTDesC16Type(des));

        // Symbian wants to keep it as safe as possible, so they turn them
        // into this mess
        switch (destype) {
        case EPtrC:
            return (reinterpret_cast<TPtrC16 *>(des))->iPtr;

        case EPtr:
            return (reinterpret_cast<TPtr16 *>(des))->iPtr;

        case EBufC:
            return aDes16.cast<TUint16>() + 4;

        case EBuf:
            return aDes16.cast<TUint16>() + 8;

        case EBufCPtr:
            ptr<TBufC16> buf = reinterpret_cast<TBufCPtr16 *>(des)->iPtr;
            return buf.cast<TUint16>() + 4;
        }

        return ptr<TUint16>(0);
    }

    ptr<TUint8> GetTDes8HLEPtr(eka2l1::process_ptr sys, ptr<TDesC8> aDes8) {
        if (!aDes8) {
            return ptr<TUint8>(0);
        }

        TDesC8 *des = static_cast<TDesC8 *>(sys->get_ptr_on_addr_space(aDes8.ptr_address()));

        TDesType destype = static_cast<TDesType>(GetTDesC8Type(des));

        // Symbian wants to keep it as safe as possible, so they turn them
        // into this mess
        switch (destype) {
        case EPtrC:
            return (reinterpret_cast<TPtrC8 *>(des))->iPtr;

        case EPtr:
            return (reinterpret_cast<TPtr8 *>(des))->iPtr;

        case EBufC:
            return aDes8.cast<TUint8>() + 4;

        case EBuf:
            return aDes8.cast<TUint8>() + 8;

        case EBufCPtr:
            ptr<TBufC8> buf = reinterpret_cast<TBufCPtr8 *>(des)->iPtr;
            return buf.cast<TUint8>() + 4;
        }

        return ptr<TUint8>(0);
    }

    TUint16 *GetTDes16Ptr(eka2l1::process_ptr sys, TDesC16 *aDes16) {
        TDesType destype = static_cast<TDesType>(GetTDesC16Type(aDes16));

        if (destype == EBufC) {
            return reinterpret_cast<TUint16 *>(reinterpret_cast<TUint8 *>(aDes16) + 4);
        } else if (destype == EBuf) {
            return reinterpret_cast<TUint16 *>(reinterpret_cast<TUint8 *>(aDes16) + 8);
        } else if (destype == EBufCPtr) {
            ptr<TBufC16> buf = reinterpret_cast<TBufCPtr16 *>(aDes16)->iPtr;
            TBufC16 *ptr = static_cast<TBufC16*>(sys->get_ptr_on_addr_space(buf.ptr_address()));

            return reinterpret_cast<TUint16 *>(reinterpret_cast<TUint8 *>(ptr) + 4);
        }

        ptr<TUint16> des_ptr = GetTDes16HLEPtr(sys, aDes16);
        return static_cast<TUint16*>(sys->get_ptr_on_addr_space(des_ptr.ptr_address()));
    }

    TUint8 *GetLit8Ptr(memory_system *mem, eka2l1::ptr<TLit8> aLit) {
        return reinterpret_cast<TUint8 *>(aLit.cast<TUint8>().get(mem) + 4);
    }

    TUint16 *GetLit16Ptr(memory_system *mem, eka2l1::ptr<TLit16> aLit) {
        return reinterpret_cast<TUint16 *>(GetLit8Ptr(mem, aLit.cast<TLit8>()));
    }

    void SetLengthDes(eka2l1::process_ptr sys, TDesC8 *des, uint32_t len) {
        auto t = des->iLength >> 28;
        des->iLength = len | (t << 28);

        if (GetTDesC8Type(des) == EBufCPtr) {
            TBufCPtr8 *buf_ptr = reinterpret_cast<TBufCPtr8 *>(des);
            eka2l1::ptr<TBufC8> buf_hle = buf_ptr->iPtr;

            TBufC8 *real_buf = static_cast<TBufC8*>(sys->get_ptr_on_addr_space(buf_hle.ptr_address()));
            real_buf->iLength = len;
        }
    }

    void SetLengthDes(eka2l1::process_ptr sys, TDesC16 *des, uint32_t len) {
        SetLengthDes(sys, reinterpret_cast<TDesC8 *>(des), len);
    }

    uint32_t ExtractDesLength(uint32_t len) {
        return len & 0xFFFFFF;
    }

    TUint8 *TDesC8::Ptr(eka2l1::system *sys) {
        return GetTDes8Ptr(sys->get_kernel_system()->crr_process(), this);
    }

    TUint8 *TDesC8::Ptr(eka2l1::process_ptr pr) {
        return GetTDes8Ptr(pr, this);
    }

    TUint TDesC8::Length() const {
        return iLength & 0xFFFFFF;
    }

    TDesType TDesC8::Type() const {
        return static_cast<TDesType>(GetTDesC8Type(this));
    }

    using namespace eka2l1;

    bool TDesC8::Compare(eka2l1::system *sys, TDesC8 &aRhs) {
        return StdString(sys) == aRhs.StdString(sys);
    }

    std::string TDesC8::StdString(eka2l1::system *sys) {
        TUint8 *cstr_ptr = Ptr(sys);

        std::string new_inst;
        new_inst.resize(Length());

        memcpy(new_inst.data(), cstr_ptr, new_inst.size());

        return new_inst;
    }

    TUint16 *TDesC16::Ptr(eka2l1::system *sys) {
        return GetTDes16Ptr(sys->get_kernel_system()->crr_process(), this);
    }

    TUint16 *TDesC16::Ptr(eka2l1::process_ptr pr) {
        return GetTDes16Ptr(pr, this);
    }

    TUint TDesC16::Length() const {
        return iLength & 0xFFFFFF;
    }

    TDesType TDesC16::Type() const {
        return static_cast<TDesType>(GetTDesC16Type(this));
    }

    bool TDesC16::Compare(eka2l1::system *sys, TDesC16 &aRhs) {
        return StdString(sys) == aRhs.StdString(sys);
    }

    std::u16string TDesC16::StdString(eka2l1::system *sys) {
        TUint16 *cstr_ptr = Ptr(sys);

        std::u16string new_inst;
        new_inst.resize(Length());

        memcpy(new_inst.data(), cstr_ptr, new_inst.size() * 2);

        return new_inst;
    }

    std::u16string TDesC16::StdString(eka2l1::process_ptr pr) {
        TUint16 *cstr_ptr = Ptr(pr);

        std::u16string new_inst;
        new_inst.resize(Length());

        memcpy(new_inst.data(), cstr_ptr, new_inst.size() * 2);

        return new_inst;
    }

    std::string TDesC8::StdString(eka2l1::process_ptr pr) {
        TUint8 *cstr_ptr = Ptr(pr);

        std::string new_inst;
        new_inst.resize(Length());

        memcpy(new_inst.data(), cstr_ptr, new_inst.size());

        return new_inst;
    }

    void TDesC8::SetLength(eka2l1::system *sys, TUint32 iNewLength) {
        SetLengthDes(sys->get_kernel_system()->crr_process(), this, iNewLength);
    }

    void TDesC8::SetLength(eka2l1::process_ptr pr, TUint32 iNewLength) {
        SetLengthDes(pr, this, iNewLength);
    }

    void TDesC16::SetLength(eka2l1::process_ptr pr, TUint32 iNewLength) {
        SetLengthDes(pr, this, iNewLength);
    }

    void TDesC8::Assign(eka2l1::system *sys, std::string iNewString) {
        memcpy(Ptr(sys), iNewString.data(), iNewString.length());
        SetLength(sys, iNewString.length());
    }

    void TDesC8::Assign(eka2l1::process_ptr pr, std::string iNewString) {
        memcpy(Ptr(pr), iNewString.data(), iNewString.length());
        SetLength(pr, iNewString.length());
    }

    void TDesC16::SetLength(eka2l1::system *sys, TUint32 iNewLength) {
        SetLengthDes(sys->get_kernel_system()->crr_process(), this, iNewLength);
    }

    void TDesC16::Assign(eka2l1::system *sys, std::u16string iNewString) {
        memcpy(Ptr(sys), iNewString.data(), iNewString.length() * 2);
        SetLength(sys, iNewString.length());
    }

    void TDesC16::Assign(eka2l1::process_ptr pr, std::u16string iNewString) {
        memcpy(Ptr(pr), iNewString.data(), iNewString.length() * 2);
        SetLength(pr, iNewString.length());
    }
}
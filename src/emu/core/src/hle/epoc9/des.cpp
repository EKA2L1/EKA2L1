#include <epoc9/des.h>

TInt GetTDesC8Type(const TDesC8 *aDes8) {
    if (!aDes8) {
        return -1;
    }

    return aDes8->iLength >> KShiftDes8Type;
}

ptr<TUint8> GetTDes8HLEPtr(eka2l1::system *sys, TDesC8 *aDes8) {
    if (!aDes8) {
        return ptr<TUint8>(nullptr);
    }

    // Symbian wants to keep it as safe as possible, so they turn them
    // into this mess
    switch (static_cast<TDesType>(GetTDesC8Type(aDes8))) {
    case EPtrC:
        return (reinterpret_cast<TPtrC8 *>(aDes8))->iPtr;

    case EPtr:
        return (reinterpret_cast<TPtr8 *>(aDes8))->iPtr;
    }

    return eka2l1::ptr<TUint8>(nullptr);
}

TUint8 *GetTDes8Ptr(eka2l1::system *sys, TDesC8 *aDes8) {
    TDesType destype = static_cast<TDesType>(GetTDesC8Type(aDes8));

    if (destype == EBufC) {
        return reinterpret_cast<TUint8 *>(aDes8) + 4;
    } else if (destype == EBuf) {
        return reinterpret_cast<TUint8 *>(aDes8) + 8;
    } else if (destype == EBufCPtr) {
        ptr<TBufC8> buf = reinterpret_cast<TBufCPtr8 *>(aDes8)->iPtr;
        TBufC8 *ptr = buf.get(sys->get_memory_system());

        return reinterpret_cast<TUint8 *>(ptr) + 4;
    }

    ptr<TUint8> des_ptr = GetTDes8HLEPtr(sys, aDes8);
    return des_ptr.get(sys->get_memory_system());
}

TInt GetTDesC16Type(const TDesC16 *aDes16) {
    return GetTDesC8Type(reinterpret_cast<const TDesC8 *>(aDes16));
}

ptr<TUint16> GetTDes16HLEPtr(eka2l1::system *sys, TDesC16 *aDes16) {
    if (!aDes16) {
        return ptr<TUint16>(nullptr);
    }

    TDesType destype = static_cast<TDesType>(GetTDesC16Type(aDes16));

    // Symbian wants to keep it as safe as possible, so they turn them
    // into this mess
    switch (destype) {
    case EPtrC:
        return (reinterpret_cast<TPtrC16 *>(aDes16))->iPtr;

    case EPtr:
        return (reinterpret_cast<TPtr16 *>(aDes16))->iPtr;
    }

    return ptr<TUint16>(nullptr);
}

TUint16 *GetTDes16Ptr(eka2l1::system *sys, TDesC16 *aDes16) {
    TDesType destype = static_cast<TDesType>(GetTDesC16Type(aDes16));

    if (destype == EBufC) {
        return reinterpret_cast<TUint16 *>(reinterpret_cast<TUint8 *>(aDes16) + 4);
    } else if (destype == EBuf) {
        return reinterpret_cast<TUint16 *>(reinterpret_cast<TUint8 *>(aDes16) + 8);
    } else if (destype == EBufCPtr) {
        ptr<TBufC16> buf = reinterpret_cast<TBufCPtr16 *>(aDes16)->iPtr;
        TBufC16 *ptr = buf.get(sys->get_memory_system());

        return reinterpret_cast<TUint16 *>(reinterpret_cast<TUint8 *>(ptr) + 4);
    }

    ptr<TUint16> des_ptr = GetTDes16HLEPtr(sys, aDes16);
    return des_ptr.get(sys->get_memory_system());
}

TUint8 *GetLit8Ptr(memory_system *mem, eka2l1::ptr<TLit8> aLit) {
    return reinterpret_cast<TUint8 *>(aLit.cast<TUint8>().get(mem) + 4);
}

TUint16 *GetLit16Ptr(memory_system *mem, eka2l1::ptr<TLit16> aLit) {
    return reinterpret_cast<TUint16 *>(GetLit8Ptr(mem, aLit.cast<TLit8>()));
}

void SetLengthDes(eka2l1::system *sys, TDesC8 *des, uint32_t len) {
    auto t = des->iLength >> 28;
    des->iLength = len | (t << 28);

    if (GetTDesC8Type(des) == EBufCPtr) {
        TBufCPtr8 *buf_ptr = reinterpret_cast<TBufCPtr8 *>(des);

        eka2l1::ptr<TBufC8> buf_hle = buf_ptr->iPtr;

        TBufC8 *real_buf = buf_hle.get(sys->get_memory_system());
        real_buf->iLength = len | (EBufC << 28);
    }
}

void SetLengthDes(eka2l1::system *sys, TDesC16 *des, uint32_t len) {
    SetLengthDes(sys, reinterpret_cast<TDesC8 *>(des), len);
}

uint32_t ExtractDesLength(uint32_t len) {
    return len & 0xFFFFFF;
}

TUint8 *TDesC8::Ptr(eka2l1::system *sys) {
    return GetTDes8Ptr(sys, this);
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
    return std::string(cstr_ptr, cstr_ptr + Length());
}

TUint16 *TDesC16::Ptr(eka2l1::system *sys) {
    return GetTDes16Ptr(sys, this);
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
    return std::u16string(cstr_ptr, cstr_ptr + Length());
}

void TDesC8::SetLength(eka2l1::system *sys, TUint32 iNewLength) {
    SetLengthDes(sys, this, iNewLength);
}

void TDesC8::Assign(eka2l1::system *sys, std::string iNewString) {
    memcpy(Ptr(sys), iNewString.data(), iNewString.length());
    SetLength(sys, iNewString.length());
}

void TDesC16::SetLength(eka2l1::system *sys, TUint32 iNewLength) {
    SetLengthDes(sys, this, iNewLength);
}

void TDesC16::Assign(eka2l1::system *sys, std::string iNewString) {
    memcpy(Ptr(sys), iNewString.data(), iNewString.length());
    SetLength(sys, iNewString.length());
}
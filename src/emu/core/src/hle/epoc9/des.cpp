#include <epoc9/des.h>

TInt GetTDesC8Type(TDesC8 *aDes8) {
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
            return (reinterpret_cast<TPtrC8*>(aDes8))->iPtr;

        case EPtr:
            return (reinterpret_cast<TPtr8*>(aDes8))->iPtr;
    }

    return eka2l1::ptr<TUint8>(nullptr);
}

TUint8 *GetTDes8Ptr(eka2l1::system *sys, TDesC8 *aDes8) {
    TDesType destype = static_cast<TDesType>(GetTDesC8Type(aDes8));

    if (destype == EBuf || destype == EBufC) {
        return reinterpret_cast<TUint8*>(aDes8) + 8;
    } else if (destype == EBufCPtr) {
        ptr<TBufC8> buf = reinterpret_cast<TBufCPtr8 *>(aDes8)->iPtr;
        TBufC8 *ptr = buf.get(sys->get_memory_system());

        return reinterpret_cast<TUint8 *>(ptr) + 8;
    }

    ptr<TUint8> des_ptr = GetTDes8HLEPtr(sys, aDes8);
    return des_ptr.get(sys->get_memory_system());
}

TInt GetTDesC16Type(TDesC16 *aDes16) {
    return GetTDesC8Type(reinterpret_cast<TDesC8*>(aDes16));
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
        return (reinterpret_cast<TPtrC16*>(aDes16))->iPtr;

    case EPtr:
        return (reinterpret_cast<TPtr16*>(aDes16))->iPtr;
    }

    return ptr<TUint16>(nullptr);
}

TUint16 *GetTDes16Ptr(eka2l1::system *sys, TDesC16 *aDes16) {
    TDesType destype = static_cast<TDesType>(GetTDesC16Type(aDes16));

    if (destype == EBuf || destype == EBufC) {
        return reinterpret_cast<TUint16*>(reinterpret_cast<TUint8*>(aDes16) + 8);
    } else if (destype == EBufCPtr) {
        ptr<TBufC16> buf = reinterpret_cast<TBufCPtr16 *>(aDes16)->iPtr;
        TBufC16 *ptr = buf.get(sys->get_memory_system());

        return reinterpret_cast<TUint16*>(reinterpret_cast<TUint8 *>(ptr) + 8);
    }

    ptr<TUint16> des_ptr = GetTDes16HLEPtr(sys, aDes16);
    return des_ptr.get(sys->get_memory_system());
}

TUint8 *GetLit8Ptr(memory_system *mem, eka2l1::ptr<TLit8> aLit) {
    return reinterpret_cast<TUint8*>(aLit.cast<TUint8>().get(mem) + 4);
}

TUint16 *GetLit16Ptr(memory_system *mem, eka2l1::ptr<TLit16> aLit) {
    return reinterpret_cast<TUint16*>(GetLit8Ptr(mem, aLit.cast<TLit8>()));
}

void SetLengthDes(TDesC8 *des, uint32_t len) {
    auto t = des->iLength >> 28;
    des->iLength = len | (t << 28);
}

void SetLengthDes(TDesC16 *des, uint32_t len) {
    SetLengthDes(reinterpret_cast<TDesC8 *>(des), len);
}

uint32_t ExtractDesLength(uint32_t len) {
    return len & 0xFFFFFF;
}
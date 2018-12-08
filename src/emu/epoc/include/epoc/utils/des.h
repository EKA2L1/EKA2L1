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

#include <common/e32inc.h>

#include <e32err.h>
#include <epoc/ptr.h>

#include <string>
#include <memory>

namespace eka2l1 {
    class system;

    namespace kernel {
        class process;
    }

    using process_ptr = std::shared_ptr<kernel::process>;
}

/*! \brief Epoc namespace is the namespace for all morden C++ implementation of 
 *         class/function in Symbian, for EKA1, EKA2 (both ^1, ^3) 
 * 
 * These can help services implementation or executive calls implementation.
 */
namespace eka2l1::epoc {
    const TInt KShiftDes8Type = 28;
    const TInt KShiftDes16Type = 28;

    /*! \brief Descriptor type */
    enum TDesType {
        //! Constant buffer.
        /*! 
            Contains length and a stack array of element. 
        */
        EBufC,

        //! Constant Pointer Descriptor.
        /* Contains length and pointer to the char array.
        */
        EPtrC,

        //! Pointer Descriptor, can grow and expand.
        /* Contains length, max length and pointer to expandable char array.
        */
        EPtr,

        //! Buffer descriptor.
        /* Contains length, max length and a stack array of element that size 
         * of maximum length.
        */
        EBuf,

        //! Buffer Constant Pointer.
        /* Contains length, max length and pointer to a Constant Buffer Descriptor.
        */
        EBufCPtr
    };

    /*! \brief A constant 8-bit descriptor. 
     *
     * Descriptor are Symbian's vector and string. 
     * They can both contains binary data and also used for
     * string storage.
     */
    struct TDesC8 {
        //! Length of the descriptor mask with the type.
        /*! The length and the type of descriptor are combined by 
          this formula: iLength = DescriptorLength | ((int)(DescriptorType) << 28)
        */
        TUint iLength;

        /*! \brief Get the pointer to the data of descriptor.
         * \param sys The system pointer.
         * \returns Pointer to the data descriptor.
        */
        TUint8 *Ptr(eka2l1::system *sys);

        TUint8 *Ptr(eka2l1::process_ptr pr);

        /*! \brief Get the length of the descriptor.
         * \returns The descriptor length.
        */
        TUint Length() const;

        /*! \brief Get the descriptor type. 
         *
         * \returns Descriptor type.
        */
        TDesType Type() const;

        /*! \brief Compare two descriptor. 
         * \returns True if equal, false if not.
         */
        bool Compare(eka2l1::system *sys, TDesC8 &aRhs);

        /*! \brief Turns the descriptor into an 8-bit string. 
         * \returns The expected string.
         */
        std::string StdString(eka2l1::system *sys);

        std::string StdString(eka2l1::process_ptr pr);

        /*! \brief Set the length of the descriptor.
         * \param iNewLength The new length to be set.
         */
        void SetLength(eka2l1::system *sys, TUint32 iNewLength);

        void SetLength(eka2l1::process_ptr pr, TUint32 iNewLength);

        /*! \brief Assign the descriptor with the provided string. 
         * \param iNewString The string to assign.
         */
        void Assign(eka2l1::system *sys, std::string iNewString);

        void Assign(eka2l1::process_ptr pr, std::string iNewString);
    };

    /*! \brief A constant 16-bit descriptor. 
     *
     * Descriptor are Symbian's vector and string. 
     * They can both contains binary data and also used for
     * string storage.
     */
    struct TDesC16 {
        //! Length of the descriptor mask with the type.
        /*! The length and the type of descriptor are combined by 
          this formula: iLength = DescriptorLength | ((int)(DescriptorType) << 28)
        */
        TUint iLength;

        /*! \brief Get the pointer to the data of descriptor.
         * \param sys The system pointer.
         * \returns Pointer to the data descriptor.
        */
        TUint16 *Ptr(eka2l1::system *sys);

        TUint16 *Ptr(eka2l1::process_ptr pr);

        /*! \brief Get the length of the descriptor.
         * \returns The descriptor length.
        */
        TUint Length() const;

        /*! \brief Get the descriptor type. 
         *
         * \returns Descriptor type.
        */
        TDesType Type() const;

        /*! \brief Compare two descriptor. 
         * \returns True if equal, false if not.
         */
        bool Compare(eka2l1::system *sys, TDesC16 &aRhs);

        /*! \brief Turns the descriptor into an 16-bit string. 
         * \returns The expected string.
         */
        std::u16string StdString(eka2l1::system *sys);

        std::u16string StdString(eka2l1::process_ptr pr);

        /*! \brief Set the length of the descriptor.
         * \param iNewLength The new length to be set.
         */
        void SetLength(eka2l1::system *sys, TUint32 iNewLength);

        void SetLength(eka2l1::process_ptr pr, TUint32 iNewLength);

        /*! \brief Assign the descriptor with the provided string. 
         * \param iNewString The string to assign.
         */
        void Assign(eka2l1::system *sys, std::u16string iNewString);

        void Assign(eka2l1::process_ptr pr, std::u16string iNewString);
    };

    /*! \brief Modifiable 8-bit descriptor */
    struct TDes8 : public TDesC8 {
        TInt iMaxLength;
    };

    /*! \brief Modifiable 16-bit descriptor */
    struct TDes16 : public TDesC16 {
        TInt iMaxLength;
    };

    /*! \brief 8-bit descriptor with data pointer */
    struct TPtrC8 : public TDesC8 {
        ptr<TUint8> iPtr;
    };

    /*! \brief 8-bit modifiable and expandable descriptor with
         data pointer */
    struct TPtr8 : public TDes8 {
        ptr<TUint8> iPtr;
    };

    /*! \brief 16-bit descriptor with data pointer */
    struct TPtrC16 : public TDesC16 {
        ptr<TUint16> iPtr;
    };

    /*! \brief 16-bit modifiable and expandable descriptor with
         data pointer */
    struct TPtr16 : public TDes16 {
        ptr<TUint16> iPtr;
    };

    struct TBufCBase8 : public TDesC8 {};
    struct TBufCBase16 : public TDesC16 {};

    /*! \brief 8-bit descriptor which data array resides on stack. */
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

    /*! \brief 16-bit expandable and modifiable descriptor which data array resides on stack. */
    struct TBuf16 {
        TInt iLength;
        TInt iMaxLength;
        ptr<TUint16> iBuf;
    };

    /*! \brief 8-bit expandable and modifiable descriptor which data array resides on stack. */
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

    /*! \brief Get the type of a 8-bit descriptor. */
    TInt GetTDesC8Type(const TDesC8 *aDes8);
    ptr<TUint8> GetTDes8HLEPtr(eka2l1::process_ptr sys, TDesC8 *aDes8);
    ptr<TUint8> GetTDes8HLEPtr(eka2l1::process_ptr sys, ptr<TDesC8> aDes8);
    TUint8 *GetTDes8Ptr(eka2l1::process_ptr sys, TDesC8 *aDes8);

    /*! \brief Get the type of a 16-bit descriptor. */
    TInt GetTDesC16Type(const TDesC16 *aDes16);
    ptr<TUint16> GetTDes16HLEPtr(eka2l1::process_ptr sys, TDesC16 *aDes16);
    ptr<TUint16> GetTDes16HLEPtr(eka2l1::process_ptr sys, ptr<TDesC16> aDes16);
    TUint16 *GetTDes16Ptr(eka2l1::process_ptr sys, TDesC16 *aDes16);

    /*! \brief A 8-bit literal 
     *
     * Symbian literal are written in binary with first 4 bytes contains 
     * the descriptor length, and length or length * 2 bytes later (depend
     * on the type), contains the literal data.
     */
    struct TLit8 {
        TUint iTypeLength;
    };

    /*! \brief A 16-bit literal 
     *
     * Symbian literal are written in binary with first 4 bytes contains 
     * the descriptor length, and length or length * 2 bytes later (depend
     * on the type), contains the literal data.
     */
    struct TLit16 : public TLit8 {};

    /*! \brief Get a pointer to the actual literal data. */
    TUint8 *GetLit8Ptr(memory_system *mem, eka2l1::ptr<TLit8> aLit);
    
    /*! \brief Get a pointer to the actual literal data. */
    TUint16 *GetLit16Ptr(memory_system *mem, eka2l1::ptr<TLit16> aLit);

    void SetLengthDes(eka2l1::process_ptr sys, TDesC8 *des, uint32_t len);
    void SetLengthDes(eka2l1::process_ptr sys, TDesC16 *des, uint32_t len);

    uint32_t ExtractDesLength(uint32_t len);

    // It doesn't really matter if it's Des8 or Des16, they have the same structure
    // So the size and offset stay the same
    uint32_t ExtractDesMaxLength(TDes8 *des);

    using TLit = TLit16;
}
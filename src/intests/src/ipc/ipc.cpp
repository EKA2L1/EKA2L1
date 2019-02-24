/*
 * ipc.cpp
 *
 *  Created on: Dec 23, 2018
 *      Author: fewds
 */

#include <intests/ext/dummycli/dummycli.h>
#include <intests/ipc/ipc.h>

#include <intests/testmanager.h>
#include <utf.h>

void IpcReadWriteDescriptorWithoutOffsetL() {
    RDummySession session;
    session.ConnectL();

    const TInt org = 50;

    TInt value = org;
    TInt result = session.GetHash(value);

    TBuf8<70> expectedLine;
    expectedLine.Format(_L8("DummyServ: GetHash in %d, out %d, expect %d result %d"), org,
        value, (org << 16) + 0x5972, result);

    EXPECT_INPUT_EQUAL_L(expectedLine);

    session.Close();
}

void IpcWriteDescriptorWithoutOffsetL() {
    RDummySession session;
    session.ConnectL();

    TBuf8<128> secertStr;

    {
        TBuf<128> whatsDeSecert;
        session.GetSecertMessage(whatsDeSecert);

        CnvUtfConverter::ConvertFromUnicodeToUtf8(secertStr, whatsDeSecert);
    }

    TBuf8<70> expectedLine;
    expectedLine.Format(_L8("DummyServ: GetSecertString out %S"), &secertStr);

    EXPECT_INPUT_EQUAL_L(expectedLine);
    session.Close();
}

void IpcWriteDescriptorWithOffsetL() {
    SSecert meSecert;
    RDummySession session;
    session.ConnectL();

    session.GetSecertStruct(meSecert);
    TBuf8<70> expectedLine;
    expectedLine.Format(_L8("DummyServ: GetSecertStruct magic %d num1 %d num2 %d"),
        meSecert.iMagic, meSecert.iA, meSecert.iB);

    EXPECT_INPUT_EQUAL_L(expectedLine);
    session.Close();
}

void IpcReadWithOffsetAndWriteWithoutOffsetL() {
    _LIT(KStringToDivide, "Sh! Divide me, if no i will asdskjwajekacjk");

    RDummySession session;
    session.ConnectL();

    {
        TBuf<4> string1;
        TBuf<128> string2;

        TInt err = session.DivideString(KStringToDivide(),
            string1, string2);

        TBuf8<256> expectedLine;

        {
            TBuf<256> expectedLine16;
            expectedLine16.Format(_L("DummyServ: Divide org %S str1 %S str2 %S"),
                &KStringToDivide, &string1, &string2);

            CnvUtfConverter::ConvertFromUnicodeToUtf8(expectedLine, expectedLine16);
        }

        EXPECT_INPUT_EQUAL_L(expectedLine);
    }

    // Now divide a new string, but make it fail because the descriptor is not long enough
    // See what error code it's return this time
    _LIT(KNewStringToDivide, "Hi! I'm the evil string!");

    TBuf<1> evilBuf1;
    TBuf<1> evilBuf2;

    TInt err = session.DivideString(KNewStringToDivide(),
        evilBuf1, evilBuf2);

    TBuf8<60> expectedLine2;
    expectedLine2.Format(_L8("Evil string divide request returns code %d"), err);

    EXPECT_INPUT_EQUAL_L(expectedLine2);

    session.Close();
}

void AddIpcTestCasesL() {
    ADD_TEST_CASE_L(ReadWriteDescriptorWithoutOffset, IPC, IpcReadWriteDescriptorWithoutOffsetL);
    ADD_TEST_CASE_L(WriteDescriptorWithoutOffset, IPC, IpcWriteDescriptorWithoutOffsetL);
    ADD_TEST_CASE_L(WriteDescriptorWithOffset, IPC, IpcWriteDescriptorWithOffsetL);
    ADD_TEST_CASE_L(ReadWithOffsetWriteDescriptorWithoutOffset, IPC, IpcReadWithOffsetAndWriteWithoutOffsetL);
}

/*
 * ipc.cpp
 *
 *  Created on: Dec 23, 2018
 *      Author: fewds
 */

#include <intests/ipc/ipc.h>
#include <intests/ext/dummycli/dummycli.h>

#include <intests/testmanager.h>
#include <utf.h>

void IpcReadWriteDescriptorWithoutOffsetL()
    {
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

void IpcWriteDescriptorWithoutOffsetL()
    {
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

void IpcWriteDescriptorWithOffsetL()
    {
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

void AddIpcTestCasesL()
    {
        ADD_TEST_CASE_L(ReadWriteDescriptorWithoutOffset, IPC, IpcReadWriteDescriptorWithoutOffsetL);
        ADD_TEST_CASE_L(WriteDescriptorWithoutOffset, IPC, IpcWriteDescriptorWithoutOffsetL);
        ADD_TEST_CASE_L(WriteDescriptorWithOffset, IPC, IpcWriteDescriptorWithOffsetL);
    }

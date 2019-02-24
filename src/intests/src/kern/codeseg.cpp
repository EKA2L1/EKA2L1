/*
 * codeseg.cpp
 *
 *  Created on: Dec 21, 2018
 *      Author: fewds
 */

#include <intests/absorber.h>
#include <intests/kern/codeseg.h>
#include <intests/testmanager.h>

#include <e32rom.h>
#include <e32std.h>

void ExceptionDescriptorRamCodeL() {
    RProcess pr;
    TModuleMemoryInfo info;

    User::LeaveIfError(pr.GetMemoryInfo(info));
    TLinAddr exceptionDescriptor = UserSvr::ExceptionDescriptor(info.iCodeBase);

    TBuf8<40> expectedLine;
    expectedLine.Format(_L8("%08X"), exceptionDescriptor - info.iCodeBase);

    EXPECT_INPUT_EQUAL_L(expectedLine);

    TExceptionDescriptor *descriptor = reinterpret_cast<TExceptionDescriptor *>(exceptionDescriptor);
    expectedLine.Format(_L8("%08X %08X %08X %08X"), descriptor->iExIdxBase - info.iCodeBase, descriptor->iExIdxLimit - info.iCodeBase,
        descriptor->iROSegmentBase - info.iCodeBase, descriptor->iROSegmentLimit - info.iCodeBase);

    EXPECT_INPUT_EQUAL_L(expectedLine);
}

void AddCodeSegTestCasesL() {
    ADD_TEST_CASE_L(ExceptionDescriptorForRamCode, CodeSeg, ExceptionDescriptorRamCodeL);
}

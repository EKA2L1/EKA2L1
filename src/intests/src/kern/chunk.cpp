/*
 * chunk.cpp
 *
 *  Created on: Jun 15, 2019
 *      Author: fewds
 */

#include <intests/absorber.h>
#include <intests/testmanager.h>

#include <e32std.h>

void CodeChunkExecutionL() {
    RChunk codeChunk;
    codeChunk.CreateLocalCode(0x1000, 0x1000, EOwnerProcess);

    TUint32 *dat = reinterpret_cast<TUint32 *>(codeChunk.Base());

    // Put ARM code in (32 - bit)
    // This function add two numbers with each other, then shift left the result with 4 (or multiply the result with 16).
    // The return value is in R0
    dat[0] = 0xE0800001; // ADD R0, R0, R1
    dat[1] = 0xE1A00200; // LSL R0, R0, #4
    dat[2] = 0xE12FFF1E; // BX LR

    // Invalidate cache range
    User::IMB_Range(dat, dat + 3);

    typedef TUint32 (*TestFunc)(TUint32 num1, TUint32 num2);

    union TestFuncWrapper {
        TUint32 *dat;
        TestFunc func;
    } func_wrapper;

    func_wrapper.dat = dat;
    const TUint32 result = func_wrapper.func(2, 8);

    TBuf8<40> text;
    text.Format(_L8("JIT code expected result: %d"), result);
    EXPECT_INPUT_EQUAL_L(text);

    codeChunk.Close();
}

void AddKernChunkTestCasesL() {
    ADD_TEST_CASE_L(ChunkCodeExecution, Chunk, CodeChunkExecutionL);
}

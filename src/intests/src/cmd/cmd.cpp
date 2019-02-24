/*
 * cmd.cpp
 *
 *  Created on: Dec 21, 2018
 *      Author: fewds
 */

#include <intests/absorber.h>
#include <intests/cmd/cmd.h>
#include <intests/testmanager.h>

#include <e32base.h>
#include <e32std.h>

#include <utf.h>

void CheckCommandLengthL() {
    TInt cmdLength = User::CommandLineLength();
    User::LeaveIfError(cmdLength);

    TBuf8<60> expectLine;
    expectLine.Format(_L8("Command Line length: %d"), cmdLength);

    EXPECT_INPUT_EQUAL_L(expectLine);
}

void CheckCommandLine() {
    TBuf<128> cmdLine;
    User::CommandLine(cmdLine);

    TBuf8<128> cmdLineUtf8;
    CnvUtfConverter::ConvertFromUnicodeToUtf8(cmdLineUtf8, cmdLine);

    TBuf8<160> expectLine;
    expectLine.Format(_L8("Command Line string: %S"), &cmdLineUtf8);

    EXPECT_INPUT_EQUAL_L(expectLine);
}

void AddCmdTestCaseL() {
    ADD_TEST_CASE_L(CheckCommandLineLength, CmdAndParams, CheckCommandLengthL);
    ADD_TEST_CASE_L(CheckCommandLineString, CmdAndParams, CheckCommandLine);
}

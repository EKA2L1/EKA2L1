/*
 * file.cpp
 *
 *  Created on: Dec 19, 2018
 *      Author: fewds
 */

#include <absorber.h>
#include <file.h>
#include <testmanager.h>

void DifferentFileOpenResultL()
    {
        RFs fsSession;
        User::LeaveIfError(fsSession.Connect(-1));
        
        fsSession.Close();
    }

void AddFileTestCasesL()
    {
        ADD_TEST_CASE_L("DifferentFileOpenResult", "FileIO", DifferentFileOpenResultL);
    }

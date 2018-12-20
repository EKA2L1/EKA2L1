/*
 * file.cpp
 *
 *  Created on: Dec 19, 2018
 *      Author: fewds
 */

#include <absorber.h>
#include <file.h>
#include <testmanager.h>

struct SFsSessionGuard
	{
		RFs iFs;
		
		SFsSessionGuard()
			{
				iFs.Connect(-1);
				iFs.SetSessionToPrivate(EDriveC);
			}
		
		~SFsSessionGuard()
			{
				iFs.Close();
			}
	};

_LIT(KAssetsFolder, "FileIO\\Assets\\");

void SeekPosAfterCustomReadL()
    {
		SFsSessionGuard guard;
		RFile f;
		
		TFileName fileName;
		guard.iFs.SessionPath(fileName);
		
		fileName.Append(KAssetsFolder);
		fileName.Append(_L("SeekPosTest.inp"));
		
		User::LeaveIfError(f.Open(guard.iFs, fileName, EFileRead | EFileShareAny));
		
		// Opened, let's do some tests
		// Read some data first
		HBufC8 *lineBuf = HBufC8::NewL(20);
		TPtr8 line = lineBuf->Des();
		
		f.Read(line, 15);
		
		// Save the position
		TInt currentPos = 0;
		f.Seek(ESeekCurrent, currentPos);
		
		HBufC8 *expectedLineDescriptionBuf = HBufC8::NewL(45);
		TPtr8 expectedLineDescription = expectedLineDescriptionBuf->Des();
		
		expectedLineDescription.Format(_L8("RFile.Read(line, 15).Pos = %d"), currentPos);
		
		EXPECT_INPUT_EQUAL_L(expectedLineDescription);
		
		// Got the current position, it should be 14 ?
		f.Read(2, line, 7);
		
		currentPos = 0;
		f.Seek(ESeekCurrent, currentPos);
		
		expectedLineDescription.Format(_L8("RFile.Read(2, line, 7).Pos = %d"), currentPos);
		EXPECT_INPUT_EQUAL_L(expectedLineDescription);

		f.Close();
		delete expectedLineDescriptionBuf;
    }

void AddFileTestCasesL()
    {
        ADD_TEST_CASE_L(SeekPositionAfterCustomRead, FileIO, SeekPosAfterCustomReadL);
    }

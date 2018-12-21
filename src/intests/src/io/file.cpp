/*
 * file.cpp
 *
 *  Created on: Dec 19, 2018
 *      Author: fewds
 */

#include <intests/absorber.h>
#include <intests/io/file.h>
#include <intests/testmanager.h>

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

_LIT(KAssetsFolder, "Assets\\FileIO\\");

#define MAKE_ASSET_FILE_PATH(path, fn, fs)		\
        fs.SessionPath(path);				\
        path.Append(KAssetsFolder);			\
        path.Append(fn)

void PosAfterCustomReadL()
    {
		SFsSessionGuard guard;
		RFile f;
		
		TFileName fileName;
		MAKE_ASSET_FILE_PATH(fileName, _L("SeekPosTest.inp"), guard.iFs);
		
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

void CommonSeekingL()
    {
        // Hey, let's try open a ROM file
        // Assuming the most common ROM file I known: EUser.DLL, will always be available
        SFsSessionGuard guard;
        RFile romFile;
        
        User::LeaveIfError(romFile.Open(guard.iFs, _L("Z:\\sys\\bin\\EUser.DLL"), EFileShareAny | EFileRead));
        
        // Seek ROM address, are we gonna get an address?
        TInt seekAddr = 2;
        romFile.Seek(ESeekAddress, seekAddr);
        
        TBuf8<260> expectedLine;
        expectedLine.Format(_L8("Seek EUSER.DLL ROM FILE with offset 2, mode address, return = %08X"), seekAddr);
        
        EXPECT_INPUT_EQUAL_L(expectedLine);
        
        seekAddr = 50;
        romFile.Seek(ESeekStart, seekAddr);
        
        expectedLine.Format(_L8("Seek EUSER.DLL ROM FILE with offset 50, mode start, return = %08X"), seekAddr);
        
        EXPECT_INPUT_EQUAL_L(expectedLine);
        
        // Let's seek backwards
        seekAddr = -2;
        romFile.Seek(ESeekCurrent, seekAddr);
        
        // Should be 48
        expectedLine.Format(_L8("Seek EUSER.DLL ROM FILE with offset -2, mode current, return = %08X"), seekAddr);
        EXPECT_INPUT_EQUAL_L(expectedLine);
        
        // Let's seek end
        seekAddr = 0;
        romFile.Seek(ESeekEnd, seekAddr);

        expectedLine.Format(_L8("Seek EUSER.DLL ROM FILE with offset 0, mode end, return = %08X"), seekAddr);
        EXPECT_INPUT_EQUAL_L(expectedLine);        
    }

void AddFileTestCasesL()
    {
        ADD_TEST_CASE_L(PositionAfterCustomRead, FileIO, PosAfterCustomReadL);
        ADD_TEST_CASE_L(CommonSeeking, FileIO, CommonSeekingL);
    }

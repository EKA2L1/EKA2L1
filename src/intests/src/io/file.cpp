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

void CommonSeekingRomL()
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

void FileReadWithSpecifiedLenAndOffsetL()
    {
        SFsSessionGuard guard;
        RFile f;
        
        TFileName fileName;
        MAKE_ASSET_FILE_PATH(fileName, _L("ReadDummy.txt"), guard.iFs);
        
        User::LeaveIfError(f.Open(guard.iFs, fileName, EFileRead | EFileShareAny));
        
        // No length set
        TBuf8<30> hitMiss;
        TInt orgLen = hitMiss.Length();
        
        // length = 11 (Hit or Miss)
        f.Read(hitMiss, 11);
        
        TBuf8<180> expectedLine;
        expectedLine.Format(_L8("Read with len = 11, org len = %d, new_len = %d, str = %S"),
                orgLen, expectedLine.Length(), &hitMiss);
        
        EXPECT_INPUT_EQUAL_L(expectedLine);
        
        // Now read with offset and specified len
        // We read a big chunk of data now
        f.Read(11, hitMiss, 5);
        
        // But it should limit our data already!
        expectedLine.Format(_L8("String from offset 11 with len 5: %S"),
                &hitMiss);
        
        EXPECT_INPUT_EQUAL_L(expectedLine);
    }

void FileReadEofLimitL()
    {
        SFsSessionGuard guard;
        RFile f;
        
        TFileName fileName;
        MAKE_ASSET_FILE_PATH(fileName, _L("ReadDummy.txt"), guard.iFs);
        
        User::LeaveIfError(f.Open(guard.iFs, fileName, EFileRead | EFileShareAny));
     
        // The behavior should be: when we are reading beyond eof, all data will be discard
        TBuf8<58> buf;
        f.Read(0, buf, 102);
        
        TInt filePointer = 0;
        f.Seek(ESeekCurrent, filePointer);
        
        TBuf8<120> expectedLine;
        expectedLine.Format(_L8("Read offset 0 with size 102 (beyond eof and buf size), data %S pointer %d"), &buf, filePointer);

        EXPECT_INPUT_EQUAL_L(expectedLine);

        TBuf8<58> buf2;
        f.Read(0, buf2, 25);
        
        f.Seek(ESeekCurrent, filePointer);
                
        expectedLine.Format(_L8("Read offset 0 with size 25, data %S pointer %d"), &buf2, filePointer);
        EXPECT_INPUT_EQUAL_L(expectedLine);
        
        f.Close();        
    }

void FileWriteWithSpecifiedLenAndOffsetL()
    {
        SFsSessionGuard guard;
        RFile f;
        
        TFileName fileName;
        MAKE_ASSET_FILE_PATH(fileName, _L("WriteDummy.txt"), guard.iFs);
        
        User::LeaveIfError(f.Replace(guard.iFs, fileName, EFileWrite | EFileShareAny));
        
        _LIT8(KStringToWrite, "Hey, I'm Dummy!");
        f.Write(5, KStringToWrite, 3);
        
        f.Close();
        
        User::LeaveIfError(f.Open(guard.iFs, fileName, EFileRead | EFileShareAny));
        
        TBuf8<6> buf;
        f.Read(buf, 3);
        
        TBuf8<40> expectedLine;
        expectedLine.Format(_L8("Writing to file, results: %S"), &buf);
        
        EXPECT_INPUT_EQUAL_L(expectedLine);
    }

void AddFileTestCasesL()
    {
        ADD_TEST_CASE_L(PositionAfterCustomRead, FileIO, PosAfterCustomReadL);
        ADD_TEST_CASE_L(CommonSeekingRom, FileIO, CommonSeekingRomL);
        ADD_TEST_CASE_L(ReadWithSpecifiedLenAndOffset, FileIO, FileReadWithSpecifiedLenAndOffsetL);
        ADD_TEST_CASE_L(ReadTillEof, FileIO, FileReadEofLimitL);
        ADD_TEST_CASE_L(FileWriteWithLengthAndOffset, FileIO, FileWriteWithSpecifiedLenAndOffsetL);
    }

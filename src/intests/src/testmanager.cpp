/*
 * testmanager.cpp
 *
 *  Created on: Dec 19, 2018
 *      Author: fewds
 */

#include <intests/fate.h>
#include <intests/testmanager.h>

#include <e32base.h>
#include <e32debug.h>

CTestManager *instance;

CTestManager *CTestManager::NewLC(const TAbsorberMode aAbsorbMode) {
    CTestManager *self = new (ELeave) CTestManager;
    CleanupStack::PushL(self);
    self->ConstructL(aAbsorbMode);

    return self;
}

CTestManager *CTestManager::NewL(const TAbsorberMode aAbsorbMode) {
    CTestManager *self = NewLC(aAbsorbMode);
    CleanupStack::Pop();

    return self;
}

TInt CTestManager::Run() {
    TInt successfulTests = 0;
    RFs &fs = iAbsorber->GetFsSession();

    for (iCurrentTest = 0; iCurrentTest < iTests.Count(); iCurrentTest++) {
        TTest &test = iTests[iCurrentTest];

        _LIT(KTestExt, ".expected");
        _LIT(KTestFolder, "expected\\");
        
        HBufC *testPathBuf;
        TRAPD(err, testPathBuf = HBufC::NewL(test.iCategory->Length() + test.iName->Length() + KTestExt.iTypeLength + 2 + iSessionPath.Length() + KTestFolder.iTypeLength));

        if (err != KErrNone) {
            return err;
        }

        {
            TPtr testPath = testPathBuf->Des();

            testPath.Append(iSessionPath);
            testPath.Append(KTestFolder);

            testPath.Append(test.iCategory->Des());
            testPath.Append('\\');
            testPath.Append(test.iName->Des());
            testPath.Append(KTestExt);

            fs.MkDirAll(testPath);

            TRAPD(ferr, iAbsorber->NewAbsorbSessionL(testPath));

            if (ferr != KErrNone) {
                return 0;
            }
        }

        delete testPathBuf;

        TPtr category = iTests[iCurrentTest].iCategory->Des();
        TPtr name = iTests[iCurrentTest].iName->Des();

        RDebug::Print(_L("[%S] %S running"), &category, &name);

        err = KErrNone;

        TRAP(err, iTests[iCurrentTest].iTestFunc());

        if (err != KErrNone) {
            // Something happens, log to debug
            RDebug::Print(_L("[%S] %S: Test failed with leave code %d"), &category, &name, err);
        } else {
            RDebug::Print(_L("[%S] %S passed"), &category, &name);

            successfulTests++;
        }
    }

    return successfulTests;
}

TInt CTestManager::TotalTests() {
    return iTests.Count();
}

void CTestManager::AddTestL(const TDesC &aName, const TDesC &aCategory, TTestFunc aFunc) {
    TTest test;
    test.iTestFunc = aFunc;
    test.iName = HBufC::NewL(aName.Length());
    test.iCategory = HBufC::NewL(aCategory.Length());

    *test.iName = aName;
    *test.iCategory = aCategory;

    iTests.AppendL(test);
}

CTestManager::~CTestManager() {
    for (TInt i = 0; i < iTests.Count(); i++) {
        delete iTests[i].iName;
        delete iTests[i].iCategory;
    }

    iTests.Close();
}

void CTestManager::ConstructL(const TAbsorberMode aAbsorbMode) {
    iAbsorber = CAbsorber::NewL(aAbsorbMode);
    RFs &fs = iAbsorber->GetFsSession();

#if GEN_TESTS
    // Search for a memory card drive. Would be best to store on it
    for (TInt drv = EDriveA; drv <= EDriveZ; drv++) {
        TDriveInfo drvInfo;
        if (fs.Drive(drvInfo, drv) == KErrNone) {
            if (drvInfo.iDriveAtt & KDriveAttRemovable) {
                TChar c;
                fs.DriveToChar(drv, c);

                iSessionPath.Append(c);
                iSessionPath.Append(_L(":\\"));

                break;
            }
        }
    }

    if (iSessionPath.Length() == 0) {
        iSessionPath = RProcess().FileName().Left(3);
    }
#else
    fs.SessionPath(iSessionPath);
#endif
}

void CTestManager::ExpectInputFileEqualL(const TDesC8 &aData) {
    iAbsorber->ExpectEqualL(aData);
}

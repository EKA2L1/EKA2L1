/*
 * testmanager.cpp
 *
 *  Created on: Dec 19, 2018
 *      Author: fewds
 */

#include <testmanager.h>
#include <e32debug.h>

CTestManager *instance;

TInt CTestManager::Run()
    {
        TInt successfulTests = 0;
    
        for (TInt i = 0; i < iTests.Count(); i++)
            {
                TRAPD(err, iTests[i].iTestFunc());
                
                if (err != KErrNone)
                    {
                        // Something happens, log to debug
                        RDebug::Printf("[%s] %s: Test failed with leave code %d", &iTests[i].iCategory, 
                                &iTests[i].iName, err);
                    }
                else
                    {
                        successfulTests++;
                    }
            }
        
        return successfulTests;
    }

TInt CTestManager::TotalTests()
    {
        return iTests.Count();
    }

void CTestManager::AddTestL(const TDesC8 &aName, const TDesC8 &aCategory, TTestFunc aFunc)
    {
        TTest test;
        test.iTestFunc = aFunc;
        test.iName = HBufC8::NewL(aName.Length());
        test.iCategory = HBufC8::NewL(aCategory.Length());
        
        *test.iName = aName;
        *test.iCategory = aCategory;
        
        iTests.AppendL(test);
    }

CTestManager::~CTestManager()
    {
        iTests.Close();
    }

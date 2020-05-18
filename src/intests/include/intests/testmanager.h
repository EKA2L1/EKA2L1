/*
 * testmanager.h
 *
 *  Created on: Dec 19, 2018
 *      Author: fewds
 */

#ifndef TESTMANAGER_H_
#define TESTMANAGER_H_

#include <e32debug.h>
#include <e32std.h>

#include <intests/absorber.h>

typedef void (*TTestFunc)();

struct TTest {
    TTestFunc iTestFunc;
    HBufC *iName;
    HBufC *iCategory;
};

class CTestManager {
public:
    TInt GetWorkingDrive() const {
    	return iWorkingDrive;
    }

    /**
         * \brief Run all the tests
         * 
         * \returns The number of successful test
         */
    TInt Run();

    /**
         * \brief Get the number of total tests.
         * 
         */
    TInt TotalTests();

    /**
         * \brief Add new test to test manager.
         * 
         */
    void AddTestL(const TDesC &aName, const TDesC &aCategory, TTestFunc aFunc);

    static CTestManager *NewLC(const TAbsorberMode aAbsorbMode);
    static CTestManager *NewL(const TAbsorberMode aAbsorbMode);

    ~CTestManager();

    void ExpectInputFileEqualL(const TDesC8 &aData);


private:
    void ConstructL(const TAbsorberMode aAbsorbMode);

    RArray<TTest> iTests;
    TInt iCurrentTest;
    TFileName iSessionPath;
    TInt iWorkingDrive;

    CAbsorber *iAbsorber;
};

#define ADD_TEST_CASE_L(name, category, func) \
    instance->AddTestL(_L(#name), _L(#category), func)

#define EXPECT_INPUT_EQUAL_L(data) \
    instance->ExpectInputFileEqualL(data)

extern CTestManager *instance;

#endif

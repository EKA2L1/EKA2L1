/*
 * testmanager.h
 *
 *  Created on: Dec 19, 2018
 *      Author: fewds
 */

#include <e32std.h>

typedef void (*TTestFunc)();

struct TTest
    {
        TTestFunc    iTestFunc;
        HBufC8*      iName;
        HBufC8*      iCategory;
    };

class CTestManager
    {
public:
        /* !\brief Run all the tests
         * 
         * \returns The number of successful test
         */
        TInt Run();
        
        /* !\brief Get the number of total tests.
         * 
         */
        TInt TotalTests();
        
        /* !\brief Add new test to test manager.
         * 
         */
        void AddTestL(const TDesC8 &aName, const TDesC8 &aCategory, TTestFunc aFunc);
        
        ~CTestManager();
        
private:
        RArray<TTest> iTests;
    };

#define ADD_TEST_CASE_L(name, category, func)                                 \
    instance->AddTestL(_L8(#name), _L8(#category), func)

extern CTestManager *instance;

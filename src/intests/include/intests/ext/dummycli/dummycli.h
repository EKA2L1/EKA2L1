/*
 * dummycli.h
 *
 *  Created on: Dec 23, 2018
 *      Author: fewds
 */

#ifndef DUMMYCLI_H_
#define DUMMYCLI_H_

#include <e32base.h>

struct SSecert 
    {
        TInt    iMagic;
        TInt    iA;
        TInt    iB;
    };

class RDummySession: public RSessionBase
    {
public:
        void ConnectL();
        
        // Return error code
        TInt GetHash(TInt &aValue);
        
        void GetSecertMessage(TDes &aDes);
        void GetSecertStruct(SSecert &aSecert);
        
        TInt DivideString(const TDesC &aDes, TDes &str1, TDes &str2);
    };

#endif /* DUMMYCLI_H_ */

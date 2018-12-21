/*
 * absorber.h
 *
 *  Created on: Dec 19, 2018
 *      Author: fewds
 */

#ifndef ABSORBER_H_
#define ABSORBER_H_

#include <e32std.h>
#include <f32file.h>

/* !\brief The absorb mode.
 * 
 * This is the mode used to open the output file.
 * Depends on the mode, the absorber will does different thing
 */
enum  TAbsorberMode
    {
        EAbsorbInvalid,
        EAbsorbWrite,
        EAbsorbVerify
    };

/* !\brief The absorber.
 * 
 */
class CAbsorber
    {
public:
		CAbsorber();
	
        explicit CAbsorber(const TAbsorberMode aMode);
        ~CAbsorber();
        
        TBool AbsorbL(const TDesC8 &aData);
        void ExpectEqualL(const TDesC8 &aData);
        
        static CAbsorber *NewL(const TAbsorberMode aMode);
        static CAbsorber *NewLC(const TAbsorberMode aMode);
        
        void NewAbsorbSessionL(const TDesC16 &aTestPath);
        
        TInt TryCloseAbsorbSession();
        void CloseAbsorbSessionL();
        
        RFs &GetFsSession()
        	{
        		return iFs;
        	}
        
private:
        void ConstructL();
        
        TAbsorberMode iAbsorbMode;
        
        TUint		  iOpenMode;
        
        RFs			  iFs;
        RFile         iFile;
        
        TBool 		  iFirst;
    };

#endif /* ABSORBER_H_ */

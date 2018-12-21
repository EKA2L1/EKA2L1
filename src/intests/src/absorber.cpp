/*
 * absorber.cpp
 *
 *  Created on: Dec 19, 2018
 *      Author: fewds
 */

#include <e32base.h>

#include <intests/absorber.h>

CAbsorber::CAbsorber()
	{
			
	}

CAbsorber::CAbsorber(const TAbsorberMode aMode)
    : iAbsorbMode(aMode), iFirst(ETrue)
    {
    }
   
CAbsorber::~CAbsorber() 
    {
		TryCloseAbsorbSession();
		
		iFs.Close();
    }

void CAbsorber::NewAbsorbSessionL(const TDesC16 &aTestPath)
	{
		if (iFirst == true) 
			{
				iFirst = EFalse; 
			} else {
				CloseAbsorbSessionL();
			}
		
		if (iAbsorbMode == EAbsorbVerify)
			{
				User::LeaveIfError(iFile.Open(iFs, aTestPath, iOpenMode));
				return;
			}
		
        User::LeaveIfError(iFile.Replace(iFs, aTestPath, iOpenMode));
	}

void CAbsorber::ConstructL() 
    {
        User::LeaveIfError(iFs.Connect(-1));
        User::LeaveIfError(iFs.SetSessionToPrivate(EDriveC));
        
        iOpenMode = EFileStreamText;
        
        switch (iAbsorbMode)
            {
            case EAbsorbWrite:
                {
                    iOpenMode |= (EFileWrite | EFileShareAny);
                    break;
                }
                
            case EAbsorbVerify:
                {
					iOpenMode |= (EFileShareAny | EFileRead);
                    break;
                }
               
            default:
                {
                    User::Leave(KErrNotSupported);
                }
            }
    }

TInt CAbsorber::TryCloseAbsorbSession()
	{
		if (iAbsorbMode == EAbsorbWrite)
			{
				TInt flushResult = iFile.Flush();
				
				if (flushResult != KErrNone)
					{
						return flushResult;
					}
			}
		
		iFile.Close();
		return KErrNone;
	}

void CAbsorber::CloseAbsorbSessionL()
	{
		User::LeaveIfError(TryCloseAbsorbSession());
	}

CAbsorber *CAbsorber::NewLC(const TAbsorberMode aMode)
    {
        CAbsorber *self = new (ELeave) CAbsorber(aMode);
        CleanupStack::PushL(self);
        self->ConstructL();
        
        return self;
    }

CAbsorber *CAbsorber::NewL(const TAbsorberMode aMode)
    {
        CAbsorber *self = NewLC(aMode);
        CleanupStack::Pop();
        
        return self;
    }

void CAbsorber::ExpectEqualL(const TDesC8 &aData)
    {
        if (!AbsorbL(aData))
            {
                // Hey, this is not what we expected
                User::Leave(KErrArgument);
            }
    }

TBool CAbsorber::AbsorbL(const TDesC8 &aData)
    {
        _LIT8(KNewLine, "\n");
    
        switch (iAbsorbMode)
            {
            case EAbsorbWrite:
                {
                    iFile.Write(aData);
                    
                    // Write the end line
                    iFile.Write(KNewLine);
                    
                    return true;
                }
                
            case EAbsorbVerify:
                {
                    TUint8 append;
                    TPtr8 appendWrap(&append, 1);
                    
                    TInt count = 0;
                    TInt crrPos = 0;
                    
                    iFile.Seek(ESeekCurrent, crrPos);
                    
                    do
                        {
                            TInt err = iFile.Read(appendWrap, 1);
                            
                            // No bytes read
                            if (appendWrap.Length() == 0)
                            	{
                            		break;
                            	}
                            
                            User::LeaveIfError(err);
                            
                            if (append != '\n')
                                {
                            		count++;
                                }
                        }
                    while (append != '\n');

                    HBufC8 *buf = HBufC8::NewL(count);
                    
                    TPtr8 line = buf->Des();
                    
                    iFile.Read(crrPos, line, count);
    
                    // Also read the \n
                    iFile.Read(appendWrap, 1);
                    
                    TInt compareRes = aData.Compare(line);

                    if (compareRes != 0)
                    	{
                    		RDebug::Printf("Expected \"%S\", got \"%S\"", &line, &aData);

                            delete buf;
                            return false;
                    	}
                
                    delete buf;
                    return true;
                }
                
            default:
                {
                    User::Leave(KErrNotSupported);
                    break;
                }
            }
        
        return false;
    }

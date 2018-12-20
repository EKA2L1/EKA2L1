/*
 * absorber.cpp
 *
 *  Created on: Dec 19, 2018
 *      Author: fewds
 */

#include <e32base.h>

#include <absorber.h>

CAbsorber::CAbsorber(const TAbsorberMode aMode)
    : iAbsorbMode(aMode)
    {
    }
   
CAbsorber::~CAbsorber() 
    {
        // Flush the file
        iFile.Flush();
        iFile.Close();
    }

void CAbsorber::ConstructL(const TDesC16 &aTestPath) 
    {
        RFs fsSession;
        User::LeaveIfError(fsSession.Connect(-1));
        
        TUint openMode = EFileStreamText;
        
        switch (iAbsorbMode)
            {
            case EAbsorbWrite:
                {
                    openMode |= (EFileWrite | EFileShareAny);
                    break;
                }
                
            case EAbsorbVerify:
                {
                    openMode |= (EFileShareReadersOnly | EFileRead);
                    break;
                }
               
            default:
                {
                    User::Leave(KErrNotSupported);
                }
            }
        
        User::LeaveIfError(iFile.Replace(fsSession, aTestPath, openMode));
        fsSession.Close();
    }

CAbsorber *CAbsorber::NewLC(const TDesC16 &aTestPath, const TAbsorberMode aMode)
    {
        CAbsorber *self = new (ELeave) CAbsorber(aMode);
        CleanupStack::PushL(self);
        self->ConstructL(aTestPath);
        
        return self;
    }

CAbsorber *CAbsorber::NewL(const TDesC16 &aTestPath, const TAbsorberMode aMode)
    {
        CAbsorber *self = NewLC(aTestPath, aMode);
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
                    
                    HBufC8 *buf = HBufC8::NewL(2);
                    CleanupStack::PushL(buf);
                    
                    TPtr8 line = buf->Des();
                    
                    do
                        {
                            TInt err = iFile.Read(appendWrap);
                            User::LeaveIfError(err);
                            
                            if (append != '\n')
                                {
                                    line.Append(append);
                                }
                        }
                    while (append != '\n');
                    
                    // Pop the buffer
                    CleanupStack::Pop();
                    
                    TBool eres = aData.Compare(line);
                    
                    if (!eres)
                        {
                            RDebug::Printf("Expect %s, got %s", &aData, &line);
                        }
                    
                    return eres;
                }
                
            default:
                {
                    User::Leave(KErrNotSupported);
                    break;
                }
            }
        
        return false;
    }

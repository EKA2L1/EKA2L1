/*
 * dummyservession.h
 *
 *  Created on: Dec 23, 2018
 *      Author: fewds
 */

#ifndef DUMMYSERVESSION_H_
#define DUMMYSERVESSION_H_

#include <e32base.h>
#include <intests/ext/dummyserv/dummycmn.h>

class CDummyServer;

class CDummyServerSession: public CSession2
    {
public:
        static CDummyServerSession *NewL(CDummyServer &aServer);
        static CDummyServerSession *NewLC(CDummyServer &aServer);
        
        virtual ~CDummyServerSession() 
            {
            }
        
        void ServiceL(const RMessage2 &aMessage);
      
private:
        CDummyServerSession(CDummyServer &aServer);
        void ConstructL();
       
        CDummyServer &iServer;
    };

#endif /* DUMMYSERVESSION_H_ */

/*
 * dummyserv.h
 *
 *  Created on: Dec 23, 2018
 *      Author: fewds
 */

#ifndef DUMMYSERV_H_
#define DUMMYSERV_H_

#include <e32base.h>
#include <intests/ext/dummyserv/dummycmn.h>

class CDummyServer : public CServer2 {
public:
    static CDummyServer *NewL();
    static CDummyServer *NewLC();

private:
    CDummyServer(TInt aPriority);
    void ConstructL();

    CSession2 *NewSessionL(const TVersion &aVersion, const RMessage2 &aMsg) const;
};

#endif /* DUMMYSERV_H_ */

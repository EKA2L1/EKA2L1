/*
 * dummycmn.h
 *
 *  Created on: Dec 23, 2018
 *      Author: fewds
 */

#ifndef DUMMYCMN_H_
#define DUMMYCMN_H_

#include <e32base.h>
#include <e32cmn.h>
#include <e32std.h>

_LIT(KDummyServName, "DeDummyServ");

enum TDummyOpCode
    {
        EDummyOpGetSecertString = 0x1000,
        EDummyOpGetSecertPackage = 0x2000,
        EDummyOpHashFromNumber = 0x2500,
        EDummyOpDivideString = 0x3000
    };

#endif /* DUMMYCMN_H_ */

/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef EKA2
#include <e32debug.h>
#else
#include "debug.h"
#endif

#include <f32file.h>
#include <e32std.h>
#include "log.h"

class TDesOverflowHandler : public TDes16Overflow {
    virtual void Overflow(TDes16 &) {
        User::Panic(_L("PCOMMON-HLE"), 0);
    }
};

void LogOut(const TDesC &aCategory, const TDesC &aMessage, ...) {
    HBufC *newString = HBufC::NewL(aMessage.Length() * 2);
    
#ifndef EKA2
    HBufC *newStringFinal = HBufC::NewL(aMessage.Length() * 3);
#endif

    VA_LIST list;
    VA_START(list, aMessage);

    TDesOverflowHandler handler;

    TPtr stringDes = newString->Des();
    stringDes.AppendFormatList(aMessage, list, &handler);

#ifdef EKA2
    RDebug::Print(_L("[%S] %S"), &aCategory, &stringDes);
#else
    TPtr stringDesFinal = newStringFinal->Des();
    stringDesFinal.AppendFormat(_L("[%S] %S"), &handler, &aCategory, &stringDes);

    DebugPrint16(stringDesFinal);
#endif

    User::Free(newString);
    
#ifndef EKA2
    User::Free(newStringFinal);
#endif
    
    VA_END(list);
}

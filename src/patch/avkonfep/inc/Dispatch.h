/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#ifndef AVKONFEP_DISPATCH_H_
#define AVKONFEP_DISPATCH_H_

#include <e32std.h>

#define HLE_DISPATCH_FUNC(ret, name, ARGS...) \
    ret name(const TUint32 func_id, ##ARGS)

extern "C" {
// On beginning, contains the initial text. On finish with request status done, return the committed text
// The text will be clamped to max length of descriptor
HLE_DISPATCH_FUNC(TInt, EHUIOpenGlobalTextView, const TDesC *aInitialText, const TInt aMaxLength, TRequestStatus *aStatus);
HLE_DISPATCH_FUNC(void, EHUIGetStoredText, TInt *aLength, const void *aPtr);
HLE_DISPATCH_FUNC(void, EHUICancelGlobalTextView);
}

#endif

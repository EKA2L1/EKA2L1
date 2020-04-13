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

#define IMPORT_C __declspec(dllexport)

#include <cdsb.h>
#include <scdv/log.h>
#include <scdv/panic.h>

CDirectScreenBitmap *CDirectScreenBitmap::NewL() {
    return CDirectScreenBitmap::NewL(0);
}

CDirectScreenBitmap *CDirectScreenBitmap::NewL(const TInt aScreenNum) {
    Scdv::Log("ERR:: Unsupported creating direct screen bitmap, TODO!");
    Scdv::Panic(Scdv::EPanicUnsupported);
    return NULL;
}

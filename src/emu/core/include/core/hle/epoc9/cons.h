// EKA2l1 project
// Copyright (C) 2018 EKA2l1 team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <epoc9/base.h>

namespace eka2l1 {
    namespace hle {
        namespace epoc9 {
            struct CConsoleBase : public CBase {
            };

            struct CColorConsoleBase : public CConsoleBase {
            };
        }
    }
}
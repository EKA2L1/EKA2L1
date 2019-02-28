/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <common/time.h>
#include <epoc/services/window/common.h>

namespace eka2l1::epoc {
    // TODO: Use emulated time
    event::event(const std::uint32_t handle, event_code evt_code)
        : handle(handle)
        , type(evt_code)
        , time(common::get_current_time_in_microseconds_since_1ad()) {
    }
    
    TKeyCode map_scancode_to_keycode(TStdScanCode scan_code) {
        switch (scan_code) {
        case EStdKeyNull: {
            return EKeyNull;
        }

        case EStdKeyBackspace: {
            return EKeyBackspace;
        }

        case EStdKeyTab: {
            return EKeyTab;
        }

        case EStdKeyEnter: {
            return EKeyEnter;
        }

        case EStdKeyYes: {
            return EKeyYes;
        }

        case EStdKeyNo: {
            return EKeyNo;
        }

        // Is this a laptop??? :)
        case EStdKeyIncBrightness: {
            return EKeyIncBrightness;
        }

        case EStdKeyDecBrightness: {
            return EKeyDecBrightness;
        }

        case EStdKeyIncVolume: {
            return EKeyIncVolume;
        }

        case EStdKeyDecVolume: {
            return EKeyDecVolume;
        }

        default:
            break;
        }

        if (scan_code >= EStdKeyF1 && scan_code <= EStdKeyF24) {
            return static_cast<TKeyCode>(EKeyF1 + static_cast<int>(scan_code - EStdKeyF1));
        }

        if (scan_code >= EStdKeyApplication0 && scan_code <= EStdKeyApplicationF) {
            return static_cast<TKeyCode>(EKeyApplication0 + static_cast<int>(scan_code - EStdKeyApplication0));
        }

        if (scan_code >= EStdKeyApplication10 && scan_code <= EStdKeyApplication1F) {
            return static_cast<TKeyCode>(EKeyApplication10 + static_cast<int>(scan_code - EStdKeyApplication10));
        }
        
        if (scan_code >= EStdKeyApplication20 && scan_code <= EStdKeyApplication27) {
            return static_cast<TKeyCode>(EKeyApplication20 + static_cast<int>(scan_code - EStdKeyApplication20));
        }
        
        if (scan_code >= EStdKeyDevice0 && scan_code <= EStdKeyDevice0) {
            return static_cast<TKeyCode>(EKeyDevice0 + static_cast<int>(scan_code - EKeyDevice0));
        }

        if (scan_code >= EStdKeyDevice10 && scan_code <= EStdKeyDevice1F) {
            return static_cast<TKeyCode>(EKeyDevice10 + static_cast<int>(scan_code - EKeyDevice10));
        }
        
        if (scan_code >= EStdKeyDevice20 && scan_code <= EStdKeyDevice27) {
            return static_cast<TKeyCode>(EKeyDevice20 + static_cast<int>(scan_code - EKeyDevice20));
        }

        return EKeyNull;
    }
}

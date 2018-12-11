/*
 * Copyright (c) 2018 EKA2L1 Team / Nokia
 * 
 * This file is part of EKA2L1 project / Symbian Source Code
 * (see bentokun.github.com/EKA2L1).
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

#pragma once

#include <cstdint>

namespace eka2l1 {
    constexpr std::int32_t dm_category = 0x1020E406;
    constexpr std::int32_t dm_init_key = 0x1;

    /* Error codes */
    constexpr std::int32_t KErrBadHierarchyId = -263;
    constexpr std::int32_t KDmErrOutstanding = -262;
    constexpr std::int32_t KDmErrBadSequence = -261;
    constexpr std::int32_t KDmErrBadDomainSpec = -260;
    constexpr std::int32_t KDmErrBadDomainId = -256;
    constexpr std::int32_t KDmErrNotJoin = -258;

    struct TTransInfo {
        uint16_t id;
        int state;
        int err;
    };

    struct TTransFailInfo {
        uint16_t id;
        int err;
    };

    enum TDmNotifyType {
        EDmNotifyTransRequest = 0x02,
        EDmNotifyPass = 0x04,
        EDmNotifyFail = 0x08,
        EDmNotifyTransOnly = EDmNotifyPass | EDmNotifyFail,
        EDmNotifyReqNPass = EDmNotifyTransRequest | EDmNotifyPass,
        EDmNotifyReqNFail = EDmNotifyTransRequest | EDmNotifyFail,
        EDmNotifyAll = EDmNotifyTransRequest | EDmNotifyFail | EDmNotifyPass
    };

    enum TDmTraverseDirection {
        ETraverseParentFirst,
        ETraverseChildrenFirst,
        ETraverseDefault
    };

    enum TDmTranstitionFailurePolicy {
        ETransitionFailureStop,
        ETransitionFailureContinue
    };

    enum TSsmStartupSubStates {
        ESsmStartupSubStateUndefined = 0x00,
        ESsmStartupSubStateReserved1 = 0x08,
        ESsmStartupSubStateCriticalStatic = 0x10,
        ESsmStartupSubStateReserved2 = 0x18,
        ESsmStartupSubStateCriticalDynamic = 0x20,
        ESsmStartupSubStateNetworkingCritical = 0x28,
        ESsmStartupSubStateNonCritical = 0x30,
        ESsmStartupSubStateReserved3 = 0x38,
        ESsmStartupSubStateReserved4 = 0x40
    };

    enum TStartupStateIdentifier {
        EStartupStateUndefined = 0x00,
        EReservedStartUpState1 = 0x08,
        EStartupStateCriticalStatic = 0x10,
        EReservedStartUpState2 = 0x18,
        EStartupStateCriticalDynamic = 0x20,
        EStartupStateNetworkingCritical = 0x28,
        EStartupStateNonCritical = 0x30,
        EReservedStartUpState4 = 0x38,
        EReservedStartUpState5 = 0x40
    };

    enum TMemberRequests {
        EDmDomainJoin,                                  // x
        EDmStateAcknowledge,                            // x
        EDmStateRequestTransitionNotification,          // x
        EDmStateCancelTransitionNotification,           // x
        EDmStateDeferAcknowledgement,                   // x
        EDmStateCancelDeferral                          // x
    };

    enum TControllerRequests {
        EDmRequestSystemTransition,        // x
        EDmRequestDomainTransition,        // x 
        EDmCancelTransition,               // x 
        EDmHierarchyJoin,                  // x
        EDmHierarchyAdd,                   // x
        EDmGetTransitionFailureCount,      // x
        EDmGetTransitionFailures,
        EDmObserverJoin,                   // x
        EDmObserverStart,                  // x
        EDmObserverStop,                   // This was equals to cancel, weird
        EDmObserverNotify,                 // x
        EDmObserverEventCount,             // x
        EDmObserverGetEvent,
        EDmObserverCancel,                 // x
        EDmObserveredCount                 // x
    };
}
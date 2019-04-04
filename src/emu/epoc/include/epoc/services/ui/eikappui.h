/*
 * Copyright (c) 2018 EKA2L1 Team
 * Copyright (c) 1997-2009 Nokia Corporation and/or its subsidiary(-ies).
 * 
 * This file is part of EKA2L1 project
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

#include <cstdint>

// Classic UI turned into an HLE disaster
#include <epoc/services/context.h>
#include <epoc/services/server.h>

namespace eka2l1 {
    enum TEikAppUiRanges {
        EFirstUnrestrictedOpcodeInEikAppUi
    };

    enum TEikUiOpCode {
        // No Capability requirement
        EEikAppUiFirst = EFirstUnrestrictedOpcodeInEikAppUi,
        EEikAppUiLaunchTaskList,
        EEikAppUiCycleTasks,
        EEikAppUiSetStatusPaneFlags,
        EEikAppUiSetStatusPaneLayout,
        EEikAppUiBlankScreen,
        EEikAppUiUnblankScreen,
        EEikAppUiResolveError,
        EEikAppUiExtension,
        EEikAppUiEnableTaskList,
        EEikAppUiGetDebugPreferences,
        EEikAppUiResolveErrorWithTitleText,
        // End Marker no Capability
        EEikAppUiFirstUnusedOpcode,
    };

    struct debug_preferences {
        int flags{ 0 };
        int spare{ 0 };

        std::string to_buf();
    };

    // Unless app do weird stuff like checking if the eik app ui semaphore exists,
    // this server will does thing just fine
    class eikappui_server : public service::server {
    protected:
        void get_debug_preferences(service::ipc_context &ctx);

    public:
        eikappui_server(eka2l1::system *sys);
    };
}

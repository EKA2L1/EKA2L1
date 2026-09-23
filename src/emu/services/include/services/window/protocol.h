/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <services/window/common.h>
#include <services/window/op.h>
#include <utils/version.h>

namespace eka2l1::epoc {
    // The client build selects the wire ABI; OS 7.0 reports 151 with an earlier table.
    class window_server_protocol {
        epocver os_;
        version client_;

        bool versioned_opcodes() const {
            return client_.major == WS_MAJOR_VER && client_.minor == WS_MINOR_VER;
        }

        bool legacy_opcodes() const {
            return client_.build <= WS_OLDARCH_VER || os_ == epocver::epoc70;
        }

    public:
        window_server_protocol(epocver os, version client) : os_(os), client_(client) {}

        std::uint16_t session_opcode(std::uint16_t opcode) const {
            if (versioned_opcodes() && legacy_opcodes()) {
                if (opcode >= ws_cl_op_start_custom_text_cursor) {
                    opcode += 2;
                }
                if (os_ == epocver::epoc70 && opcode >= ws_cl_op_set_faded) {
                    ++opcode;
                }
            }
            return opcode;
        }

        std::uint16_t window_opcode(std::uint16_t opcode) const {
            if (versioned_opcodes()) {
                if (legacy_opcodes() && opcode >= EWsWinOpAbsPosition) {
                    ++opcode;
                }
                if (client_.build <= WS_NEWARCH_VER && os_ <= epocver::epoc94
                    && opcode >= EWsWinOpSendAdvancedPointerEvent) {
                    ++opcode;
                }
                if (os_ == epocver::epoc70 && opcode >= EWsWinOpEnableGroupListChangeEvents) {
                    opcode += 2;
                }
            }
            return opcode;
        }

        bool legacy_dsa() const {
            return client_.build <= WS_OLDARCH_VER || os_ <= epocver::epoc80;
        }

        // A sync-thread client of this table reads GetRegion's result as a rect count and expects 0.
        bool legacy_dsa_region() const {
            return legacy_opcodes();
        }
    };
}

/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <kernel/reg.h>
#include <kernel/svc.h>

#include <kernel/libmanager.h>
#include <bridge/bridge.h>

namespace eka2l1::epoc {
    void register_epocv93(eka2l1::hle::lib_manager &mngr) {
        ADD_SVC_REGISTERS(mngr, svc_register_funcs_v93);
    }

    void register_epocv94(eka2l1::hle::lib_manager &mngr) {
        ADD_SVC_REGISTERS(mngr, svc_register_funcs_v94);
    }

    void register_epocv10(eka2l1::hle::lib_manager &mngr) {
        ADD_SVC_REGISTERS(mngr, svc_register_funcs_v10);
    }
    
    void register_epocv6(eka2l1::hle::lib_manager &mngr) {
        ADD_SVC_REGISTERS(mngr, svc_register_funcs_v6);
    }
}
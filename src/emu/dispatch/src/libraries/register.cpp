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

#include <dispatch/dispatcher.h>
#include <dispatch/libraries/register.h>
#include <dispatch/libraries/sysutils/register.h>
#include <dispatch/libraries/egl/register.h>
#include <dispatch/libraries/gles1/register.h>

#include <kernel/kernel.h>
#include <config/config.h>

namespace eka2l1::dispatch::libraries {
    void register_functions(kernel_system *kern, dispatcher *disp) {
        if (kern->get_epoc_version() <= epocver::epoc81a) {
            disp->patch_libraries(u"Z:\\System\\Libs\\SysUtil.dll", SYSUTILS_PATCH_EPOCV81A_INFOS,
                SYSUTILS_PATCH_EPOCV81A_COUNT);
        } else if (kern->get_epoc_version() >= epocver::epoc93fp1) {
            config::state *conf = kern->get_config();

            if (conf->enable_hw_gles1) {
                disp->patch_libraries(u"Z:\\Sys\\Bin\\libgles_cm.dll", LIBGLES_CM_PATCH_INFOS, LIBGLES_CM_PATCH_COUNT);
                disp->patch_libraries(u"Z:\\Sys\\Bin\\libegl.dll", LIBEGL_PATCH_INFOS, LIBEGL_PATCH_COUNT);
                disp->patch_libraries(u"Z:\\Sys\\Bin\\libglesv1_cm.dll", LIBGLES_V1_CM_PATCH_INFOS, LIBGLES_V1_CM_PATCH_COUNT);
            }
        }
    }
}
/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#pragma once

#include <cstdint>
#include <dispatch/libraries/register.h>

namespace eka2l1::dispatch {
    static const patch_info LIBEGL_PATCH_INFOS[] = {
        // Dispatch number, Ordinal number
        { 0x11AF, 2 },      // eglBindAPI
        { 0x11AC, 3 },      // eglBindTexImage
        { 0x1100, 4 },      // eglChooseConfig
        { 0x1101, 5 },      // eglCopyBuffers
        { 0x1102, 6 },      // eglCreateContext
        { 0x11B0, 7 },      // eglCreatePbufferFromClientBuffer
        { 0x1103, 8 },      // eglCreatePbufferSurface
        { 0x1104, 9 },      // eglCreatePixmapSurface
        { 0x1105, 10 },     // eglCreateWindowSurface
        { 0x1106, 11 },     // eglDestroyContext
        { 0x1107, 12 },     // eglDestroySurface
        { 0x1108, 13 },     // eglGetConfigAttrib
        { 0x1109, 14 },     // eglGetConfigs
        { 0x110A, 15 },     // eglGetCurrentContext
        { 0x110B, 16 },     // eglGetCurrentDisplay
        { 0x110C, 17 },     // eglGetCurrentSurface
        { 0x110D, 18 },     // eglGetDisplay
        { 0x110E, 19 },     // eglGetError
        { 0x110F, 20 },     // eglGetProcAddress
        { 0x1110, 21 },     // eglInitialize
        { 0x1111, 22 },     // eglMakeCurrent
        { 0x11B1, 23 },     // eglQueryAPI
        { 0x1112, 24 },     // eglQueryContext
        { 0x1113, 25 },     // eglQueryString
        { 0x1114, 26 },     // eglQuerySurface
        { 0x11AD, 27 },     // eglReleaseTexImage
        { 0x11B2, 28 },     // eglReleaseThread
        { 0x11AE, 29 },     // eglSurfaceAttrib
        { 0x1115, 30 },     // eglSwapBuffers
        { 0x1184, 31 },     // eglSwapInterval
        { 0x1116, 32 },     // eglTerminate
        { 0x11B3, 33 },     // eglWaitClient
        { 0x1117, 34 },     // eglWaitGL
        { 0x1118, 35 }      // eglWaitNative
    };

    static const std::uint32_t LIBEGL_PATCH_COUNT = sizeof(LIBEGL_PATCH_INFOS) / sizeof(patch_info);
}
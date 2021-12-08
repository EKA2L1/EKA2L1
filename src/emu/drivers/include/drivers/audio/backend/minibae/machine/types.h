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

#pragma once

#include <common/platform.h>
#include <stdint.h>

#if EKA2L1_PLATFORM(WIN32)
#include <windows.h>
#include <stdlib.h>
#include <windowsx.h>
#include <commdlg.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <mmsystem.h>   // for timeGetTime()

#define X_PLATFORM X_WIN95
#else
#include <pthread.h>
#if EKA2L1_PLATFORM(MACOS)
#define X_PLATFORM X_MACINTOSH
#elif EKA2L1_PLATFORM(UNIX)
#if EKA2L1_PLATFORM(ANDROID)
#define X_PLATFORM X_ANDROID
#else
#define X_PLATFORM X_LINUX
#endif
#endif 
#endif

#if EKA2L1_ARCH(ARM64)
#define CPU_TYPE kAarch64
#elif EKA2L1_ARCH(X64)
#define CPU_TYPE kx86_64
#elif EKA2L1_ARCH(ARM)
#define CPU_TYPE kARM
#endif
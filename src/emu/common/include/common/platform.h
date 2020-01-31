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

/*  Purpose of the file is like PPSSPP: define platform and arch macro to make
    it easier for porting things in the future.
*/

#pragma once

#define EKA2L1_PLATFORM(PLAT) EKA2L1_PLATFORM_##PLAT
#define EKA2L1_ARCH(ARCH) EKA2L1_ARCH_##ARCH

#ifdef _MSC_VER
#ifdef _M_X86
#define EKA2L1_ARCH_X86 1
#endif

#ifdef _M_ARM
#define EKA2L1_ARCH_ARM 1
#endif

#ifdef _M_ARM64
#define EKA2L1_ARCH_ARM64 1
#endif

#ifdef _M_X64
#define EKA2L1_ARCH_X64 1
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#ifdef __i386__
#define EKA2L1_ARCH_X86 1
#endif

#ifdef __x86_64__
#define EKA2L1_ARCH_X64 1
#endif

#ifdef __arm__
#define EKA2L1_ARCH_ARM 1
#endif

#ifdef __aarch64__
#define EKA2L1_ARCH_ARM64 1
#endif
#endif

#ifdef _WIN32
#define EKA2L1_PLATFORM_WIN32 1

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
#define EKA2L1_PLATFORM_WIN32_DESKTOP 1
#else
#define EKA2L1_PLATFORM_UWP 1
#endif
#endif

#ifdef __unix__
#define EKA2L1_PLATFORM_UNIX 1
#endif

#ifdef __APPLE__
#define EKA2L1_PLATFORM_DARWIN 1

#include <TargetConditionals.h>

#if TARGET_OS_MAC
#define EKA2L1_PLATFORM_MACOS 1
#else
#if TARGET_OS_IPHONE
#define EKA2L1_PLATFORM_IOS 1
#endif
#endif
#endif

#ifdef __ANDROID__
#define EKA2L1_PLATFORM_ANDROID
#endif

#ifdef __EPOC32__
#define EKA2L1_PLATFORM_SYMBIAN

#if defined(__S60_3X__) || defined(__SERIES60_3X__)
#define EKA2L1_PLATFORM_S60V3
#elif defined(__S60_50__)
#define EKA2L1_PLATFORM_S60V5
#endif
#endif

#if EKA2L1_PLATFORM(ANDROID) || EKA2L1_PLATFORM(UNIX) || EKA2L1_PLATFORM(MACOS) || EKA2L1_PLATFORM(IOS)
#define EKA2L1_PLATFORM_POSIX 1
#endif

namespace eka2l1 {
    // Add JIT backend name here
    static constexpr const char *dynarmic_jit_backend_name = "dynarmic";  ///< Dynarmic recompiler backend name
    static constexpr const char *unicorn_jit_backend_name = "unicorn";    ///< Unicorn recompiler backend name
    static constexpr const char *earm_jit_backend_name = "earm";          ///< EKA2L1's ARM recompiler backend name
}
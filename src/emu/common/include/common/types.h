/*
 * Copyright (c) 2018 EKA2L1 Team / RPCS3 Project.
 * 
 * This file is part of EKA2L1 project / RPCS3 Project 
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

#include <functional>
#include <string>

#define FOUND_STR(x) x != std::string::npos
#define INVALID_HANDLE 0xFFFFFFFF

typedef std::u16string utf16_str;
typedef uint32_t address;

typedef unsigned int uint;

struct uint128_t {
    uint64_t low;
    uint64_t high;
};

enum class prot {
    none = 0,
    read = 1,
    write = 2,
    exec = 3,
    read_write = 4,
    read_exec = 5,
    read_write_exec = 6
};

// This can be changed manually
enum class epocver {
    epocu6,
    epoc6, // Epoc 6.0
    epoc93, // Epoc 9.3
    epoc9, // Epoc 9.4
    epoc10
};

enum drive_number {
    drive_a,
    drive_b,
    drive_c,
    drive_d,
    drive_e,
    drive_f,
    drive_g,
    drive_h,
    drive_i,
    drive_j,
    drive_k,
    drive_l,
    drive_m,
    drive_n,
    drive_o,
    drive_p,
    drive_q,
    drive_r,
    drive_s,
    drive_t,
    drive_u,
    drive_v,
    drive_w,
    drive_x,
    drive_y,
    drive_z,
    drive_count
};

enum class io_attrib {
    none,
    include_dir = 0x50,
    hidden = 0x100,
    write_protected = 0x200,
    internal = 0x400,
    removeable = 0x800
};

inline io_attrib operator|(io_attrib a, io_attrib b) {
    return static_cast<io_attrib>(static_cast<int>(a) | static_cast<int>(b));
}

inline io_attrib operator&(io_attrib a, io_attrib b) {
    return static_cast<io_attrib>(static_cast<int>(a) & static_cast<int>(b));
}

enum class drive_media {
    none,
    usb,
    physical,
    reflect,
    rom,
    ram
};

enum arm_emulator_type {
    unicorn = 0,
    dynarmic = 1
};

typedef std::uint32_t vaddress;

int translate_protection(prot cprot);
char16_t drive_to_char16(const drive_number drv);
drive_number char16_to_drive(const char16_t c);

// Formatting helper, type-specific preprocessing for improving safety and functionality
template <typename T, typename = void>
struct fmt_unveil;

template <typename Arg>
using fmt_unveil_t = typename fmt_unveil<Arg>::type;

struct fmt_type_info;

namespace fmt
{
	template <typename... Args>
	const fmt_type_info* get_type_info();
}

namespace fmt
{
	[[noreturn]] void raw_error(const char* msg);
	[[noreturn]] void raw_verify_error(const char* msg, const fmt_type_info* sup, std::uint64_t arg);
	[[noreturn]] void raw_narrow_error(const char* msg, const fmt_type_info* sup, std::uint64_t arg);
}

struct verify_func
{
	template <typename T>
	bool operator()(T&& value) const
	{
		if (std::forward<T>(value))
		{
			return true;
		}

		return false;
	}
};

template <uint N>
struct verify_impl
{
	const char* cause;

	template <typename T>
	auto operator,(T&& value) const
	{
		// Verification (can be safely disabled)
		if (!verify_func()(std::forward<T>(value)))
		{
			fmt::raw_verify_error(cause, nullptr, N);
		}

		return verify_impl<N + 1>{cause};
	}
};

// Verification helper, checks several conditions delimited with comma operator
inline auto verify(const char* cause)
{
	return verify_impl<0>{cause};
}

// Verification helper (returns value or lvalue reference, may require to use verify_move instead)
template <typename F = verify_func, typename T>
inline T verify(const char* cause, T&& value, F&& pred = F())
{
	if (!pred(std::forward<T>(value)))
	{
		using unref = std::remove_const_t<std::remove_reference_t<T>>;
		fmt::raw_verify_error(cause, fmt::get_type_info<fmt_unveil_t<unref>>(), fmt_unveil<unref>::get(value));
	}

	return std::forward<T>(value);
}

// Verification helper (must be used in return expression or in place of std::move)
template <typename F = verify_func, typename T>
inline std::remove_reference_t<T>&& verify_move(const char* cause, T&& value, F&& pred = F())
{
	if (!pred(std::forward<T>(value)))
	{
		using unref = std::remove_const_t<std::remove_reference_t<T>>;
		fmt::raw_verify_error(cause, fmt::get_type_info<fmt_unveil_t<unref>>(), fmt_unveil<unref>::get(value));
	}

	return std::move(value);
}
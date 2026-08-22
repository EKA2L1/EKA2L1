/*
 * Copyright (c) 2018 EKA2L1 Team
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

// Native (no-Lua) implementation of the compatibility patches that ship as
// scripts/*.lua on platforms with a LuaJIT runtime. LuaJIT cannot run on iOS
// (its FFI callbacks need writable-executable memory, which the OS forbids on
// non-jailbroken devices), so these builds compile the patch table directly in
// C++. Each entry mirrors one shipped *.lua script; keep them in sync.

#include <common/configure.h>

#ifndef ENABLE_SCRIPTING_LUA

#include <common/log.h>
#include <common/types.h>

#include <scripting/cpu.h>
#include <scripting/manager.h>

#include <kernel/libmanager.h>
#include <system/epoc.h>

namespace eka2l1::manager {
    // Breakpoint callbacks are plain C function pointers (breakpoint_hit_func).
    // Captureless lambdas convert to them, and touch no writable-executable
    // memory, so this whole path is iOS-safe.

    // --- AstroQuest (mBounce) crash fix (aq_crash_fix.lua) ---
    // Crash caused by a garbage variable value not being 0 because User::Free
    // did not clear R2.
    static void aq_fix_garbage_value_not_zero() {
        scripting::cpu::set_register(2, 0);
    }

    // --- Eternal Legacy (Gameloft) crash fix (eternal_legacy_crash_fix.lua) ---
    // The game hashes the configuration header string instead of the intended
    // one; feed it the combined string length in R8 instead of R7.
    static void el_fix_wrong_glsl_code_hash() {
        scripting::cpu::set_register(7, scripting::cpu::get_register(8));
    }

    // --- Hero of Sparta (Gameloft) crash fix (hero_of_sparta_crash_fix.lua) ---
    // The 0x80000 (~0.5MB) temporary file buffer fills too fast; enlarge it.
    static void hos_fix_temp_file_buffer_too_small() {
        scripting::cpu::set_register(0, 6 * 1024 * 1024); // 6MB
    }

    // --- Warhammer 40K slow loading fix (wh40k_slow_load_fix.lua) ---
    // The loading active object gets starved because screen update and audio run
    // at a fast rate on a modern CPU.
    static void wh40k_adjust_loading_active_obj_priority() {
        scripting::cpu::set_register(0, 0x14);
    }

    static void wh40k_skip_recursive_wait() {
        scripting::cpu::set_register(15, scripting::cpu::get_pc() + 24);
    }

    static void wh40k_adjust_audio_feedback_wait_time() {
        scripting::cpu::set_register(0, 3500);
    }

    // --- S60v3 empty Avkon menu fix ---
    // These Avkon builds index Count()-1 without checking for an empty
    // menu-title array. Later versions end menu display through the existing
    // cleanup path instead.
    static void skip_empty_avkon_menu(const std::uint32_t cleanup_offset) {
        if (static_cast<std::int32_t>(scripting::cpu::get_register(1)) < 0) {
            scripting::cpu::set_register(7, 0);
            scripting::cpu::set_register(15, scripting::cpu::get_pc() + cleanup_offset);
        }
    }

    static void rm409_skip_empty_avkon_menu() {
        skip_empty_avkon_menu(0xF4);
    }

    static void rm320_skip_empty_avkon_menu() {
        skip_empty_avkon_menu(0x12C);
    }

    void scripts::register_builtin_patches() {
        // The kernel-level hooks must be live for breakpoints to fire.
        register_kernel_hooks();

        // A synthetic module owns the patch functions, mirroring how each Lua
        // script owns its callbacks.
        auto native_module = std::make_shared<script_module>();
        modules.emplace("<builtin>", native_module);
        current_module = native_module;

        // AstroQuest (mBounce)
        register_breakpoint("AQ.app", 0x100040C9, 0, 0x004750E8, 0, aq_fix_garbage_value_not_zero);

        // Eternal Legacy (Gameloft)
        register_breakpoint("eternallegacy.exe", 0x00270488, 0, 0, 0x7C16F812, el_fix_wrong_glsl_code_hash);

        // Hero of Sparta (Gameloft) - 1.1.6 and 1.0.9
        register_breakpoint("heroofsparta.exe", 0x000886B3, 0, 0, 0x612393CE, hos_fix_temp_file_buffer_too_small);
        register_breakpoint("heroofsparta.exe", 0x00089063, 0, 0, 0x6094E8CC, hos_fix_temp_file_buffer_too_small);

        // Warhammer 40K - registered against both the app codeseg (6r92.app) and
        // its main.dll, for the two known versions (hash 0x5F894B9D / 0x8EFD9C7).
        struct wh40k_patch {
            std::uint32_t addr_;
            breakpoint_hit_func func_;
        };

        static constexpr std::uint32_t WH40K_UID3 = 0x101FD427;

        const wh40k_patch wh40k_v042b[] = {
            { 0x100CC880, wh40k_adjust_loading_active_obj_priority },
            { 0x1006D3E8, wh40k_skip_recursive_wait },
            { 0x1006D3D0, wh40k_adjust_audio_feedback_wait_time },
        };

        const wh40k_patch wh40k_v069[] = {
            { 0x100DB3F8, wh40k_adjust_loading_active_obj_priority },
            { 0x1007290C, wh40k_skip_recursive_wait },
            { 0x100728DC, wh40k_adjust_audio_feedback_wait_time },
        };

        for (const char *lib : { "6r92.app", "main.dll" }) {
            for (const wh40k_patch &p : wh40k_v042b) {
                register_breakpoint(lib, p.addr_, 0, WH40K_UID3, 0x5F894B9D, p.func_);
            }
            for (const wh40k_patch &p : wh40k_v069) {
                register_breakpoint(lib, p.addr_, 0, WH40K_UID3, 0x8EFD9C7, p.func_);
            }
        }

        // CEikMenuBar::StartDisplayingMenuBarL is export 70 in these Avkon
        // builds. Resolve it dynamically and verify only that method's bytes;
        // the hook offset is relative to the method, not a firmware address.
        register_rom_export_breakpoint("eikcoctl.dll", 70, 0x39A36149,
            0x182, 0, 0x1000489E, rm409_skip_empty_avkon_menu);
        register_rom_export_breakpoint("eikcoctl.dll", 70, 0x1595EE13,
            0x17E, 0, 0x1000489E, rm320_skip_empty_avkon_menu);

        current_module = nullptr;

        // --- S^3 driver patch (s3_driver_patch.lua) ---
        // Runs at load time (not a breakpoint). On EPOC9.5+, preload ROM DLLs
        // that a real phone already has resident at startup but the emulator
        // does not (EComServer.exe needs domaincli.dll). The OpenVG-accel-to-sw
        // swap is left disabled to match the shipped script.
        if (sys->get_symbian_version_use() >= epocver::epoc95) {
            LOG_INFO(SCRIPTING, "Applying S^3 and higher patch (native)");

            if (hle::lib_manager *libmngr = sys->get_lib_manager()) {
                libmngr->load(u"z:\\sys\\bin\\domaincli.dll");
            }
        }

        LOG_INFO(SCRIPTING, "Built-in native game patches registered");
    }
}

#endif // !ENABLE_SCRIPTING_LUA

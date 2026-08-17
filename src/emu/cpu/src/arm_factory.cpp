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
#include <common/log.h>
#include <common/platform.h>
#include <cpu/arm_factory.h>

#include <cpu/dyncom/arm_dyncom.h>

// Dynarmic availability: on iOS the EKA2L1_IOS_DYNARMIC flag (one cmake
// option = one compile macro) marks builds that carry dynarmic — simulator
// builds by default, sideload-only device builds opt in, App Store /
// TestFlight never. The dyncom-only build (e.g. the differential test
// harness) forces dynarmic off on any host so it doesn't drag in the
// dynarmic headers/library.
#if !defined(EKA2L1_CPU_DYNCOM_ONLY_BUILD)
#define EKA2L1_CPU_DYNCOM_ONLY_BUILD 0
#endif
#define EKA2L1_CPU_HAS_DYNARMIC (!EKA2L1_ARCH(ARM) && (!EKA2L1_PLATFORM(IOS) || EKA2L1_IOS_DYNARMIC) && !EKA2L1_CPU_DYNCOM_ONLY_BUILD)

#if EKA2L1_ARCH(ARM)
#include <cpu/12l1r/arm_12l1r.h>
#elif EKA2L1_CPU_HAS_DYNARMIC
#include <cpu/arm_dynarmic.h>
#endif

#include <cpu/12l1r/exclusive_monitor.h>

#if EKA2L1_PLATFORM(IOS) && EKA2L1_IOS_DYNARMIC
#include <libkern/OSCacheControl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace eka2l1::arm {
#if EKA2L1_PLATFORM(IOS) && EKA2L1_IOS_DYNARMIC
    // Probe whether this process can actually create and run generated code.
    // On a jailed iPhone, mprotect(RW -> RX) on dirty anonymous pages only
    // succeeds when the process is debuggable (CS_DEBUGGED), i.e. a sideloaded
    // build with JIT enabled via debugger / JIT enabler. App Store and
    // TestFlight processes always fail the mprotect, so this degrades cleanly.
    // Mirrors the RW->RX dance oaknut's CodeBlock does on TARGET_OS_IPHONE.
    static bool probe_ios_jit() {
        const size_t page = static_cast<size_t>(getpagesize());
        void *mem = mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        if (mem == MAP_FAILED) {
            return false;
        }

        // AArch64 `ret`.
        *reinterpret_cast<std::uint32_t *>(mem) = 0xD65F03C0;

        bool ok = (mprotect(mem, page, PROT_READ | PROT_EXEC) == 0);
        if (ok) {
            sys_icache_invalidate(mem, page);
            reinterpret_cast<void (*)()>(mem)();
        }

        munmap(mem, page);
        return ok;
    }
#endif

    bool host_can_jit() {
#if EKA2L1_PLATFORM(IOS)
#if EKA2L1_IOS_DYNARMIC
        static const bool available = probe_ios_jit();
        return available;
#else
        return false;
#endif
#elif EKA2L1_ARCH(ARM)
        // 32-bit ARM hosts use 12l1r rather than dynarmic, treat as JIT-capable.
        return true;
#elif EKA2L1_CPU_HAS_DYNARMIC
        return true;
#else
        return false;
#endif
    }

    arm_emulator_type resolve_emulator_type(arm_emulator_type requested, const char **out_reason) {
        const char *reason = nullptr;
        arm_emulator_type resolved = requested;

#if EKA2L1_PLATFORM(IOS)
        if (requested == arm_emulator_type::r12l1 || requested == arm_emulator_type::unicorn) {
            reason = "backend not available on iOS";
            resolved = arm_emulator_type::dyncom;
        } else if (requested == arm_emulator_type::dynarmic && !host_can_jit()) {
#if EKA2L1_IOS_DYNARMIC
            reason = "process has no JIT permission (needs sideload + debugger/JIT enabler)";
#else
            reason = "JIT not compiled into this build (App Store / TestFlight)";
#endif // EKA2L1_IOS_DYNARMIC
            resolved = arm_emulator_type::dyncom;
        }
#else
        (void)requested;
#endif

        if (out_reason) {
            *out_reason = reason;
        }
        return resolved;
    }

    core_instance create_core(exclusive_monitor *monitor, arm_emulator_type arm_type) {
        const char *reason = nullptr;
        const arm_emulator_type resolved = resolve_emulator_type(arm_type, &reason);
        if (reason) {
            LOG_WARN(CPU, "CPU backend request {} downgraded to {} ({})",
                static_cast<int>(arm_type), static_cast<int>(resolved), reason);
        }

        switch (resolved) {
        case arm_emulator_type::unicorn:
            return nullptr;

#if EKA2L1_ARCH(ARM)
        case arm_emulator_type::r12l1:
            return std::make_unique<r12l1_core>(monitor, 12);
#elif EKA2L1_CPU_HAS_DYNARMIC
        case arm_emulator_type::dynarmic:
            return std::make_unique<dynarmic_core>(monitor);
#endif

        case arm_emulator_type::dyncom:
            return std::make_unique<dyncom_core>(monitor, 12);

        default:
            break;
        }

        return nullptr;
    }

    exclusive_monitor_instance create_exclusive_monitor(arm_emulator_type arm_type, const std::size_t core_count) {
        switch (arm_type) {
        case arm_emulator_type::unicorn:
            return nullptr;

        case arm_emulator_type::dyncom:
            return std::make_unique<r12l1::exclusive_monitor>(core_count);

#if EKA2L1_ARCH(ARM)
        case arm_emulator_type::r12l1:
            return std::make_unique<r12l1::exclusive_monitor>(core_count);
#elif EKA2L1_CPU_HAS_DYNARMIC
        case arm_emulator_type::dynarmic:
            return std::make_unique<dynarmic_exclusive_monitor>(core_count);
#endif

        default:
            break;
        }

        return nullptr;
    }
}

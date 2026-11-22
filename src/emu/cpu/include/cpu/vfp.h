// Copyright (c) 2026 EKA2L1 Team. Licensed under GPLv2 or later.
#pragma once

#include <array>
#include <cstdint>

inline constexpr std::uint32_t arm11_fpsid = 0x410120b4;
inline constexpr std::uint32_t arm11_mvfr0 = 0x11111111;
inline constexpr std::uint32_t arm11_mvfr1 = 0;

struct vfp_register_view {
    std::array<std::uint32_t, 64> &ExtReg;
};

std::uint32_t vfp_single_cpdo(vfp_register_view *state, std::uint32_t inst, std::uint32_t fpscr, bool precise);
std::uint32_t vfp_double_cpdo(vfp_register_view *state, std::uint32_t inst, std::uint32_t fpscr);
std::uint32_t vfp_apply_exceptions(std::uint32_t exceptions, std::uint32_t inst, std::uint32_t fpscr);

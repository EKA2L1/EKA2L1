// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <windows.h>
#include <string>

#include <drivers/graphics/context.h>

namespace eka2l1::drivers::graphics {
    class gl_context_wgl final : public gl_context {
    public:
        explicit gl_context_wgl() = default;
        explicit gl_context_wgl(const drivers::window_system_info& wsi, bool stereo, bool core);

        ~gl_context_wgl() override;

        bool is_headless() const override;

        std::unique_ptr<gl_context> create_shared_context() override;

        bool make_current() override;
        bool clear_current() override;

        void swap_buffers() override;
        void set_swap_interval(const std::int32_t interval) override;

        void update(const std::uint32_t new_width, const std::uint32_t new_height) override;

    protected:
        static HGLRC create_core_context(HDC dc, HGLRC share_context);
        static bool create_pbuffer(HDC onscreen_dc, int width, int height, HANDLE* pbuffer_handle, HDC* pbuffer_dc);

        HWND m_window_handle = nullptr;
        HANDLE m_pbuffer_handle = nullptr;
        HDC m_dc = nullptr;
        HGLRC m_rc = nullptr;
    };
}
/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <common/cvt.h>
#include <common/log.h>

#include <services/ui/cap/oom_app.h>
#include <services/window/window.h>
#include <utils/err.h>

#include <epoc/epoc.h>
#include <kernel/kernel.h>
#include <vfs/vfs.h>

namespace eka2l1 {
    oom_ui_app_server::oom_ui_app_server(eka2l1::system *sys)
        : service::typical_server(sys, OOM_APP_UI_SERVER_NAME) {
    }

    void oom_ui_app_server::connect(service::ipc_context &ctx) {
        create_session<oom_ui_app_session>(&ctx);

        if (!sgc) {
            init(ctx.sys->get_kernel_system());
        }

        typical_server::connect(ctx);
    }

    oom_ui_app_session::oom_ui_app_session(service::typical_server *svr, service::uid client_ss_uid, epoc::version client_version)
        : service::typical_session(svr, client_ss_uid, client_version)
        , blank_count(0) {
    }

    void oom_ui_app_session::redraw_status_pane(service::ipc_context *ctx) {
        server<oom_ui_app_server>()->redraw_status_pane();
        ctx->set_request_status(epoc::error_none);
    }

    void oom_ui_app_session::fetch(service::ipc_context *ctx) {
        if (ctx->sys->get_symbian_version_use() <= epocver::epoc93) {
            // Move app in z order does not exist. Forward to other message
            if (ctx->msg->function >= akn_eik_app_ui_move_app_in_z_order) {
                ctx->msg->function++;
            }
        }

        switch (ctx->msg->function) {
        case akn_eik_app_ui_layout_config_size: {
            server<oom_ui_app_server>()->get_layout_config_size(*ctx);
            break;
        }

        case akn_eik_app_ui_get_layout_config: {
            server<oom_ui_app_server>()->get_layout_config(*ctx);
            break;
        }

        case akn_eik_app_ui_set_sgc_params: {
            server<oom_ui_app_server>()->set_sgc_params(*ctx);
            break;
        }

        case akns_blank_screen: {
            blank_count++;

            if (blank_count == 0) {
                // No way... This is impossible
                LOG_ERROR("App session has blank count negative before called blank screen");
                ctx->set_request_status(epoc::error_abort);
                break;
            }

            LOG_TRACE("Blanking screen in AKNCAP session stubbed");
            ctx->set_request_status(epoc::error_none);
            break;
        }

        case akns_unblank_screen: {
            blank_count--;

            LOG_TRACE("Unblanking screen in AKNCAP session stubbed");
            ctx->set_request_status(epoc::error_none);
            break;
        }

        case akn_eik_app_ui_redraw_server_status_pane: {
            redraw_status_pane(ctx);
            break;
        }

        case akns_update_key_block_mode:
            server<oom_ui_app_server>()->update_key_block_mode(*ctx);
            break;

        default: {
            LOG_WARN("Unimplemented opcode for OOM AKNCAP server: 0x{:X}, fake return with epoc::error_none", ctx->msg->function);
            ctx->set_request_status(epoc::error_none);
        }
        }
    }

    void oom_ui_app_server::redraw_status_pane() {
        LOG_TRACE("Status pane redrawed");
    }

    static std::uint32_t calculate_screen_style_hash(const std::string &style) {
        std::uint64_t hash = 0;
        static constexpr std::uint8_t HASH_MULT = 131;

        for (const char c : style) {
            hash *= HASH_MULT;
            hash += c;
        }

        return static_cast<std::uint32_t>(hash);
    }

    std::string oom_ui_app_server::get_layout_buf() {
        if (!winsrv) {
            winsrv = reinterpret_cast<window_server *>(&(*sys->get_kernel_system()->get_by_name<service::server>(
                eka2l1::get_winserv_name_by_epocver(sys->get_symbian_version_use()))));
        }

        epoc::config::screen *scr_config = winsrv->get_current_focus_screen_config();
        assert(scr_config && "Current screen config must be valid");

        akn_layout_config akn_config;

        akn_config.num_screen_mode = 1;

        // TODO: Find out what this really does
        akn_config.num_hardware_mode = 0;

        // Static check on those pointer
        akn_config.screen_modes = sizeof(akn_layout_config);
        akn_config.hardware_infos = sizeof(akn_layout_config) + sizeof(akn_screen_mode_info) * akn_config.num_screen_mode;

        std::string result;
        result.append(reinterpret_cast<char *>(&akn_config), sizeof(akn_layout_config));

        for (std::size_t i = 0; i < 1; i++) {
            akn_screen_mode_info mode_info;

            // TODO: Change this based on user settings
            mode_info.loc = akn_softkey_loc::bottom;
            mode_info.mode_num = scr_config->modes[i].mode_number;
            mode_info.dmode = epoc::display_mode::color16ma;
            mode_info.info.orientation = epoc::number_to_orientation(scr_config->modes[i].rotation);
            mode_info.info.pixel_size = scr_config->modes[i].size;
            mode_info.info.twips_size = mode_info.info.pixel_size * twips_mul;
            mode_info.screen_style_hash = calculate_screen_style_hash(scr_config->modes[i].style);

            result.append(reinterpret_cast<char *>(&mode_info), sizeof(akn_screen_mode_info));
        }

        // TODO: Hardware mode

        return result;
    }

    void oom_ui_app_server::get_layout_config_size(service::ipc_context &ctx) {
        layout_buf = get_layout_buf();

        int layout_buf_size = static_cast<int>(layout_buf.size());

        ctx.write_arg_pkg<int>(0, layout_buf_size);
        ctx.set_request_status(epoc::error_none);
    }

    void oom_ui_app_server::get_layout_config(service::ipc_context &ctx) {
        layout_buf = get_layout_buf();

        ctx.write_arg_pkg(0, reinterpret_cast<std::uint8_t *>(&layout_buf[0]),
            static_cast<std::uint32_t>(layout_buf.size()));

        ctx.set_request_status(epoc::error_none);
    }

    void oom_ui_app_server::set_sgc_params(service::ipc_context &ctx) {
        std::optional<epoc::sgc_params> params = ctx.get_arg_packed<epoc::sgc_params>(0);

        if (!params.has_value()) {
            ctx.set_request_status(epoc::error_argument);
            return;
        }

        sgc->change_wg_param(params->window_group_id, *reinterpret_cast<epoc::cap::sgc_server::wg_state::wg_state_flags *>(&(params->bit_flags)), params->sp_layout, params->sp_flag, params->app_screen_mode);
        ctx.set_request_status(epoc::error_none);
    }

    void oom_ui_app_server::update_key_block_mode(service::ipc_context &ctx) {
        std::optional<std::uint32_t> disable_it = ctx.get_arg<std::uint32_t>(0);

        if (!disable_it.has_value()) {
            ctx.set_request_status(epoc::error_argument);
            return;
        }

        eik->key_block_mode(!static_cast<bool>(disable_it.value()));
        ctx.set_request_status(epoc::error_none);
    }

    void oom_ui_app_server::init(kernel_system *kern) {
        const std::lock_guard<std::mutex> guard(lock);

        sgc = std::make_unique<epoc::cap::sgc_server>();
        eik = std::make_unique<epoc::cap::eik_server>(kern);

        sgc->init(kern, sys->get_graphics_driver());
        eik->init(kern);
    }
}

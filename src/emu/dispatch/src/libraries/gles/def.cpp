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

#include <dispatch/libraries/gles/def.h>
#include <drivers/graphics/graphics.h>

#include <common/log.h>

namespace eka2l1::dispatch {
    void egl_context::free(drivers::graphics_driver *driver, drivers::graphics_command_list_builder &builder) {
        builder.destroy(surface_fb_);
    }

    egl_controller::egl_controller() {
    }

    egl_controller::~egl_controller() {
        auto cmd_list = driver_->new_command_list();
        auto cmd_builder = driver_->new_command_builder(cmd_list.get());

        bool freed_once = false;

        for (auto &ctx: contexts_) {
            if (ctx) {
                ctx->free(driver_, *cmd_builder);
                freed_once = true;
            }
        }

        if (freed_once) {
            driver_->submit_command_list(*cmd_list);
        }
    }

    bool egl_controller::make_current(kernel::uid thread_id, const egl_context_handle handle) {
        auto *context_ptr = contexts_.get(handle);
        if (!context_ptr) {
            return false;
        }

        auto ite = active_context_.find(thread_id);
        if (ite != active_context_.end()) {
            LOG_ERROR(HLE_DISPATCHER, "Another context is currentlty active!");
            return false;
        }

        active_context_.emplace(thread_id, context_ptr->get());
        return true;
    }

    void egl_controller::clear_current(kernel::uid thread_id) {
        auto ite = active_context_.find(thread_id);
        if (ite != active_context_.end()) {
            active_context_.erase(ite);
        }
    }

    egl_context_handle egl_controller::add_context(egl_context_instance &instance) {
        return static_cast<egl_context_handle>(contexts_.add(instance));
    }

    void egl_controller::remove_context(const egl_context_handle handle) {
        auto *context_ptr = contexts_.get(handle);
        if (!context_ptr) {
            return;
        }

        for (auto ite = active_context_.begin(); ite != active_context_.end(); ) {
            if (ite->second == context_ptr->get()) {
                LOG_ERROR(HLE_DISPATCHER, "Active context is cleared for thread {}!", ite->first);
                active_context_.erase(ite++);
            } else {
                ite++;
            }
        }

        contexts_.remove(static_cast<std::size_t>(handle));
    }

    void egl_controller::push_error(kernel::uid thread_id, const std::uint32_t error) {
        error_map_[thread_id] = error;
    }

    std::uint32_t egl_controller::pop_error(kernel::uid thread_id) {
        auto ite = error_map_.find(thread_id);
        if (ite == error_map_.end()) {
            return GL_NO_ERROR;
        }

        std::uint32_t result = ite->second;
        error_map_.erase(ite);

        return result;
    }

    void egl_controller::push_error(egl_context *context, const std::uint32_t error) {
        if (!context) {
            return;
        }

        push_error(context->associated_thread_uid_, error);
    }

    std::uint32_t egl_controller::pop_error(egl_context *context) {
        if (!context) {
            return GL_NO_ERROR;
        }

        return pop_error(context->associated_thread_uid_);
    }
}
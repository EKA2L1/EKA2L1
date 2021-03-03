/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <dispatch/dispatcher.h>
#include <dispatch/register.h>
#include <dispatch/libraries/register.h>
#include <dispatch/screen.h>
#include <kernel/kernel.h>
#include <services/window/window.h>
#include <utils/event.h>
#include <utils/err.h>

#include <common/log.h>
#include <common/armemitter.h>
#include <system/epoc.h>

#include <mem/mem.h>

namespace eka2l1::dispatch {
    static std::uint32_t MAX_TRAMPOLINE_CHUNK_SIZE = 0x4000;

    dispatcher::dispatcher(kernel_system *kern, ntimer *timing)
        : winserv_(nullptr)
        , trampoline_chunk_(nullptr)
        , libmngr_(nullptr)
        , mem_(nullptr)
        , trampoline_allocated_(0) {
        trampoline_chunk_ = kern->create<kernel::chunk>(kern->get_memory_system(), nullptr, "DispatcherTrampolines", 0,
            MAX_TRAMPOLINE_CHUNK_SIZE, MAX_TRAMPOLINE_CHUNK_SIZE, prot_read_write_exec, kernel::chunk_type::normal,
            kernel::chunk_access::rom, kernel::chunk_attrib::none);

        winserv_ = reinterpret_cast<eka2l1::window_server *>(kern->get_by_name<service::server>(
            eka2l1::get_winserv_name_by_epocver(kern->get_epoc_version())));

        audren_sema_ = std::make_unique<dsp_epoc_audren_sema>();

        // Set global variables
        timing_ = timing;
        libmngr_ = kern->get_lib_manager();
        mem_ = kern->get_memory_system();
    }

    dispatcher::~dispatcher() {
        shutdown();
    }

    void dispatcher::resolve(eka2l1::system *sys, const std::uint32_t function_ord) {
        auto dispatch_find_result = dispatch::dispatch_funcs.find(function_ord);

        if (dispatch_find_result == dispatch::dispatch_funcs.end()) {
            LOG_ERROR(HLE_DISPATCHER, "Can't find dispatch function {}", function_ord);
            return;
        }

        dispatch_find_result->second(sys, sys->get_kernel_system()->crr_process(), sys->get_cpu());
    }

    void dispatcher::shutdown() {
    }

    void dispatcher::update_all_screens(eka2l1::system *sys) {
        epoc::screen *scr = winserv_->get_screens();

        while (scr != nullptr) {
            eka2l1::rect up_rect;
            up_rect.top = { 0, 0 };
            up_rect.size = scr->size();

            dispatch::update_screen(sys, 0, scr->number, 1, &up_rect);
            scr = scr->next;
        }
    }

    dsp_epoc_audren_sema *dispatcher::get_audren_sema() {
        return audren_sema_.get();
    }

    bool dispatcher::patch_libraries(const std::u16string &path, patch_info *patches,
        const std::size_t patch_count) {
        codeseg_ptr seg = libmngr_->load(path);

        if (!seg) {
            return false;
        }

        for (std::size_t i = 0; i < patch_count; i++) {
            const address orgaddr = seg->lookup_no_relocate(patches[i].ordinal_number_);
            if (!orgaddr) {
                continue;
            }
            
            const address entryentry = trampoline_chunk_->base(nullptr).ptr_address() + trampoline_allocated_;

            if (seg->is_rom()) {
                void *ptr = mem_->get_real_pointer(orgaddr & ~1);

                if (orgaddr & 1) {
                    std::memcpy(ptr, hle::THUMB_TRAMPOLINE_ASM, sizeof(hle::THUMB_TRAMPOLINE_ASM));

                    std::uint32_t offset_do_write = sizeof(hle::THUMB_TRAMPOLINE_ASM);

                    if (((orgaddr & ~1) & 3) == 0) {
                        offset_do_write -= 2;
                    }

                    ptr = reinterpret_cast<std::uint8_t*>(ptr) + offset_do_write;
                } else {
                    std::memcpy(ptr, hle::ARM_TRAMPOLINE_ASM, sizeof(hle::ARM_TRAMPOLINE_ASM));
                    ptr = reinterpret_cast<std::uint8_t*>(ptr) + sizeof(hle::ARM_TRAMPOLINE_ASM);
                }

                *reinterpret_cast<std::uint32_t*>(ptr) = entryentry;
            }

            std::uint32_t *start_base = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(
                trampoline_chunk_->host_base()) + trampoline_allocated_);
  
            start_base[0] = 0xEFC10001;
            start_base[1] = patches[i].dispatch_number_;

            // TODO!!! Export table is fixed as a whole, not as an individual, this is bad for HLEing only some functions!
            seg->set_export(patches[i].ordinal_number_, entryentry);

            trampoline_allocated_ += 8;
        }

        return true;
    }
}

namespace eka2l1::epoc {
    void dispatcher_do_resolve(eka2l1::system *sys, const std::uint32_t ordinal) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatcher->resolve(sys, ordinal);
    }

    void dispatcher_do_event_add(eka2l1::system *sys, epoc::raw_event &evt) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();

        switch (evt.type_) {
        case epoc::raw_event_type_redraw:
            dispatcher->update_all_screens(sys);
            break;

        default:
            LOG_WARN(HLE_DISPATCHER, "Unhandled raw event {}", static_cast<int>(evt.type_));
            break;
        }
    }
}
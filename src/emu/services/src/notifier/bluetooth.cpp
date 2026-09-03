/*
 * Copyright (c) 2026 EKA2L1 Team
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <services/notifier/bluetooth.h>

#include <kernel/kernel.h>
#include <services/bluetooth/btman.h>
#include <services/bluetooth/protocols/btmidman_inet.h>
#include <services/bluetooth/protocols/common.h>

#include <common/log.h>
#include <uvlooper/uvlooper.h>

#include <utils/err.h>

#include <array>
#include <cstring>

namespace eka2l1::epoc::notifier {
    namespace {
        constexpr std::size_t bt_device_response_size = 544;
        constexpr std::size_t bt_device_name_offset = 8;
        constexpr std::size_t bt_device_class_offset = 512;

        int write_selected_device(kernel_system *kern, epoc::des8 *response,
            epoc::notify_info &complete_info, const epoc::bt::device_address &selected_address) {
            std::array<std::uint8_t, bt_device_response_size> result{};
            std::memcpy(result.data(), &selected_address, sizeof(selected_address));

            constexpr std::u16string_view peer_name = u"EKA2L1 peer";
            const std::uint32_t name_info = (static_cast<std::uint32_t>(epoc::buf) << 28)
                | static_cast<std::uint32_t>(peer_name.size());
            const std::uint32_t name_capacity = 248;
            std::memcpy(result.data() + bt_device_name_offset, &name_info, sizeof(name_info));
            std::memcpy(result.data() + bt_device_name_offset + sizeof(name_info), &name_capacity, sizeof(name_capacity));
            std::memcpy(result.data() + bt_device_name_offset + sizeof(name_info) + sizeof(name_capacity),
                peer_name.data(), peer_name.size() * sizeof(char16_t));

            const std::uint32_t phone_device_class = 0x0200;
            const std::int32_t valid = 1;
            const std::size_t validity_offset = bt_device_class_offset + (kern->is_eka1() ? 4 : 12);
            std::memcpy(result.data() + bt_device_class_offset, &phone_device_class, sizeof(phone_device_class));
            std::memcpy(result.data() + validity_offset, &valid, sizeof(valid));
            std::memcpy(result.data() + validity_offset + sizeof(valid), &valid, sizeof(valid));
            std::memcpy(result.data() + validity_offset + sizeof(valid) * 2, &valid, sizeof(valid));

            kernel::process *requester = complete_info.requester->owning_process();
            return response->assign(requester, result.data(), result.size()) == 0
                ? epoc::error_none : epoc::error_overflow;
        }
    }

    bluetooth_device_selection_plugin::~bluetooth_device_selection_plugin() {
        cancel();
    }

    void bluetooth_device_selection_plugin::handle(epoc::desc8 *, epoc::des8 *response,
        epoc::notify_info &complete_info) {
        if (!response) {
            complete_info.complete(epoc::error_argument);
            return;
        }

        btman_server *btman = kern_->get_by_name<btman_server>(
            get_btman_server_name_by_epocver(kern_->get_epoc_version()));
        if (!btman || !btman->get_midman() || btman->get_midman()->type() != epoc::bt::MIDMAN_INET_BT) {
            complete_info.complete(epoc::error_not_supported);
            return;
        }

        auto *midman = static_cast<epoc::bt::midman_inet *>(btman->get_midman());

        // Netplay off: no discovery runs, so answer like a search that found nobody.
        if (midman->get_discovery_mode() == epoc::bt::DISCOVERY_MODE_OFF) {
            complete_info.complete(epoc::error_not_found);
            return;
        }

        epoc::bt::device_address selected_address{};
        if (midman->get_friend_device_address(0, selected_address)) {
            complete_info.complete(write_selected_device(kern_, response, complete_info, selected_address));
            return;
        }

        const std::shared_ptr<request_state> state = request_state_;
        std::lock_guard<std::mutex> guard(state->lock);
        if (state->response) {
            complete_info.complete(epoc::error_in_use);
            return;
        }

        state->midman = midman;
        state->response = response;
        state->complete_info = complete_info;
        state->generation.fetch_add(1, std::memory_order_acq_rel);
        midman->begin_hearing_stranger_call(this);
    }

    void bluetooth_device_selection_plugin::cancel() {
        const std::shared_ptr<request_state> state = request_state_;
        epoc::bt::midman_inet *midman = nullptr;
        epoc::notify_info complete_info;
        {
            std::lock_guard<std::mutex> guard(state->lock);
            state->generation.fetch_add(1, std::memory_order_acq_rel);
            midman = state->midman;
            complete_info = state->complete_info;
            state->midman = nullptr;
            state->response = nullptr;
            state->complete_info = epoc::notify_info();
        }

        if (midman) {
            midman->unregister_stranger_call_observer(this);
            complete_info.complete(epoc::error_cancel);
        }
    }

    void bluetooth_device_selection_plugin::on_stranger_call(epoc::socket::saddress &, std::uint32_t) {
        // The peer list is read from the midman once the search ends, so an
        // individual sighting needs no bookkeeping here.
    }

    void bluetooth_device_selection_plugin::on_no_more_strangers() {
        const std::shared_ptr<request_state> state = request_state_;
        const std::uint64_t generation = state->generation.load(std::memory_order_acquire);
        libuv::default_looper->one_shot([state, generation]() {
            if (generation != state->generation.load(std::memory_order_acquire)) {
                return;
            }

            epoc::bt::midman_inet *midman = nullptr;
            {
                std::lock_guard<std::mutex> guard(state->lock);
                midman = state->midman;
            }

            // Hearing no stranger does not mean there is no peer: direct IP takes
            // its peers from the config list and never announces them through the
            // observer, so a silent search there just means the friend's virtual
            // address is not resolved yet. Let the refresh below decide.
            if (!midman) {
                finish_request(state, generation);
                return;
            }

            epoc::bt::device_address selected_address{};
            if (midman->get_friend_device_address(0, selected_address)) {
                finish_request(state, generation);
                return;
            }

            midman->refresh_friend_infos_async([state, generation]() {
                finish_request(state, generation);
            });
        });
    }

    void bluetooth_device_selection_plugin::finish_request(const std::shared_ptr<request_state> &state,
        const std::uint64_t generation) {
        if (generation != state->generation.load(std::memory_order_acquire)) {
            return;
        }

        state->kern->lock();
        std::lock_guard<std::mutex> guard(state->lock);
        if ((generation != state->generation.load(std::memory_order_acquire)) || !state->response) {
            state->kern->unlock();
            return;
        }

        epoc::bt::device_address selected_address{};
        const bool found = state->midman && state->midman->get_friend_device_address(0, selected_address);
        const int result = found
            ? write_selected_device(state->kern, state->response, state->complete_info, selected_address)
            : epoc::error_not_found;

        state->midman = nullptr;
        state->response = nullptr;
        epoc::notify_info complete_info = state->complete_info;
        state->complete_info = epoc::notify_info();
        state->generation.fetch_add(1, std::memory_order_acq_rel);
        complete_info.complete(result);
        state->kern->unlock();
    }
}

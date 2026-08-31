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

#pragma once

#include <services/bluetooth/protocols/btmidman_inet.h>
#include <services/notifier/plugin.h>

#include <atomic>
#include <memory>
#include <mutex>

namespace eka2l1::epoc::notifier {
    class bluetooth_device_selection_plugin : public plugin_base, public epoc::bt::inet_stranger_call_observer {
        struct request_state {
            explicit request_state(kernel_system *kern)
                : kern(kern) {
            }

            kernel_system *kern;
            std::mutex lock;
            epoc::bt::midman_inet *midman = nullptr;
            epoc::des8 *response = nullptr;
            epoc::notify_info complete_info{};
            std::atomic<std::uint64_t> generation{ 0 };
        };

        std::shared_ptr<request_state> request_state_;
        static void finish_request(const std::shared_ptr<request_state> &state, std::uint64_t generation);

    public:
        explicit bluetooth_device_selection_plugin(kernel_system *kern)
            : plugin_base(kern)
            , request_state_(std::make_shared<request_state>(kern)) {
        }

        ~bluetooth_device_selection_plugin() override;

        epoc::uid unique_id() const override {
            return 0x100069D1;
        }

        void handle(epoc::desc8 *request, epoc::des8 *response, epoc::notify_info &complete_info) override;
        void cancel() override;

        void on_stranger_call(epoc::socket::saddress &addr, std::uint32_t index_in_list) override;
        void on_no_more_strangers() override;
    };
}

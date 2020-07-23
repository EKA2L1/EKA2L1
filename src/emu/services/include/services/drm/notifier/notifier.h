/*
 * Copyright (c) 2020 EKA2L1 Team
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

#pragma once

#include <services/drm/notifier/events.h>
#include <services/framework.h>

#include <utils/des.h>
#include <utils/reqsts.h>
#include <kernel/server.h>

#include <queue>
#include <vector>

namespace eka2l1 {
    enum drm_notifier_opcode {
        drm_notifier_send_events = 0x1,
        drm_notifier_listen_for_event = 0x2,
        drm_notifier_register_accept_event = 0x3,
        drm_notifier_unregister_accept_event = 0x4,
        drm_notifier_register_accept_event_with_uri = 0x5,
        drm_notifier_unregister_accept_event_with_uri = 0x6,
        drm_notifier_cancel_listen_for_event = 0xFF
    };

    struct drm_event_message {
        drm::notifier_event_type type_;
        std::string data_;

        drm_event_message *next_;
        std::uint32_t count_;

        explicit drm_event_message();
    };

    struct drm_accept_event_type {
        std::uint32_t event_type_;
        std::string content_uri_;
    };

    class drm_notifier_server : public service::typical_server {
        drm_event_message *storage_;

    public:
        explicit drm_notifier_server(eka2l1::system *sys);

        void connect(service::ipc_context &context) override;

        bool send_event(const std::uint32_t event, std::uint8_t *data);

        /**
         * @brief       Release a message from a session.
         * 
         * This deref the message. If the reference count is 0 this message will be removed from the storage.
         * 
         * @param       msg     The message to be released.
         * @returns     True on success.
         */
        bool release(drm_event_message *msg);
    };

    struct drm_notifier_client_session : public service::typical_session {
    private:
        std::queue<drm_event_message*> msgs_;

        epoc::notify_info notify_;
        std::uint8_t *to_write_data_;
        epoc::des8 *to_write_event_type_;

        std::vector<drm_accept_event_type> accept_types_;     ///< Type of events that this session will accept to be notified of

    protected:
        /**
         * @brief       Check if this event message can be accepted to this session.
         * 
         * A message can be accepted when its type and content ID is registered to be accepted in the session
         * before.
         * 
         * @param       msg         Message to check.
         * @returns     True if this session accepts the message.
         */
        bool can_accept(drm_event_message *msg);

        /**
         * @brief   Finish a pending event listen.
         * @param   msg         The event info to notify the listener.
         */
        void finish_listen(drm_event_message *msg);

        /**
         * @brief  Listen to incoming DRM events.
         * 
         * A session can only listen once a time. If a listen is pending, this function will not succeed.
         * 
         * @param   info                    The notify info to be listened.
         * @param   data_to_write           On notify, the event data is written to this descriptor.
         * @param   event_type_to_write     On notify, the event type is written to this descriptor.
         * 
         * @returns True on success.
         */
        bool listen(epoc::notify_info &info, std::uint8_t *data_to_write, epoc::des8 *event_type_to_write);

        // IPC opcodes implementation
        void listen_for_event(service::ipc_context *ctx);
        void cancel_listen_for_event(service::ipc_context *ctx);
        void send_event(service::ipc_context *ctx);
        void register_event(service::ipc_context *ctx);
        void unregister_event(service::ipc_context *ctx);
        void register_event_with_uri(service::ipc_context *ctx);
        void unregister_event_with_uri(service::ipc_context *ctx);

    public:
        explicit drm_notifier_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;

        /**
         * @param       Try to receive a DRM event from the server.
         * 
         * This message can only be received if its type is registered beforehand.
         * 
         * @param       msg     Message to be received.
         * @returns     True on success.
         */
        bool receive_event(drm_event_message *msg);

        /**
         * @brief       Register DRM event to be accepted to notify.
         * 
         * @param       type            The type of the event.
         * @param       uri             The content ID of the event.
         * 
         * @returns     True on success.
         */
        bool register_event(const std::uint32_t type, const std::string uri);

        /**
         * @brief       Unegister DRM event to be accepted to notify.
         * 
         * @param       type            The type of the event.
         * @param       uri             The content ID of the event.
         * 
         * @returns     True on success.
         */
        bool unregister_event(const std::uint32_t type, const std::string uri);
    };
}

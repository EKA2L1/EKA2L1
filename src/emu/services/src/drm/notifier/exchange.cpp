/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/drm/notifier/notifier.h>
#include <utils/err.h>

namespace eka2l1 {
    drm_event_message::drm_event_message()
        : type_(drm::notifier_event_type_none)
        , next_(nullptr)
        , count_(1) {
    }

    bool drm_notifier_server::release(drm_event_message *msg) {
        msg->count_--;

        if (msg->count_ > 0) {
            return true;
        }

        if (storage_ == msg) {
            storage_ = storage_->next_;
            delete msg;

            return true;
        }

        drm_event_message *begin = storage_;

        while (begin) {
            if (begin->next_ == msg) {
                break;
            }

            begin = begin->next_;
        }

        if (!begin) {
            return false;
        }

        begin->next_ = msg->next_;
        delete msg;

        return true;
    }

    bool drm_notifier_server::send_event(const std::uint32_t event, std::uint8_t *data) {
        drm_event_message *msg = new drm_event_message;
        msg->type_ = static_cast<drm::notifier_event_type>(event);

        std::unique_ptr<drm::notifier_event> event_obj;

        switch (msg->type_) {
        case drm::notifier_event_type_modify:
            event_obj = std::make_unique<drm::notifier_modify_event>();
            break;

        case drm::notifier_event_type_add_remove:
            event_obj = std::make_unique<drm::notifier_add_remove_event>();
            break;

        case drm::notifier_event_type_time_change:
            event_obj = std::make_unique<drm::notifier_time_change_event>();
            break;

        default:
            LOG_ERROR("Unidentified DRM event type to notify: {}", event);
            return false;
        }

        // TODO: No length :(
        common::ro_buf_stream parse_stream(data, 1000);
        if (!event_obj->internalize(parse_stream)) {
            return false;
        }

        std::optional<std::string> data_for_msg = event_obj->content_id();

        if (data_for_msg) {
            msg->data_ = std::move(data_for_msg.value());
        }

        // Push this message to storage
        if (!storage_) {
            storage_ = msg;
        } else {
            drm_event_message *ite = storage_;

            while (ite->next_ != nullptr) {
                ite = ite->next_;
            }

            ite->next_ = msg;
        }

        // Send to all sessions
        for (auto &session: sessions) {
            drm_notifier_client_session *ss = reinterpret_cast<drm_notifier_client_session*>(session.second.get());
            ss->receive_event(msg);
        }

        return true;
    }

    void drm_notifier_client_session::finish_listen(drm_event_message *msg) {
        to_write_event_type_->assign(notify_.requester->owning_process(), reinterpret_cast<std::uint8_t*>(&msg->type_),
            sizeof(msg->type_));

        std::memcpy(to_write_data_, msg->data_.data(), msg->data_.size());
        notify_.complete(epoc::error_none);

        // Ignore result
        server<drm_notifier_server>()->release(msg);
    }

    bool drm_notifier_client_session::listen(epoc::notify_info &info, std::uint8_t *data_to_write, epoc::des8 *event_type_to_write) {
        if (!notify_.empty()) {
            return false;
        }

        notify_ = info;
        to_write_data_ = data_to_write;
        to_write_event_type_ = event_type_to_write;

        if (!msgs_.empty()) {
            drm_event_message *msg = msgs_.front();
            msgs_.pop();

            finish_listen(msg);
        }

        return true;
    }

    bool drm_notifier_client_session::can_accept(drm_event_message *msg) {
        for (const drm_accept_event_type &accept: accept_types_) {
            // We can accept if event type of the registered is same as the message.
            // Content URI of the registered is empty then this condition is a free pass.
            if ((accept.event_type_ == msg->type_) && ((accept.content_uri_.empty()) || (msg->data_ == accept.content_uri_))) {
                return true;
            }
        }

        return false;
    }

    bool drm_notifier_client_session::receive_event(drm_event_message *msg) {
        if (!can_accept(msg)) {
            return false;
        }

        if (!notify_.empty()) {
            finish_listen(msg);
            return true;
        }

        msgs_.push(msg);
        return true;
    }
    
    bool drm_notifier_client_session::register_event(const std::uint32_t type, const std::string uri) {
        for (std::size_t i = 0; i < accept_types_.size(); i++) {
            if ((accept_types_[i].event_type_ == type) && (accept_types_[i].content_uri_ == uri)) {
                return false;
            }
        }

        accept_types_.push_back({ type, uri });
        return true;
    }

    bool drm_notifier_client_session::unregister_event(const std::uint32_t type, const std::string uri) {
        for (std::size_t i = 0; i < accept_types_.size(); i++) {
            if ((accept_types_[i].event_type_ == type) && (accept_types_[i].content_uri_ == uri)) {
                accept_types_.erase(accept_types_.begin() + i);
                return true;
            }
        }

        return false;
    }
}
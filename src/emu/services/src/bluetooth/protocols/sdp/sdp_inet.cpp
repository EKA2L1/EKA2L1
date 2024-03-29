/*
 * Copyright (c) 2022 EKA2L1 Team
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

#include <services/bluetooth/protocols/sdp/sdp_inet.h>
#include <services/bluetooth/protocols/btmidman_inet.h>
#include <common/log.h>
#include <common/bytes.h>
#include <kernel/thread.h>
#include <kernel/kernel.h>
#include <utils/err.h>

extern "C" {
#include <uv.h>
}

namespace eka2l1::epoc::bt {
    enum {
        error_sdp_peer_error = -6401,
        error_sdp_bad_result_data = -6410
    };

    inline constexpr std::uint8_t de_header(const std::uint8_t type, const std::uint8_t size_index) {
        return static_cast<std::uint8_t>((type << 3) | (size_index & 0x7));
    }

    sdp_inet_net_database::sdp_inet_net_database(sdp_inet_protocol *protocol, internet::inet_bridged_protocol *inet_pro)
        : inet_pro_(inet_pro)
        , protocol_(protocol)
        , bt_port_asker_(reinterpret_cast<midman_inet*>(protocol->get_midman()))
        , sdp_connect_(nullptr)
        , connected_(false)
        , provided_result_(nullptr)
        , store_to_temp_buffer_(false)
        , target_pdu_buffer_(nullptr)
        , target_pdu_buffer_size_(0) {
    }

    sdp_inet_net_database::~sdp_inet_net_database() {
        close_connect_handle(true);
    }

    void sdp_inet_net_database::close_connect_handle(const bool should_wait) {
        const std::lock_guard<std::mutex> guard(access_lock_);

        if (sdp_connect_) {
            inet_pro_->get_looper()->one_shot([sdp_connect_copy = sdp_connect_]() {
                sdp_connect_copy->reset<uvw::data_event>();
                sdp_connect_copy->reset<uvw::error_event>();
            });
        }
    }

    void sdp_inet_net_database::handle_normal_query_complete(const std::uint8_t *param, const std::uint32_t param_len) {
        if (current_query_notify_.empty()) {
            return;
        }

        kernel_system *kern = current_query_notify_.requester->get_kernel_object_owner();

        if (!provided_result_) {
            LOG_ERROR(SERVICE_BLUETOOTH, "No result buffer for SDP query!");

            kern->lock();
            current_query_notify_.complete(epoc::error_general);
            kern->unlock();

            return;
        }

        kernel::process *own_pr = current_query_notify_.requester->owning_process();
        std::uint32_t current_len = provided_result_->get_length();

        if (store_to_temp_buffer_) {
            stored_query_buffer_.resize(param_len);
            std::memcpy(stored_query_buffer_.data(), param, param_len);

            if (current_len != 4) {
                LOG_WARN(SERVICE_BLUETOOTH, "Query packet length buffer's size is incorrect");
                current_query_notify_.complete(epoc::error_argument);

                return;
            }

            provided_result_->assign(own_pr, reinterpret_cast<const std::uint8_t*>(&param_len), 4);
        } else {
            if (current_len < param_len) {
                LOG_WARN(SERVICE_BLUETOOTH, "Provided result buffer size is smaller then total query result length!");
                current_query_notify_.complete(epoc::error_argument);

                return;
            }

            provided_result_->assign(own_pr, param, param_len);
            provided_result_ = nullptr;
        }

        kern->lock();
        current_query_notify_.complete(epoc::error_none);
        kern->unlock();
    }

    void sdp_inet_net_database::handle_new_pdu_packet(const char *buffer, const std::int64_t nread) {
        if (nread <= 0) {
            if ((nread == UV_ECONNRESET) || (nread == UV_EOF) || (nread == 0) || (nread == -1)) {
                close_connect_handle();
            } else {
                LOG_ERROR(SERVICE_BLUETOOTH, "Receive SDP data failed with libuv error code {}", nread);
            }

            return;
        }

        if (nread < pdu_builder::PDU_HEADER_SIZE) {
            // Drop the invalid packet
            return;
        }
        
        std::uint8_t pdu_id = static_cast<std::uint8_t>(buffer[0]);
        std::uint16_t trans_id = 0;
        std::uint16_t param_len = 0;

        // For ARM T_T
        std::memcpy(&trans_id, buffer + pdu_builder::TRANS_ID_OFFSET, 2);
        std::memcpy(&param_len, buffer + pdu_builder::PARAM_LEN_OFFSET, 2);

        trans_id = common::to_host_order(trans_id);
        param_len = common::to_host_order(param_len);

        kernel_system *kern = current_query_notify_.requester->get_kernel_object_owner();

        if (trans_id != pdu_packet_builder_.current_transmission_id()) {
            LOG_TRACE(SERVICE_BLUETOOTH, "PDU packet mismatch transmission ID (got {}, expected {})", trans_id, pdu_packet_builder_.current_transmission_id());

            kern->lock();
            current_query_notify_.complete(error_sdp_bad_result_data);
            kern->unlock();

            return;
        }

        switch (pdu_id) {
        case SDP_PDU_ERROR_RESPONE: {
            if (param_len < 2) {
                LOG_TRACE(SERVICE_BLUETOOTH, "Error response has insufficent error data!");

                kern->lock();
                current_query_notify_.complete(error_sdp_peer_error);
                kern->unlock();

                return;
            }

            std::uint16_t error = 0;
            std::memcpy(&error, buffer + pdu_builder::PDU_HEADER_SIZE, 2);
            error = common::to_host_order(error);

            kern->lock();
            current_query_notify_.complete(error);
            kern->unlock();

            return;
        }

        case SDP_PDU_SERVICE_ATTRIBUTE_RESPONSE:
        case SDP_PDU_SERVICE_SEARCH_RESPONSE:
            handle_normal_query_complete(reinterpret_cast<const std::uint8_t*>(buffer + pdu_builder::PDU_HEADER_SIZE), param_len);
            return;

        default:
            kern->lock();
            current_query_notify_.complete(error_sdp_bad_result_data);
            kern->unlock();

            return;
        }
    }

    void sdp_inet_net_database::handle_connect_done(const int status) {
        if (current_query_notify_.empty()) {
            return;
        }

        kernel_system *kern = current_query_notify_.requester->get_kernel_object_owner();

        if (status < 0) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Connect to SDP server failed with libuv error code {}", status);

            kern->lock();
            current_query_notify_.complete(epoc::error_could_not_connect);
            kern->unlock();

            return;
        }

        sdp_connect_->on<uvw::error_event>([this](const uvw::error_event &event, uvw::tcp_handle &handle) {
            handle_new_pdu_packet(nullptr, event.code());
        });

        sdp_connect_->on<uvw::data_event>([this](const uvw::data_event &event, uvw::tcp_handle &handle) {
            handle_new_pdu_packet(event.data.get(), static_cast<std::int64_t>(event.length));
        });

        sdp_connect_->read();

        kern->lock();
        current_query_notify_.complete(epoc::error_none);
        kern->unlock();
    }

    void sdp_inet_net_database::handle_connect_query(const char *record_buf, const std::uint32_t record_size) {
        const sdp_connect_query *query = reinterpret_cast<const sdp_connect_query*>(record_buf);
        midman_inet *midman = reinterpret_cast<midman_inet*>(protocol_->get_midman());

        epoc::socket::saddress friend_addr_real;
        if (!midman->get_friend_address(query->addr_, friend_addr_real)) {
            current_query_notify_.complete(epoc::error_could_not_connect);
            return;
        }

        bt_port_asker_.ask_for_routed_port_async(1, friend_addr_real, [this, friend_addr_real](std::int64_t port_result) {
            if (port_result <= 0) {
                handle_connect_done(static_cast<int>(port_result));
                return;
            } else {
                // Establish TCP connect
                if (!sdp_connect_) {
                    sdp_connect_ = uvw::loop::get_default()->resource<uvw::tcp_handle>();
                }

                epoc::socket::saddress dest_friend = friend_addr_real;
                dest_friend.port_ = static_cast<std::uint32_t>(port_result);

                sockaddr *addr_translated = nullptr;
                GUEST_TO_BSD_ADDR(dest_friend, addr_translated);

                sockaddr_in6 bind_any;
                std::memset(&bind_any, 0, sizeof(sockaddr_in6));
                bind_any.sin6_family = AF_INET6;

                sdp_connect_->bind(*reinterpret_cast<const sockaddr *>(&bind_any));

                sdp_connect_->on<uvw::connect_event>([this](const uvw::connect_event &event, uvw::tcp_handle &handle) {
                    handle_connect_done(0);
                });
                
                sdp_connect_->on<uvw::error_event>([this](const uvw::error_event &event, uvw::tcp_handle &handle) {
                    handle_connect_done(event.code());
                });

                sdp_connect_->connect(*addr_translated);
            }
        });
    }

    void sdp_inet_net_database::handle_send_done(const int status) {
        // Re-register new pdu packet error event
        sdp_connect_->on<uvw::error_event>([this](const uvw::error_event &event, uvw::tcp_handle &handle) {
            handle_new_pdu_packet(nullptr, event.code());
        });

        if (status < 0) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Sending PDU packet to SDP server failed with libuv error code {}", status);

            kernel_system *kern = current_query_notify_.requester->get_kernel_object_owner();

            kern->lock();
            current_query_notify_.complete(epoc::error_server_busy);
            kern->unlock();

            return;
        }
    }

    void sdp_inet_net_database::send_pdu_packet(const char *buf, const std::uint32_t buf_size) {
        target_pdu_buffer_ = buf;
        target_pdu_buffer_size_ = buf_size;

        if (!send_pdu_packet_task_) {
            send_pdu_packet_task_ = libuv::create_task([this]() {
                // Re-register new pdu packet error event
                sdp_connect_->on<uvw::error_event>([this](const uvw::error_event &event, uvw::tcp_handle &handle) {
                    handle_send_done(event.code());
                });

                sdp_connect_->on<uvw::write_event>([this](const uvw::write_event &event, uvw::tcp_handle &handle) {
                    handle_send_done(0);
                });

                sdp_connect_->write(const_cast<char*>(target_pdu_buffer_), target_pdu_buffer_size_);
            });
        }

        inet_pro_->get_looper()->post_task(send_pdu_packet_task_);
    }

    void sdp_inet_net_database::handle_service_query(const char *record_buf, const std::uint32_t record_size) {
        pdu_packet_builder_.new_packet();
        pdu_packet_builder_.set_pdu_id(SDP_PDU_SERVICE_SEARCH_REQUEST);
        pdu_packet_builder_.put_byte(de_header(6, 5));      // DES header

        const sdp_service_query *query_base_all = reinterpret_cast<const sdp_service_query*>(record_buf);
        std::uint32_t shortest_size = 0;
        std::uint8_t sz_index = 0;

        const std::uint8_t *shortest_data_ptr = query_base_all->uuid_.shortest_form(shortest_size);
        std::uint32_t shortest_size_copy = shortest_size;

        while (shortest_size_copy >>= 1) {
            sz_index++;
        }

        pdu_packet_builder_.put_byte(static_cast<std::uint8_t>(shortest_size + 1));
        pdu_packet_builder_.put_byte(de_header(3, sz_index));
        pdu_packet_builder_.put_data(reinterpret_cast<const char*>(shortest_data_ptr), shortest_size);
        pdu_packet_builder_.put_be16(query_base_all->max_record_return_count_);

        if (record_size == sizeof(sdp_service_query_new)) {   
            const sdp_service_query_new *query_base_all_new = reinterpret_cast<const sdp_service_query_new*>(record_buf);
            pdu_packet_builder_.put_byte(static_cast<std::uint8_t>(query_base_all_new->state_length_));
            pdu_packet_builder_.put_data(reinterpret_cast<const char*>(query_base_all_new->continuation_state_), query_base_all_new->state_length_);
        } else {
            pdu_packet_builder_.put_byte(static_cast<std::uint8_t>(query_base_all->state_length_));
            pdu_packet_builder_.put_data(reinterpret_cast<const char*>(query_base_all->continuation_state_), query_base_all->state_length_);
        }

        std::uint32_t total_packet_size = 0;
        const char *packet = pdu_packet_builder_.finalize(total_packet_size);

        send_pdu_packet(packet, total_packet_size);
    }
    
    void sdp_inet_net_database::handle_encoded_query(const char *record_buf, const std::uint32_t record_size) {
        pdu_packet_builder_.new_packet();

        const sdp_encoded_query *query_base_all = reinterpret_cast<const sdp_encoded_query*>(record_buf);
        pdu_packet_builder_.set_pdu_id(query_base_all->pdu_id_);
        pdu_packet_builder_.put_data(record_buf + sizeof(sdp_encoded_query), record_size - sizeof(sdp_encoded_query));

        std::uint32_t total_packet_size = 0;
        const char *packet = pdu_packet_builder_.finalize(total_packet_size);

        send_pdu_packet(packet, total_packet_size);
    }

    void sdp_inet_net_database::handle_retrieve_buffer_query() {
        if (provided_result_->get_length() < stored_query_buffer_.size()) {
            LOG_WARN(SERVICE_BLUETOOTH, "Provided result buffer size is smaller then total query result length!");
            current_query_notify_.complete(epoc::error_argument);

            return;
        }

        provided_result_->assign(current_query_notify_.requester->owning_process(), reinterpret_cast<std::uint8_t*>(stored_query_buffer_.data()), 
            static_cast<std::uint32_t>(stored_query_buffer_.size()));

        provided_result_ = nullptr;
        current_query_notify_.complete(epoc::error_none);
    }

    void sdp_inet_net_database::query(const char *query_data, const std::uint32_t query_size, epoc::des8 *result_buffer, epoc::notify_info &complete_info) {
        if (!query_data) {
            complete_info.complete(epoc::error_argument);
            return;
        }

        if (!current_query_notify_.empty()) {
            complete_info.complete(epoc::error_server_busy);
            return;
        }

        current_query_notify_ = complete_info;
        provided_result_ = result_buffer;

        const std::uint32_t pdu_id = *reinterpret_cast<const std::uint32_t*>(query_data);
        store_to_temp_buffer_ = (pdu_id & SDP_QUERY_STORE_TO_NETDB_BUFFER_FIRST_FLAG);

        switch (pdu_id & SDP_QUERY_TYPE_MASK) {
        case SDP_QUERY_CONNECT:
            handle_connect_query(query_data, query_size);
            break;

        case SDP_QUERY_SERVICE:
            handle_service_query(query_data, query_size);
            break;

        case SDP_QUERY_ENCODED:
            handle_encoded_query(query_data, query_size);
            break;

        case SDP_QUERY_RETRIEVE_RESULT_BUFFER:
            handle_retrieve_buffer_query();
            break;

        default:
            LOG_ERROR(SERVICE_BLUETOOTH, "Unhandled SDP NetDatabase query 0x{:X}", pdu_id & SDP_QUERY_TYPE_MASK);
            break;
        }
    }

    void sdp_inet_net_database::add(const char *record_buf, const std::uint32_t record_size, epoc::notify_info &complete_info) {
        LOG_TRACE(SERVICE_BLUETOOTH, "SDP's NetDatabase does not support adding!");
        complete_info.complete(epoc::error_not_supported);
    }

    void sdp_inet_net_database::remove(const char *record_buf, const std::uint32_t record_size, epoc::notify_info &complete_info) {
        LOG_TRACE(SERVICE_BLUETOOTH, "SDP's NetDatabase does not support remove!");
        complete_info.complete(epoc::error_not_supported);
    }

    void sdp_inet_net_database::cancel() {
        provided_result_ = nullptr;
        current_query_notify_.complete(epoc::error_cancel);
    }
}

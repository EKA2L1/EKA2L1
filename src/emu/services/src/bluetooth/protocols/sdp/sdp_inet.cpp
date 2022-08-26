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

    sdp_inet_net_database::sdp_inet_net_database(sdp_inet_protocol *protocol)
        : protocol_(protocol)
        , sdp_connect_(nullptr)
        , connected_(false)
        , provided_result_(nullptr)
        , store_to_temp_buffer_(false)
        , should_notify_done_(false) {
    }

    sdp_inet_net_database::~sdp_inet_net_database() {
        close_connect_handle(true);
    }

    void sdp_inet_net_database::close_connect_handle(const bool should_wait) {
        const std::lock_guard<std::mutex> guard(access_lock_);

        if (sdp_connect_) {
            if (should_wait) {
                close_done_evt_.reset();
                should_notify_done_ = true;
            } else {
                should_notify_done_ = false;
            }

            uv_async_t *async = new uv_async_t;
            async->data = sdp_connect_;

            // While shutdown it calls EOF again
            sdp_connect_ = nullptr;

            uv_async_init(uv_default_loop(), async, [](uv_async_t *close_async) {
                uv_stream_t *h = reinterpret_cast<uv_stream_t*>(close_async->data);
                uv_shutdown_t *shut = new uv_shutdown_t;

                uv_shutdown(shut, h, [](uv_shutdown_t *shut, int status) {
                    uv_close(reinterpret_cast<uv_handle_t*>(shut->handle), [](uv_handle_t *handle) {
                        sdp_inet_net_database *sdp = reinterpret_cast<sdp_inet_net_database*>(handle->data);
                        if (sdp->should_notify_done_) {
                            sdp->close_done_evt_.set();
                        }
                        delete handle;
                    });

                    delete shut;
                });

                uv_close(reinterpret_cast<uv_handle_t*>(close_async), [](uv_handle_t *hh) { delete hh; });
            });

            uv_async_send(async);

            if (should_wait) {
                close_done_evt_.wait();
            }
        }
    }

    char *sdp_inet_net_database::prepare_read_buffer(const std::size_t suggested_size) {
        if (pdu_response_buffer_.size() < suggested_size) {
            pdu_response_buffer_.resize(suggested_size);
        }

        return pdu_response_buffer_.data();
    }

    void sdp_inet_net_database::handle_normal_query_complete(const std::uint8_t *param, const std::uint32_t param_len) {
        if (current_query_notify_.empty()) {
            return;
        }

        if (!provided_result_) {
            LOG_ERROR(SERVICE_BLUETOOTH, "No result buffer for SDP query!");
            current_query_notify_.complete(epoc::error_general);
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

        current_query_notify_.complete(epoc::error_none);
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

        if (trans_id != pdu_packet_builder_.current_transmission_id()) {
            LOG_TRACE(SERVICE_BLUETOOTH, "PDU packet mismatch transmission ID (got {}, expected {})", trans_id, pdu_packet_builder_.current_transmission_id());
            current_query_notify_.complete(error_sdp_bad_result_data);

            return;
        }

        switch (pdu_id) {
        case SDP_PDU_ERROR_RESPONE: {
            if (param_len < 2) {
                LOG_TRACE(SERVICE_BLUETOOTH, "Error response has insufficent error data!");
                current_query_notify_.complete(error_sdp_peer_error);
                return;
            }

            std::uint16_t error = 0;
            std::memcpy(&error, buffer + pdu_builder::PDU_HEADER_SIZE, 2);
            error = common::to_host_order(error);

            current_query_notify_.complete(error);
            return;
        }

        case SDP_PDU_SERVICE_ATTRIBUTE_RESPONSE:
        case SDP_PDU_SERVICE_SEARCH_RESPONSE:
            handle_normal_query_complete(reinterpret_cast<const std::uint8_t*>(buffer + pdu_builder::PDU_HEADER_SIZE), param_len);
            return;

        default:
            current_query_notify_.complete(error_sdp_bad_result_data);
            return;
        }
    }

    void sdp_inet_net_database::handle_connect_done(const int status) {
        if (status < 0) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Connect to SDP server failed with libuv error code {}", status);
            current_query_notify_.complete(epoc::error_could_not_connect);

            return;
        }

        uv_read_start(reinterpret_cast<uv_stream_t*>(sdp_connect_), [](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
            sdp_inet_net_database *ndb = reinterpret_cast<sdp_inet_net_database*>(handle->data);
            buf->base = ndb->prepare_read_buffer(suggested_size);
            buf->len = static_cast<std::uint32_t>(suggested_size);
        }, [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
            sdp_inet_net_database *ndb = reinterpret_cast<sdp_inet_net_database*>(stream->data);
            ndb->handle_new_pdu_packet(buf->base, nread);
        });

        current_query_notify_.complete(epoc::error_none);
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
                    uv_tcp_t *sdp_connect_impl = new uv_tcp_t;
                    uv_tcp_init(uv_default_loop(), sdp_connect_impl);
                    
                    sdp_connect_impl->data = this;
                    sdp_connect_ = sdp_connect_impl;
                }

                uv_async_t *async_connect = new uv_async_t;
                struct async_sdp_connect_data {
                    epoc::socket::saddress addr_;
                    uv_tcp_t *tcp_;
                    sdp_inet_net_database *self_;
                };

                async_sdp_connect_data *data = new async_sdp_connect_data;
                data->addr_ = friend_addr_real;
                data->addr_.port_ = static_cast<std::uint32_t>(port_result);
                data->tcp_ = reinterpret_cast<uv_tcp_t*>(sdp_connect_);
                data->self_ = this;

                async_connect->data = data;

                uv_async_init(uv_default_loop(), async_connect, [](uv_async_t *async) {
                    async_sdp_connect_data *data = reinterpret_cast<async_sdp_connect_data*>(async->data);
                    uv_connect_t *conn = new uv_connect_t;
                    conn->data = data;

                    sockaddr *addr_translated = nullptr;
                    GUEST_TO_BSD_ADDR(data->addr_, addr_translated);

                    sockaddr_in6 bind_any;
                    std::memset(&bind_any, 0, sizeof(sockaddr_in6));
                    bind_any.sin6_family = AF_INET6;

                    uv_tcp_bind(data->tcp_, reinterpret_cast<const sockaddr*>(&bind_any), 0);
                    uv_tcp_connect(conn, data->tcp_, addr_translated, [](uv_connect_t *req, int status) {
                        async_sdp_connect_data *data = reinterpret_cast<async_sdp_connect_data*>(req->data);
                        data->self_->handle_connect_done(status);

                        delete data;
                        delete req;
                    });

                    uv_close(reinterpret_cast<uv_handle_t*>(async), [](uv_handle_t *hh) { delete hh; });
                });

                uv_async_send(async_connect);
            }
        });
    }

    void sdp_inet_net_database::handle_send_done(const int status) {
        if (status < 0) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Sending PDU packet to SDP server failed with libuv error code {}", status);
            current_query_notify_.complete(epoc::error_server_busy);

            return;
        }
    }

    void sdp_inet_net_database::send_pdu_packet(const char *buf, const std::uint32_t buf_size) {
        struct send_pdu_packet_data {
            char *buf_copy_;
            std::uint32_t buf_size_;
            uv_tcp_t *tcp_handle_;
            sdp_inet_net_database *self_;
        };

        send_pdu_packet_data *info_data = new send_pdu_packet_data;
        info_data->buf_copy_ = reinterpret_cast<char*>(malloc(buf_size));
        info_data->buf_size_ = buf_size;
        info_data->tcp_handle_ = reinterpret_cast<uv_tcp_t*>(sdp_connect_);
        info_data->self_ = this;

        std::memcpy(info_data->buf_copy_, buf, buf_size);

        uv_async_t *async = new uv_async_t;
        async->data = info_data;

        uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
            send_pdu_packet_data *info_data = reinterpret_cast<send_pdu_packet_data*>(async->data);
            uv_buf_t buf = uv_buf_init(info_data->buf_copy_, info_data->buf_size_);

            uv_write_t *write_req = new uv_write_t;
            write_req->data = info_data;

            uv_write(write_req, reinterpret_cast<uv_stream_t*>(info_data->tcp_handle_), &buf, 1, [](uv_write_t *write_req, const int status) {
                send_pdu_packet_data *info_data = reinterpret_cast<send_pdu_packet_data*>(write_req->data);
                info_data->self_->handle_send_done(status);

                free(info_data->buf_copy_);
                delete info_data;
                delete write_req;
            });

            uv_close(reinterpret_cast<uv_handle_t*>(async), [](uv_handle_t *hh) { delete hh; });
        });

        uv_async_send(async);
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

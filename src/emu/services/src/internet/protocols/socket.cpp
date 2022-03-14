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

#include <services/internet/protocols/inet.h>
#include <common/platform.h>
#include <utils/err.h>
#include <kernel/kernel.h>
#include <common/thread.h>

extern "C" {
#include <uv.h>
}

namespace eka2l1::epoc::internet {    
    void inet_bridged_protocol::initialize_looper() {
        if (!loop_thread_) {
            loop_thread_ = std::make_unique<std::thread>([&]() {
                common::set_thread_name("UV socket looper thread");

                while (uv_run(uv_default_loop(), UV_RUN_DEFAULT) == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }

                uv_loop_close(uv_default_loop());
            });
        }
    }

    inet_bridged_protocol::~inet_bridged_protocol() {
        if (loop_thread_) {
            uv_async_t *async = new uv_async_t;
            uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
                uv_stop(uv_default_loop());
                delete async;
            });

            uv_async_send(async);
            loop_thread_->join();
        }
    }

    std::unique_ptr<epoc::socket::socket> inet_bridged_protocol::make_socket(const std::uint32_t family_id, const std::uint32_t protocol_id, const socket::socket_type sock_type) {
        std::unique_ptr<epoc::socket::socket> sock = std::make_unique<inet_socket>(this);
        inet_socket *sock_casted = reinterpret_cast<inet_socket*>(sock.get());

        if (!sock_casted->open(family_id, protocol_id, sock_type)) {
            return nullptr;
        }

        return sock;
    }

    void inet_socket::set_exit_event() {
        exit_event_.set();
    }

    void inet_socket::close_down() {
        if (opaque_handle_) {
            uv_async_t *async = new uv_async_t;
            async->data = this;

            uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
                inet_socket *sock = reinterpret_cast<inet_socket*>(async->data);
                uv_handle_t *handle = reinterpret_cast<uv_handle_t*>(sock->get_opaque_handle());
                handle->data = sock;

                uv_close(handle, [](uv_handle_t *handle) {
                    reinterpret_cast<inet_socket*>(handle->data)->set_exit_event();
                });

                delete async;
            });

            uv_async_send(async);
            exit_event_.wait();

            // Delete the stored data
            std::uint8_t *opaque_handle_casted = reinterpret_cast<std::uint8_t*>(opaque_handle_);
            delete opaque_handle_casted;

            opaque_handle_ = nullptr;
            protocol_ = 0;
        }

        if (opaque_connect_) {
            uv_connect_t *connect = reinterpret_cast<uv_connect_t*>(opaque_connect_);
            delete connect;

            opaque_connect_ = nullptr;
        }

        if (opaque_send_info_) {
            uv_udp_send_t *send = reinterpret_cast<uv_udp_send_t*>(opaque_send_info_);
            delete send;

            opaque_send_info_ = nullptr;
        }

        if (opaque_write_info_) {
            uv_write_t *write = reinterpret_cast<uv_write_t*>(opaque_write_info_);
            delete write;

            opaque_write_info_ = nullptr;
        }
    }

    inet_socket::~inet_socket() {
        close_down();
    }

    bool inet_socket::open(const std::uint32_t family_id, const std::uint32_t protocol_id, const epoc::socket::socket_type sock_type) {
        const int family_translated = ((family_id == INET6_ADDRESS_FAMILY) ? AF_INET6 : AF_INET);

        switch (sock_type) {
        case epoc::socket::socket_type_datagram:
            if (protocol_id != INET_UDP_PROTOCOL_ID) {
                LOG_ERROR(SERVICE_INTERNET, "Datagram socket must use UDP protocol on emulator at the moment!");
                return false;
            }

            break;

        case epoc::socket::socket_type_stream:
            if (protocol_id != INET_TCP_PROTOCOL_ID) {
                LOG_ERROR(SERVICE_INTERNET, "Stream socket must use TCP protocol on emulator at the moment!");
                return false;
            }

            break;

        default:
            LOG_ERROR(SERVICE_INTERNET, "Unrecognisable socket type to be created (value={})", static_cast<int>(sock_type));
            return false;
        }

        if (opaque_handle_) {
            LOG_ERROR(SERVICE_INTERNET, "Socket has already been opened. Please close it!");
            return false;
        }

        uv_async_t *async = new uv_async_t;
        open_event_.reset();

        struct uv_sock_init_params {
            void *opaque_handle_;
            int result_ = 0;
            common::event *done_evt_ = nullptr;
        };

        uv_sock_init_params params;
        params.done_evt_ = &open_event_;

        async->data = &params;

        if (protocol_id == INET_TCP_PROTOCOL_ID) {
            opaque_handle_ = new uv_tcp_t;
            params.opaque_handle_ = opaque_handle_;

            uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
                uv_sock_init_params *params = reinterpret_cast<uv_sock_init_params*>(async->data);
                params->result_ = uv_tcp_init(uv_default_loop(), reinterpret_cast<uv_tcp_t*>(params->opaque_handle_));
                params->done_evt_->set();

                delete async;
            });
        } else {
            opaque_handle_ = new uv_udp_t;
            params.opaque_handle_ = opaque_handle_;

            uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
                uv_sock_init_params *params = reinterpret_cast<uv_sock_init_params*>(async->data);
                params->result_ = uv_udp_init(uv_default_loop(), reinterpret_cast<uv_udp_t*>(params->opaque_handle_));
                params->done_evt_->set();

                delete async;
            });
        }

        uv_async_send(async);

        // Start the looper now, we might have the first customer!
        papa_->initialize_looper();
        exit_event_.reset();

        open_event_.wait();

        if (params.result_ < 0) {
            LOG_ERROR(SERVICE_INTERNET, "Socket failed to be initialize, error code={}", errno);
            return false;
        }

        protocol_ = protocol_id;
        return true;
    }

    void inet_socket::handle_connect_done_error_code(const int error_code) {
        if (connect_done_info_.empty()) {
            return;
        }

        if (error_code == 0) {
            connect_done_info_.complete(epoc::error_none);
        } else {
            LOG_ERROR(SERVICE_INTERNET, "Connect through libuv failed with code {}", error_code);

            int guest_error_code = 0;
            switch (error_code) {
            case UV_EACCES:
                guest_error_code = epoc::error_permission_denied;
                break;

            case UV_EADDRINUSE:
                guest_error_code = epoc::error_in_use;
                break;

            case UV_EADDRNOTAVAIL:
                guest_error_code = epoc::error_argument;
                break;

            case UV_EAFNOSUPPORT:
                guest_error_code = epoc::error_not_supported;
                break;

            case UV_ECONNREFUSED:
                guest_error_code = epoc::error_server_busy;
                break;

            case UV_ENOTSUP:
                guest_error_code = epoc::error_not_supported;
                break;

            default:
                guest_error_code = epoc::error_general;
                break;
            }

            connect_done_info_.complete(guest_error_code);
        }
    }

#define GUEST_TO_BSD_ADDR(addr, dest_ptr)                                               \
    sockaddr_in ipv4_addr;                                                              \
    sockaddr_in6 ipv6_addr;                                                             \
    if (addr.family_ == INET_ADDRESS_FAMILY) {                                          \
        ipv4_addr.sin_family = AF_INET;                                                 \
        ipv4_addr.sin_port = htons(static_cast<std::uint16_t>(addr.port_));             \
        std::memcpy(&ipv4_addr.sin_addr, addr.user_data_, 4);                           \
        dest_ptr = reinterpret_cast<sockaddr*>(&ipv4_addr);                             \
    } else {                                                                            \
        ipv6_addr.sin6_family = AF_INET6;                                               \
        ipv6_addr.sin6_port = htons(static_cast<std::uint16_t>(addr.port_));            \
        const sinet6_address &ipv6_guest = static_cast<const sinet6_address&>(addr);    \
        ipv6_addr.sin6_flowinfo = ipv6_guest.get_flow();                                \
        ipv6_addr.sin6_scope_id = ipv6_guest.get_scope();                               \
        std::memcpy(&ipv6_addr.sin6_addr, ipv6_guest.get_address_32x4(), 16);           \
        dest_ptr = reinterpret_cast<sockaddr*>(&ipv6_addr);                             \
    }

    void inet_socket::complete_connect_done_info(const int err) {
        if (connect_done_info_.empty()) {
            return;
        }

        handle_connect_done_error_code(err);
    }

    void inet_socket::connect(const epoc::socket::saddress &addr, epoc::notify_info &info) {
        if (!opaque_handle_) {
            info.complete(epoc::error_not_ready);
            return;
        }

        if (!connect_done_info_.empty()) {
            info.complete(epoc::error_in_use);
            return;
        }

        sockaddr *ip_addr_ptr = nullptr;
        GUEST_TO_BSD_ADDR(addr, ip_addr_ptr);

        connect_done_info_ = info;

        uv_async_t *async = new uv_async_t;

        if (protocol_ == INET_UDP_PROTOCOL_ID) {
            struct uv_udp_connect_params {
                inet_socket *parent_;
                sockaddr_in6 addr_;
                uv_udp_t *handle_;
            };

            uv_udp_connect_params *params = new uv_udp_connect_params;
            params->parent_ = this;
            params->handle_ = reinterpret_cast<uv_udp_t*>(opaque_handle_);

            std::memcpy(&params->addr_, ip_addr_ptr, sizeof(sockaddr_in6));
            async->data = params;

            uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
                uv_udp_connect_params *params = reinterpret_cast<uv_udp_connect_params*>(async->data);
    
                const int err = uv_udp_connect(params->handle_, reinterpret_cast<const sockaddr*>(&params->addr_));
                reinterpret_cast<inet_socket*>(params->parent_)->complete_connect_done_info(err);

                delete params;
                delete async;
            });
        } else {
            struct uv_tcp_connect_params {
                sockaddr_in6 addr_;
                uv_connect_t *connect_;
                uv_tcp_t *tcp_;
            };

            uv_tcp_t *handle_tcp = reinterpret_cast<uv_tcp_t*>(opaque_handle_);
            handle_tcp->data = this;

            if (!opaque_connect_) {
                opaque_connect_ = new uv_connect_t;
                reinterpret_cast<uv_connect_t*>(opaque_connect_)->data = this;
            }

            uv_tcp_connect_params *params = new uv_tcp_connect_params;
            params->connect_ = reinterpret_cast<uv_connect_t*>(opaque_connect_);
            params->tcp_ = handle_tcp;
            
            std::memcpy(&params->addr_, ip_addr_ptr, sizeof(sockaddr_in6));
            async->data = params;

            uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
                uv_tcp_connect_params *params = reinterpret_cast<uv_tcp_connect_params*>(async->data);

                uv_tcp_connect(params->connect_, params->tcp_, reinterpret_cast<const sockaddr*>(&params->addr_), [](uv_connect_t *connect, const int err) {
                    reinterpret_cast<inet_socket*>(connect->data)->complete_connect_done_info(err);
                });

                delete params;
                delete async;
            });
        }

        uv_async_send(async);
    }

    void inet_socket::complete_send_done_info(const int err) {
        kernel_system *kern = nullptr;
        if (!send_done_info_.empty()) {
            kern = send_done_info_.requester->get_kernel_object_owner();
        } else {
            return;
        }

        kern->lock();

        if (err != 0) {
            LOG_ERROR(SERVICE_INTERNET, "Send failed with UV error code {}, please handle it!", err);
            send_done_info_.complete(epoc::error_general);
        } else {
            send_done_info_.complete(epoc::error_none);
        }

        bytes_written_ = nullptr;
        kern->unlock();
    }

    void inet_socket::send(const std::uint8_t *data, std::uint32_t data_size, std::uint32_t *sent_size, const epoc::socket::saddress *addr_ptr, std::uint32_t flags, epoc::notify_info &complete_info) {
        if (!send_done_info_.empty()) {
            complete_info.complete(epoc::error_in_use);
            return;
        }

        sockaddr *ip_addr_ptr = nullptr;
        if (addr_ptr != nullptr) {
            GUEST_TO_BSD_ADDR((*addr_ptr), ip_addr_ptr);
        }

        bytes_written_ = sent_size;

        if (bytes_written_) {
            // Callback done then it's fully written. Else the cancel will rewrite this value
            // This assumes the pointer is still valid until then :o
            *bytes_written_ = data_size;
        }

        send_done_info_ = complete_info;

        if (flags != 0) {
            LOG_TRACE(SERVICE_INTERNET, "Send data with non-zero flags, please notice! (flag={})", flags);
        }

        if (protocol_ == INET_UDP_PROTOCOL_ID) {
            if (!opaque_send_info_) {
                opaque_send_info_ = new uv_udp_send_t;
            }

            struct uv_udp_send_task_info {
                uv_buf_t buf_sent_;
                uv_udp_send_t *send_;
                uv_udp_t *udp_;
                sockaddr_in6 *addr_ = nullptr;
            };

            uv_udp_t *udp_handle = reinterpret_cast<uv_udp_t*>(opaque_handle_);
            uv_udp_send_t *send_info_ptr = reinterpret_cast<uv_udp_send_t*>(opaque_send_info_);

            uv_udp_send_task_info *task_info = new uv_udp_send_task_info;
            task_info->buf_sent_ = uv_buf_init(const_cast<char*>(reinterpret_cast<const char*>(data)), static_cast<std::uint32_t>(data_size));
            task_info->send_ = send_info_ptr;
            task_info->udp_ = udp_handle;

            if (ip_addr_ptr) {
                task_info->addr_ = new sockaddr_in6;
                std::memcpy(task_info->addr_, ip_addr_ptr, sizeof(sockaddr_in6));
            }

            send_info_ptr->data = this;

            uv_async_t *async = new uv_async_t;
            async->data = task_info;

            uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
                uv_udp_send_task_info *task_info = reinterpret_cast<uv_udp_send_task_info*>(async->data);
                uv_udp_send(task_info->send_, task_info->udp_, &task_info->buf_sent_, 1, reinterpret_cast<const sockaddr*>(task_info->addr_), [](uv_udp_send_t *send_info, int status) {
                    reinterpret_cast<inet_socket*>(send_info->data)->complete_send_done_info(status);
                });

                if (task_info->addr_) {
                    delete task_info->addr_;
                }

                delete task_info;
                delete async;
            });

            uv_async_send(async);
        } else {
            // Address is not important here.
            if (!opaque_write_info_) {
                opaque_write_info_ = new uv_write_t;
            }

            struct uv_tcp_write_task_info {
                uv_buf_t buf_sent_;
                uv_write_t *write_;
                uv_stream_t *stream_;
                inet_socket *parent_;
            };

            uv_connect_t *connect = reinterpret_cast<uv_connect_t*>(opaque_connect_);

            uv_tcp_write_task_info *info = new uv_tcp_write_task_info;
            info->buf_sent_ = uv_buf_init(const_cast<char*>(reinterpret_cast<const char*>(data)), static_cast<std::uint32_t>(data_size));
            info->parent_ = this;
            info->write_ = reinterpret_cast<uv_write_t*>(opaque_write_info_);
            info->stream_ = connect->handle;
            info->write_->data = this;

            uv_async_t *async = new uv_async_t;
            async->data = info;

            uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
                uv_tcp_write_task_info *task = reinterpret_cast<uv_tcp_write_task_info*>(async->data);

                uv_write(task->write_, task->stream_, &task->buf_sent_, 1, [](uv_write_t *req, int status) {
                    reinterpret_cast<inet_socket*>(req->data)->complete_send_done_info(status);
                });

                delete task;
                delete async;
            });

            uv_async_send(async);
        }
    }

    void inet_socket::prepare_buffer_for_recv(const std::size_t suggested_size, void *buf_ptr) {
        uv_buf_t *buf = reinterpret_cast<uv_buf_t*>(buf_ptr);
        temp_buffer_.resize(suggested_size);
        
        buf->base = temp_buffer_.data();
        buf->len = static_cast<std::uint32_t>(suggested_size);
    }

    static bool is_same_address(const epoc::socket::saddress &addr_requested, const sockaddr *addr_to_check) {
        if (addr_requested.family_ == INET_ADDRESS_FAMILY) {
            if (addr_to_check->sa_family != AF_INET) {
                return false;
            }

            const sockaddr_in *addr_ipv4_check = reinterpret_cast<const sockaddr_in*>(addr_to_check);
            if ((addr_ipv4_check->sin_port != addr_requested.port_) || (memcmp(&addr_ipv4_check->sin_addr, addr_requested.user_data_, 4) != 0)) {
                return false;
            }
        } else {
            if (addr_to_check->sa_family != AF_INET6) {
                return false;
            }

            const sockaddr_in6 *addr_ipv6_check = reinterpret_cast<const sockaddr_in6*>(addr_to_check);
            const sinet6_address &addr_ipv6_guest = static_cast<const sinet6_address&>(addr_requested);

            if ((addr_ipv6_check->sin6_port != addr_ipv6_guest.port_) || (addr_ipv6_check->sin6_scope_id != addr_ipv6_guest.get_scope())
                || (addr_ipv6_check->sin6_flowinfo != addr_ipv6_guest.get_flow()) || (memcmp(&addr_ipv6_check->sin6_addr, addr_ipv6_guest.get_address_32x4(), 16) != 0)) {
                return false;
            }
        }

        return true;
    }

    void inet_socket::handle_udp_delivery(const std::int64_t bytes_read_arg, const void *buf_ptr, const void *addr) {
        const uv_buf_t *buf = reinterpret_cast<const uv_buf_t*>(buf_ptr);
        const sockaddr *recv_addr = reinterpret_cast<const sockaddr*>(addr);

        if (listen_addr_.family_ != epoc::socket::INVALID_FAMILY_ID) {
            // Must check if address matches
            // If not, we continue listen
            if (!is_same_address(listen_addr_, recv_addr)) {
                return;
            }
        }

        // No need, you should stop for now
        uv_udp_recv_stop(reinterpret_cast<uv_udp_t*>(opaque_handle_));

        kernel_system *kern = nullptr;

        if (!recv_done_info_.empty()) {
            kern = recv_done_info_.requester->get_kernel_object_owner();
        }

        int error_code = epoc::error_none;

        if (bytes_read_arg == UV_EOF) {
            // Not suppose to happen? But maybe maybe
            error_code = epoc::error_eof;
        } else if (bytes_read_arg < 0) {
            LOG_ERROR(SERVICE_INTERNET, "Receive data failed with error {}. Please handle!", bytes_read_arg);
            error_code = epoc::error_general;
        } else {
            const std::size_t to_write_byte_count = std::min<const std::size_t>(static_cast<std::size_t>(bytes_read_arg), recv_size_);
            std::memcpy(read_dest_, temp_buffer_.data(), to_write_byte_count);

            if (bytes_read_) {
                *bytes_read_ = static_cast<std::uint32_t>(to_write_byte_count);
            }
        }

        kern->lock();

        // Just complete now!
        if (!recv_done_info_.empty()) {
            recv_done_info_.complete(error_code);
        }

        kern->unlock();
    }

    void inet_socket::handle_tcp_delivery(const std::int64_t bytes_read_arg, const void *buf_ptr) {
        const uv_buf_t *buf = reinterpret_cast<const uv_buf_t*>(buf_ptr);
        kernel_system *kern = nullptr;

        if (!recv_done_info_.empty()) {
            kern = recv_done_info_.requester->get_kernel_object_owner();
        }

        int error_code = epoc::error_none;

        if (bytes_read_arg == UV_EOF) {
            // Not suppose to happen? But maybe maybe
            error_code = epoc::error_eof;
        } else if (bytes_read_arg < 0) {
            LOG_ERROR(SERVICE_INTERNET, "Receive data failed with error {}. Please handle!", bytes_read_arg);
            error_code = epoc::error_general;
        } else {
            if (take_available_only_ && (recv_size_ > static_cast<std::size_t>(bytes_read_arg))) {
                // Avoid adding things overhead, so we just gonna copy paste, and done :)
                memcpy(read_dest_, buf->base, static_cast<std::size_t>(bytes_read_arg));
                if (bytes_read_) {
                    *bytes_read_ = static_cast<std::uint32_t>(bytes_read_arg);
                }
            } else {
                // Push to the queue and pop, see if we got it (accounting case also for RecvOneOrMore)
                if (!stream_data_buffer_) {
                    stream_data_buffer_ = std::make_unique<common::ring_buffer<char, 0x80000>>();
                }

                stream_data_buffer_->push(buf->base, static_cast<std::size_t>(bytes_read_arg));
                if (take_available_only_ || (recv_size_ <= stream_data_buffer_->size())) {
                    auto got_data = stream_data_buffer_->pop(common::min<std::size_t>(static_cast<std::size_t>(recv_size_),
                        stream_data_buffer_->size()));

                    memcpy(read_dest_, got_data.data(), got_data.size());
                    if (bytes_read_) {
                        *bytes_read_ = static_cast<std::uint32_t>(got_data.size());
                    }
                } else {
                    return;
                }
            }
        }

        // No need, you should stop for now
        uv_read_stop(reinterpret_cast<uv_connect_t*>(opaque_connect_)->handle);
        kern->lock();

        if (receive_done_cb_) {
            receive_done_cb_(bytes_read_arg);
            receive_done_cb_ = nullptr;
        }

        // Just complete now!
        if (!recv_done_info_.empty()) {
            recv_done_info_.complete(error_code);
        }

        kern->unlock();
    }

    void inet_socket::receive(std::uint8_t *data, const std::uint32_t data_size, std::uint32_t *recv_size, const epoc::socket::saddress *addr_ptr,
        std::uint32_t flags, epoc::notify_info &complete_info, epoc::socket::receive_done_callback callback) {
        if (!recv_done_info_.empty()) {
            complete_info.complete(epoc::error_in_use);
            return;
        }

        bytes_read_ = recv_size;
        read_dest_ = data;
        recv_size_ = data_size;
        take_available_only_ = false;
        receive_done_cb_ = callback;

        if (flags & epoc::socket::SOCKET_FLAG_DONT_WAIT_FULL) {
            take_available_only_ = true;
        }

        recv_done_info_ = complete_info;

        flags &= ~(epoc::socket::SOCKET_FLAG_DONT_WAIT_FULL);

        if (flags != 0) {
            LOG_TRACE(SERVICE_INTERNET, "Receive data with non-zero flags, please notice! (flag={})", flags);
        }

        listen_addr_.family_ = epoc::socket::INVALID_FAMILY_ID;

        if (protocol_ == INET_UDP_PROTOCOL_ID) {
            if (addr_ptr) {
                listen_addr_ = *addr_ptr;
            }

            uv_udp_t *udp = reinterpret_cast<uv_udp_t*>(opaque_handle_);
            udp->data = this;

            uv_async_t *task = new uv_async_t;
            task->data = udp;

            uv_async_init(uv_default_loop(), task, [](uv_async_t *async) {
                uv_udp_t *udp = reinterpret_cast<uv_udp_t*>(async->data);
                
                uv_udp_recv_start(udp, [](uv_handle_t *handle, std::size_t suggested_size, uv_buf_t *buf) {
                    reinterpret_cast<inet_socket*>(handle->data)->prepare_buffer_for_recv(suggested_size, buf);
                }, [](uv_udp_t *handle, ssize_t bytes_read, const uv_buf_t *buf, const sockaddr *addr_recv, std::uint32_t flags) {
                    reinterpret_cast<inet_socket*>(handle->data)->handle_udp_delivery(static_cast<std::int64_t>(bytes_read), buf, addr_recv);
                });

                delete async;
            });
        
            uv_async_send(task);
        } else {
            if (stream_data_buffer_ && stream_data_buffer_->size()) {
                if (take_available_only_ || (data_size >= stream_data_buffer_->size())) {
                    std::size_t size_to_pop = common::min<std::size_t>(data_size, stream_data_buffer_->size());
                    std::vector<char> data_result = stream_data_buffer_->pop(size_to_pop);

                    if (recv_size) {
                        *recv_size = static_cast<std::uint32_t>(size_to_pop);
                    }

                    memcpy(data, data_result.data(), size_to_pop);

                    if (receive_done_cb_) {
                        receive_done_cb_(size_to_pop);
                        receive_done_cb_ = nullptr;
                    }

                    recv_done_info_.complete(epoc::error_none);
                }
            } else {
                uv_connect_t *connect = reinterpret_cast<uv_connect_t*>(opaque_connect_);
                connect->handle->data = this;

                uv_async_t *task = new uv_async_t;
                task->data = connect;

                uv_async_init(uv_default_loop(), task, [](uv_async_t *async) {
                    uv_connect_t *connect = reinterpret_cast<uv_connect_t*>(async->data);

                    uv_read_start(connect->handle, [](uv_handle_t *handle, std::size_t suggested_size, uv_buf_t *buf) {
                        reinterpret_cast<inet_socket*>(handle->data)->prepare_buffer_for_recv(suggested_size, buf);
                    }, [](uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
                        reinterpret_cast<inet_socket*>(stream->data)->handle_tcp_delivery(static_cast<std::int64_t>(nread), buf);
                    });

                    delete async;
                });

                uv_async_send(task);
            }
        }
    }

    void inet_socket::cancel_receive() {
        if (recv_done_info_.empty()) {
            return;
        }

        uv_async_t *async = new uv_async_t;

        // TODO: Length at the time of the cancel is not filled. Maybe it needs to
        if (protocol_ == INET_UDP_PROTOCOL_ID) {
            uv_udp_t *udp = reinterpret_cast<uv_udp_t*>(opaque_handle_);
            async->data = udp;

            uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
                uv_udp_recv_stop(reinterpret_cast<uv_udp_t*>(async->data));
                delete async;
            });
        } else {
            uv_connect_t *connect = reinterpret_cast<uv_connect_t*>(opaque_connect_);
            async->data = connect;

            uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
                uv_read_stop(reinterpret_cast<uv_connect_t*>(async->data)->handle);
                delete async;
            });
        }

        uv_async_send(async);

        // Don't call
        receive_done_cb_ = nullptr;
        recv_done_info_.complete(epoc::error_cancel);
    }

    void inet_socket::cancel_send() {
        send_done_info_.complete(epoc::error_cancel);
    }

    void inet_socket::cancel_connect() {
        connect_done_info_.complete(epoc::error_cancel);
    }
}
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
#include <common/thread.h>
#include <utils/des.h>
#include <utils/err.h>
#include <kernel/kernel.h>
#include <common/log.h>

extern "C" {
#include <uv.h>
}

#if EKA2L1_PLATFORM(WIN32)
#include <ifdef.h>
#include <iphlpapi.h>
#else
#include <sys/types.h>
#if EKA2L1_PLATFORM(ANDROID)
// SDK 21 does not have ifaddrs. From SDK 24 there is one implementation
#include <common/android/ifaddrs.h>
#else
#include <net/if.h>
#include <ifaddrs.h>
#endif
#endif

#include <uvlooper/uvlooper.h>

namespace eka2l1::epoc::internet {
    std::unique_ptr<epoc::socket::socket> inet_bridged_protocol::make_socket(const std::uint32_t family_id, const std::uint32_t protocol_id, const socket::socket_type sock_type) {
        std::unique_ptr<epoc::socket::socket> sock = std::make_unique<inet_socket>(this);
        inet_socket *sock_casted = reinterpret_cast<inet_socket*>(sock.get());

        if (!sock_casted->open(family_id, protocol_id, sock_type)) {
            return nullptr;
        }

        return sock;
    }

    void inet_bridged_protocol::initialize_looper() {
        if (!looper_->started()) {
            looper_->set_loop_thread_prepare_callback([]() { common::set_thread_priority(common::thread_priority_very_high); });
            looper_->start();
        }
    }

    inet_socket::inet_socket(inet_bridged_protocol *papa)
        : papa_(papa)
        , accept_socket_ptr_(nullptr)
        , accept_server_(nullptr)
        , opaque_handle_(nullptr)
        , opaque_connect_(nullptr)
        , opaque_send_info_(nullptr)
        , opaque_write_info_(nullptr)
        , protocol_(0)
        , bytes_written_(nullptr)
        , bytes_read_(nullptr)
        , read_dest_(nullptr)
        , recv_size_(0)
        , take_available_only_(false)
        , stream_data_buffer_(nullptr)
        , receive_done_cb_(nullptr)
        , broadcast_translate_cached_(false)
        , socket_accepted_hook_(nullptr)
        , looper_(papa->get_looper()) {
        create_frequent_common_tasks();
    }

    void inet_socket::close_down() {
        if (accept_server_) {
            accept_server_->cancel_accept();
        }

        if (opaque_handle_) {
            if (protocol_ == INET_TCP_PROTOCOL_ID) {
                looper_->one_shot([opaque_handle_copy = this->opaque_handle_] {
                    if (uv_tcp_close_reset(reinterpret_cast<uv_tcp_t *>(opaque_handle_copy), [](uv_handle_t *handle) {
                            delete handle;
                        })
                        < 0) {
                        uv_close(reinterpret_cast<uv_handle_t*>(opaque_handle_copy), [](uv_handle_t *handle) {
                            delete handle;
                        });
                    }
                });
            } else {
                looper_->one_shot([opaque_handle_copy = this->opaque_handle_] {
                    uv_udp_t *udp_h = reinterpret_cast<uv_udp_t*>(opaque_handle_copy);
                    uv_udp_recv_stop(udp_h);

                    uv_close(reinterpret_cast<uv_handle_t*>(udp_h), [](uv_handle_t *handle) {
                        delete handle;
                    });
                });
            }

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

        open_event_.reset();

        struct uv_sock_init_params {
            void *opaque_handle_;
            int result_ = 0;
            int family_ = 0;
            common::event *done_evt_ = nullptr;
        };

        uv_sock_init_params params;
        params.done_evt_ = &open_event_;
        params.family_ = family_translated;

        if (protocol_id == INET_TCP_PROTOCOL_ID) {
            opaque_handle_ = new uv_tcp_t;
            reinterpret_cast<uv_tcp_t*>(opaque_handle_)->data = this;

            params.opaque_handle_ = opaque_handle_;

            looper_->one_shot([&params, looper = std::ref(looper_)]() {
                params.result_ = uv_tcp_init_ex(looper.get()->raw_loop(), reinterpret_cast<uv_tcp_t*>(params.opaque_handle_), params.family_);
                params.done_evt_->set();
            });

            create_frequent_tcp_tasks();
        } else {
            opaque_handle_ = new uv_udp_t;
            reinterpret_cast<uv_udp_t*>(opaque_handle_)->data = this;

            params.opaque_handle_ = opaque_handle_;

            looper_->one_shot([&params, looper = std::ref(looper_)]() {
                params.result_ = uv_udp_init_ex(looper.get()->raw_loop(), reinterpret_cast<uv_udp_t*>(params.opaque_handle_), params.family_);
                params.done_evt_->set();
            });

            create_frequent_udp_tasks();
        }

        // Start the looper now, we might have the first customer!
        // Also unlock the kernel at this time, allow free modification, so that the socket thread can
        // proceed to deal with data and get to our open request >D<
        kernel_system *kern = papa_->get_kernel_system();
        papa_->initialize_looper();

        kern->unlock();
        open_event_.wait();
        kern->lock();

        if (params.result_ < 0) {
            LOG_ERROR(SERVICE_INTERNET, "Socket failed to be initialize, error code={}", errno);
            return false;
        }

        protocol_ = protocol_id;
        return true;
    }

    void inet_socket::create_frequent_tcp_tasks() {
        connect_task_ = libuv::create_task([this]() {
            tcp_connect_impl_async();
        });

        listen_task_ = libuv::create_task([this]() {
            listen_impl_async();
        });

        accept_task_ = libuv::create_task([this]() {
            handle_accept_impl();
        });

        send_task_ = libuv::create_task([this]() {
            tcp_send_impl_async();
        });

        recv_task_ = libuv::create_task([this]() {
            tcp_recv_impl_async();
        });

        cancel_recv_task_ = libuv::create_task([this]() {
            tcp_cancel_recv_impl_async();
        });

        shutdown_task_ = libuv::create_task([this]() {
            tcp_shutdown_impl_async();
        });
    }

    void inet_socket::create_frequent_udp_tasks() {
        connect_task_ = libuv::create_task([this]() {
            udp_connect_impl_async();
        });
        
        send_task_ = libuv::create_task([this]() {
            udp_send_impl_async();
        });

        recv_task_ = libuv::create_task([this]() {
            udp_recv_impl_async();
        });
        
        cancel_recv_task_ = libuv::create_task([this]() {
            udp_cancel_recv_impl_async();
        });

        shutdown_task_ = libuv::create_task([this]() {
            udp_shutdown_impl_async();
        });
    }

    void inet_socket::create_frequent_common_tasks() {
        bind_task_ = libuv::create_task([this]() {
            bind_impl_async();
        });

        bind_callback_task_ = libuv::create_task([this]() {
            bind_callback_impl_async();
        });
    }

    void inet_socket::handle_connect_done_error_code(const int error_code) {
        if (connect_done_info_.empty()) {
            return;
        }

        kernel_system *kern = connect_done_info_.requester->get_kernel_object_owner();
        kern->lock();

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

            case UV_ETIMEDOUT:
                guest_error_code = epoc::error_timed_out;
                break;

            default:
                guest_error_code = epoc::error_general;
                break;
            }

            connect_done_info_.complete(guest_error_code);
        }

        kern->unlock();
    }

    void inet_socket::complete_connect_done_info(const int err) {
        if (connect_done_info_.empty()) {
            return;
        }

        handle_connect_done_error_code(err);
    }

    void inet_socket::tcp_connect_impl_async() {
        uv_connect_t *connect = reinterpret_cast<uv_connect_t *>(opaque_connect_);
        int err = uv_tcp_connect(
            connect,
            reinterpret_cast<uv_tcp_t*>(opaque_handle_),
            reinterpret_cast<const sockaddr*>(&connect_addr_),
            [](uv_connect_t *connect, const int err) {
            inet_socket *socket = reinterpret_cast<inet_socket*>(connect->data);
            socket->complete_connect_done_info(err);
        });

        if (err < 0) {
            LOG_ERROR(SERVICE_INTERNET, "Connect socket failed with libuv code {}", err);
            reinterpret_cast<inet_socket*>(connect->data)->complete_connect_done_info(err);
        }
    }

    void inet_socket::udp_connect_impl_async() {
        const int err = uv_udp_connect(
            reinterpret_cast<uv_udp_t*>(opaque_handle_),
            reinterpret_cast<const sockaddr*>(&connect_addr_));

        complete_connect_done_info(err);
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

        if (protocol_ == INET_TCP_PROTOCOL_ID) {
            if (!opaque_connect_) {
                opaque_connect_ = new uv_connect_t;
                reinterpret_cast<uv_connect_t*>(opaque_connect_)->data = this;
            }
        }

        std::memcpy(&connect_addr_, ip_addr_ptr, sizeof(sockaddr_in6));
        looper_->post_task(connect_task_);
    }

    void inet_socket::bind_impl_async() {
        sockaddr *ip_addr_ptr = nullptr;
        GUEST_TO_BSD_ADDR(bind_addr_, ip_addr_ptr);

        if (reuse_addr_changed_) {
            // Set flags
            reuse_addr_changed_ = false;

            uv_os_fd_t fd = 0;
            uv_fileno(reinterpret_cast<uv_handle_t *>(opaque_handle_), &fd);

            int value = reuse_addr_;

#if EKA2L1_PLATFORM(WIN32)
            setsockopt(reinterpret_cast<SOCKET>(fd), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&value), 4);
            
#else
            setsockopt(reinterpret_cast<int>(fd), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&value), 4);
#endif
        }

        if (protocol_ == INET_UDP_PROTOCOL_ID) {
            uv_udp_bind(reinterpret_cast<uv_udp_t*>(opaque_handle_), ip_addr_ptr, 0);
        } else {
            uv_tcp_bind(reinterpret_cast<uv_tcp_t*>(opaque_handle_), ip_addr_ptr, 0);
        }

        kernel_system *kern = bind_done_info_.requester->get_kernel_object_owner();

        kern->lock();
        bind_done_info_.complete(epoc::error_none);
        kern->unlock();
    }

    void inet_socket::bind(const epoc::socket::saddress &addr, epoc::notify_info &info) {
        if (!opaque_handle_) {
            info.complete(epoc::error_not_ready);
            return;
        }

        if (!bind_done_info_.empty()) {
            info.complete(epoc::error_in_use);
            return;
        }

        bind_done_info_ = info;
        bind_addr_ = addr;

        looper_->post_task(bind_task_);
    }

    void inet_socket::bind_callback_impl_async() {
        sockaddr *ip_addr_ptr = nullptr;
        GUEST_TO_BSD_ADDR(bind_addr_, ip_addr_ptr);

        if (protocol_ == INET_UDP_PROTOCOL_ID) {
            uv_udp_bind(reinterpret_cast<uv_udp_t*>(opaque_handle_), ip_addr_ptr, 0);
        } else {
            uv_tcp_bind(reinterpret_cast<uv_tcp_t*>(opaque_handle_), ip_addr_ptr, 0);
        }

        bind_callback_(epoc::error_none);
    }

    void inet_socket::bind_callback(const epoc::socket::saddress &addr, std::function<void(int)> callback) {
        if (!opaque_handle_) {
            callback(epoc::error_not_ready);
            return;
        }

        bind_addr_ = addr;
        bind_callback_ = callback;

        looper_->post_task(bind_callback_task_);
    }

    void inet_socket::handle_new_connection() {
        kernel_system *kern = nullptr;
        if (!accept_done_info_.empty()) {
            kern = accept_done_info_.requester->get_kernel_object_owner();
        } else {
            return;
        }

        if (!accept_socket_ptr_->opaque_handle_) {
            accept_socket_ptr_->opaque_handle_ = new uv_tcp_t;
            accept_socket_ptr_->protocol_ = INET_TCP_PROTOCOL_ID;
            uv_tcp_init(uv_default_loop(), reinterpret_cast<uv_tcp_t*>(accept_socket_ptr_->opaque_handle_));

            reinterpret_cast<uv_tcp_t *>(accept_socket_ptr_->opaque_handle_)->data = accept_socket_ptr_;
        }

        uv_accept(reinterpret_cast<uv_stream_t*>(opaque_handle_), reinterpret_cast<uv_stream_t*>(accept_socket_ptr_->opaque_handle_));

        accept_socket_ptr_->accept_server_ = nullptr;
        accept_socket_ptr_->create_frequent_tcp_tasks();

        {
            const std::lock_guard<std::mutex> guard(hook_lock_);
            if (socket_accepted_hook_) {
                socket_accepted_hook_(accept_socket_ptr_->opaque_handle_);
            }
        }

        accept_socket_ptr_ = nullptr;

        kern->lock();
        accept_done_info_.complete(epoc::error_none);
        kern->unlock();
    }

    void inet_socket::set_listen_event(const int listen_event_result) {
        listen_event_.set();
        listen_event_result_ = listen_event_result;
    }

    void inet_socket::listen_impl_async() {
        const int err = uv_listen(reinterpret_cast<uv_stream_t*>(opaque_handle_),
            backlog_count_,
            [](uv_stream_t *server, int status) {
            inet_socket *sock = reinterpret_cast<inet_socket*>(server->data);
            if (status < 0) {
                LOG_ERROR(SERVICE_INTERNET, "Socket new connection has error status {}", status);
                return;
            }
            sock->handle_new_connection();
        });

        set_listen_event(err);
    }

    std::int32_t inet_socket::listen(const std::uint32_t backlog) {
        if (!opaque_handle_) {
            return epoc::error_not_ready;
        }

        if (protocol_ == INET_UDP_PROTOCOL_ID) {
            LOG_ERROR(SERVICE_INTERNET, "Listen is not supported on UDP protocol!");
            return epoc::error_not_supported;
        }

        listen_event_.reset();
        backlog_count_ = static_cast<int>(backlog);

        looper_->post_task(listen_task_);

        kernel_system *kern = papa_->get_kernel_system();
        kern->unlock();

        listen_event_.wait();

        kern->lock();

        if (listen_event_result_ < 0) {
            LOG_ERROR(SERVICE_INTERNET, "libuv's socket listening failed with code {}", listen_event_result_);
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    void inet_socket::handle_accept_impl() {
        kernel_system *kern = nullptr;
        if (!accept_done_info_.empty()) {
            kern = accept_done_info_.requester->get_kernel_object_owner();
        } else {
            return;
        }

        accept_socket_ptr_->accept_server_ = this;

        if (!accept_socket_ptr_->opaque_handle_) {
            accept_socket_ptr_->opaque_handle_ = new uv_tcp_t;
            accept_socket_ptr_->protocol_ = INET_TCP_PROTOCOL_ID;
            uv_tcp_init(uv_default_loop(), reinterpret_cast<uv_tcp_t*>(accept_socket_ptr_->opaque_handle_));

            reinterpret_cast<uv_tcp_t *>(accept_socket_ptr_->opaque_handle_)->data = accept_socket_ptr_;
        }

        if (uv_accept(reinterpret_cast<uv_stream_t*>(opaque_handle_), reinterpret_cast<uv_stream_t*>(accept_socket_ptr_->opaque_handle_)) == UV_EAGAIN) {
            // No stream is yet available, let the later connection notification handle it then!
            return;
        }

        accept_socket_ptr_->create_frequent_tcp_tasks();

        {
            const std::lock_guard<std::mutex> guard(hook_lock_);
            if (socket_accepted_hook_) {
                socket_accepted_hook_(accept_socket_ptr_->opaque_handle_);
            }
        }

        // Well we are done, lol
        kern->lock();
        accept_done_info_.complete(epoc::error_none);
        accept_socket_ptr_->accept_server_ = nullptr;
        accept_socket_ptr_ = nullptr;
        kern->unlock();
    }

    void inet_socket::accept(std::unique_ptr<epoc::socket::socket> *pending_sock, epoc::notify_info &complete_info) {
        if (!accept_done_info_.empty()) {
            LOG_ERROR(SERVICE_INTERNET, "Accept is called when pending accept is not yet finished!");
            complete_info.complete(epoc::error_permission_denied);

            return;
        }

        *pending_sock = std::make_unique<inet_socket>(papa_);

        accept_socket_ptr_ = reinterpret_cast<inet_socket*>(pending_sock->get());
        accept_done_info_ = complete_info;

        looper_->post_task(accept_task_);
    }

    void inet_socket::cancel_accept() {
        // NOTE: Sad race condition
        if (accept_done_info_.empty()) {
            return;
        }

        accept_done_info_.complete(epoc::error_cancel);
        accept_socket_ptr_->accept_server_ = nullptr;
        accept_socket_ptr_ = nullptr;
    }   

    std::int32_t inet_socket::local_name(epoc::socket::saddress &result, std::uint32_t &result_len) {
        if (!opaque_handle_) {
            return epoc::error_not_ready;
        }

        sockaddr_in6 sock_max;
        int name_len, error;

        name_len = sizeof(sockaddr_in6);

        if (protocol_ == INET_UDP_PROTOCOL_ID) {
            error = uv_udp_getsockname(reinterpret_cast<uv_udp_t*>(opaque_handle_), reinterpret_cast<sockaddr*>(&sock_max), &name_len);
        } else {
            error = uv_tcp_getsockname(reinterpret_cast<uv_tcp_t*>(opaque_handle_), reinterpret_cast<sockaddr*>(&sock_max), &name_len);
        }

        if (error != 0) {
            return epoc::error_not_ready;
        }

        host_sockaddr_to_guest_saddress(reinterpret_cast<sockaddr*>(&sock_max), result, &result_len);
        return epoc::error_none;
    }

    std::int32_t inet_socket::remote_name(epoc::socket::saddress &result, std::uint32_t &result_len) {
        if (!opaque_handle_) {
            return epoc::error_not_ready;
        }

        sockaddr_in6 sock_max;
        int name_len, error;

        name_len = sizeof(sockaddr_in6);

        if (protocol_ == INET_UDP_PROTOCOL_ID) {
            error = uv_udp_getpeername(reinterpret_cast<uv_udp_t*>(opaque_handle_), reinterpret_cast<sockaddr*>(&sock_max), &name_len);
        } else {
            error = uv_tcp_getpeername(reinterpret_cast<uv_tcp_t*>(opaque_handle_), reinterpret_cast<sockaddr*>(&sock_max), &name_len);
        }

        if (error != 0) {
            return epoc::error_not_ready;
        }

        host_sockaddr_to_guest_saddress(reinterpret_cast<sockaddr*>(&sock_max), result, &result_len);
        return epoc::error_none;
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
    
    bool retrieve_local_ip_info(epoc::socket::saddress &broadcast, epoc::socket::saddress *my_selfip) {
        inet_interface_info info_temp;
        inet_socket_interface_iterator iterator;

        if (iterator.start()) {
            while (iterator.next(info_temp) == sizeof(inet_interface_info)) {
                if ((info_temp.addr_.family_ == epoc::internet::INET_ADDRESS_FAMILY) &&
                    (info_temp.addr_.user_data_[0] != 127) && (*info_temp.addr_.addr_long() != 0)) {
                    broadcast = info_temp.broadcast_addr_;
                    broadcast.port_ = 0;

                    if (my_selfip) {
                        *my_selfip = info_temp.addr_;
                        my_selfip->port_ = 0;
                    }

                    return true;
                }
            }
        }

        return false;
    }

    void inet_socket::udp_send_impl_async() {
        uv_udp_t *udp = reinterpret_cast<uv_udp_t *>(opaque_handle_);
        uv_udp_send_t *send_info = reinterpret_cast<uv_udp_send_t *>(opaque_send_info_);

        uv_udp_set_broadcast(udp, 1);
        const int res = uv_udp_send(send_info, udp, &send_buf_, 1, reinterpret_cast<const sockaddr*>(&send_dest_), [](uv_udp_send_t *send_info, int status) {
            inet_socket *socket = reinterpret_cast<inet_socket*>(send_info->data);
            socket->complete_send_done_info(status);
        });

        if (res < 0) {
            LOG_ERROR(SERVICE_INTERNET, "Sending UDP packet failed with libuv's code {}", res);
            complete_send_done_info(res);
        }
    }

    void inet_socket::tcp_send_impl_async() {
        uv_stream_t *stream = reinterpret_cast<uv_stream_t *>(opaque_handle_);
        uv_write_t *write = reinterpret_cast<uv_write_t *>(opaque_write_info_);

        int err = uv_write(write, stream, &send_buf_, 1, [](uv_write_t *req, int status) {
            inet_socket *socket = reinterpret_cast<inet_socket *>(req->data);
            socket->complete_send_done_info(status);
        });

        if (err < 0) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Sending TCP packet failed with libuv's error {}", err);
            complete_send_done_info(err);
        }
    }

    void inet_socket::send(const std::uint8_t *data, std::uint32_t data_size, std::uint32_t *sent_size, const epoc::socket::saddress *addr_ptr, std::uint32_t flags, epoc::notify_info &complete_info) {
        if (!send_done_info_.empty()) {
            complete_info.complete(epoc::error_in_use);
            return;
        }

        sockaddr *ip_addr_ptr = nullptr;
        epoc::socket::saddress addr_guest_temp;

        if (addr_ptr != nullptr) {
            addr_guest_temp = *addr_ptr;
            // The special broadcast address has been broken even on Android since who knows when
            // In here we pick the most likely one that is running.
            // On Windows it just sends it to the top one active
            if (addr_guest_temp.family_ == epoc::internet::INET_ADDRESS_FAMILY) {
                sinet_address &addr_inet = static_cast<sinet_address&>(addr_guest_temp);
                if (*addr_inet.addr_long() == 0xFFFFFFFF) {
                    std::uint32_t prev_port = addr_guest_temp.port_;
                    if (!broadcast_translate_cached_) {
                        broadcast_translate_cached_ = retrieve_local_ip_info(addr_guest_temp, nullptr);

                        if (!broadcast_translate_cached_) {
                            LOG_ERROR(SERVICE_ESOCK, "Could not translate 255.255.255.255 special broadcast to local Internet interface!");
                            complete_info.complete(epoc::error_could_not_connect);

                            return;
                        } else {
                            cached_broadcast_translate_ = static_cast<sinet_address&>(addr_guest_temp);
                        }
                    } else {
                        addr_guest_temp = cached_broadcast_translate_;
                    }

                    addr_guest_temp.port_ = prev_port;
                }
            }
        }

        GUEST_TO_BSD_ADDR(addr_guest_temp, ip_addr_ptr);

        if (addr_ptr == nullptr)  {
            ip_addr_ptr = nullptr;
        }

        bytes_written_ = sent_size;

        if (bytes_written_) {
            // Callback done then it's fully written. Else the cancel will rewrite this value
            // This assumes the pointer is still valid until then :o
            *bytes_written_ = data_size;
        }

        send_buf_ = uv_buf_init(const_cast<char*>(reinterpret_cast<const char*>(data)), static_cast<std::uint32_t>(data_size));
        send_done_info_ = complete_info;

        if (ip_addr_ptr) {
            std::memcpy(&send_dest_, ip_addr_ptr, sizeof(sockaddr_in6));
        }

        if (flags != 0) {
            LOG_TRACE(SERVICE_INTERNET, "Send data with non-zero flags, please notice! (flag={})", flags);
        }

        if (protocol_ == INET_UDP_PROTOCOL_ID) {
            if (!opaque_send_info_) {
                opaque_send_info_ = new uv_udp_send_t;
                reinterpret_cast<uv_udp_send_t*>(opaque_send_info_)->data = this;
            }
        } else {
            // Address is not important here.
            if (!opaque_write_info_) {
                opaque_write_info_ = new uv_write_t;
                reinterpret_cast<uv_write_t *>(opaque_write_info_)->data = this;
            }
        }

        looper_->post_task(send_task_);
    }

    void inet_socket::prepare_buffer_for_recv(const std::size_t suggested_size, void *buf_ptr) {
        uv_buf_t *buf = reinterpret_cast<uv_buf_t*>(buf_ptr);
        temp_buffer_.resize(suggested_size);
        
        buf->base = temp_buffer_.data();
        buf->len = static_cast<std::uint32_t>(suggested_size);
    }

    void inet_socket::handle_udp_delivery(const std::int64_t bytes_read_arg, const void *buf_ptr, const void *addr) {
        const uv_buf_t *buf = reinterpret_cast<const uv_buf_t*>(buf_ptr);
        const sockaddr *recv_addr = reinterpret_cast<const sockaddr*>(addr);

        if (recv_addr_) {
            // sorry...
            host_sockaddr_to_guest_saddress(const_cast<sockaddr*>(recv_addr), *recv_addr_);
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
        } else if (bytes_read_arg <= 0) {
            if ((bytes_read_arg == UV_ECONNRESET) || (bytes_read_arg == 0)) {
                error_code = epoc::error_disconnected;
            } else {
                LOG_ERROR(SERVICE_INTERNET, "Receive data failed with error {}. Please handle!", bytes_read_arg);
                error_code = epoc::error_general;
            }
        } else {
            const std::size_t to_write_byte_count = std::min<const std::size_t>(static_cast<std::size_t>(bytes_read_arg), recv_size_);
            std::memcpy(read_dest_, temp_buffer_.data(), to_write_byte_count);

            if (bytes_read_) {
                *bytes_read_ = static_cast<std::uint32_t>(to_write_byte_count);
            }

            if (receive_done_cb_) {
                receive_done_cb_(static_cast<std::uint32_t>(to_write_byte_count));
                receive_done_cb_ = nullptr;
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
        std::uint32_t bytes_taken = 0;

        if (bytes_read_arg == UV_EOF) {
            // Not suppose to happen? But maybe maybe
            error_code = epoc::error_eof;
        } else if (bytes_read_arg <= 0) {
            if ((bytes_read_arg == UV_ECONNRESET) || (bytes_read_arg == 0)) {
                error_code = epoc::error_disconnected;
            } else {
                error_code = epoc::error_general;
            }
        } else {
            // We must ensure that if we want to take the data immediately, there must be nothing left in the ring buffer
            // Else we would create data disorder, leading to corruption
            if (take_available_only_ && (!stream_data_buffer_ || (stream_data_buffer_->size() == 0))
                && (recv_size_ >= static_cast<std::size_t>(bytes_read_arg))) {
                // Avoid adding things overhead, so we just gonna copy paste, and done :)
                memcpy(read_dest_, buf->base, static_cast<std::size_t>(bytes_read_arg));
                if (bytes_read_) {
                    *bytes_read_ = static_cast<std::uint32_t>(bytes_read_arg);
                }
                bytes_taken = static_cast<std::uint32_t>(bytes_read_arg);
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

                    bytes_taken = static_cast<std::uint32_t>(got_data.size());
                } else {
                    return;
                }
            }
        }

        // No need, you should stop for now
        uv_read_stop(reinterpret_cast<uv_stream_t*>(opaque_handle_));
        kern->lock();

        if (receive_done_cb_) {
            receive_done_cb_(bytes_taken);
            receive_done_cb_ = nullptr;
        }

        // Just complete now!
        if (!recv_done_info_.empty()) {
            recv_done_info_.complete(error_code);
        }

        kern->unlock();
    }

    void inet_socket::udp_recv_impl_async() {
        uv_udp_t *udp = reinterpret_cast<uv_udp_t*>(opaque_handle_);
        
        uv_udp_set_broadcast(udp, 1);
        uv_udp_recv_start(udp, [](uv_handle_t *handle, std::size_t suggested_size, uv_buf_t *buf) {
            reinterpret_cast<inet_socket*>(handle->data)->prepare_buffer_for_recv(suggested_size, buf);
        }, [](uv_udp_t *handle, ssize_t bytes_read, const uv_buf_t *buf, const sockaddr *addr_recv, std::uint32_t flags) {
            reinterpret_cast<inet_socket*>(handle->data)->handle_udp_delivery(static_cast<std::int64_t>(bytes_read), buf, addr_recv);
        });
    }

    void inet_socket::tcp_recv_impl_async() {
        uv_stream_t *tcp_stream = reinterpret_cast<uv_stream_t*>(opaque_handle_);

        int start_err = uv_read_start(tcp_stream, [](uv_handle_t *handle, std::size_t suggested_size, uv_buf_t *buf) {
            reinterpret_cast<inet_socket*>(handle->data)->prepare_buffer_for_recv(suggested_size, buf);
        }, [](uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
            reinterpret_cast<inet_socket*>(stream->data)->handle_tcp_delivery(static_cast<std::int64_t>(nread), buf);
        });

        if (start_err != 0) {
            LOG_TRACE(SERVICE_BLUETOOTH, "Error trying to receive TCP data. Libuv's error code is {}", start_err);
        }
    }

    void inet_socket::receive(std::uint8_t *data, const std::uint32_t data_size, std::uint32_t *recv_size, epoc::socket::saddress *addr_ptr,
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
        recv_addr_ = addr_ptr;

        if (flags & epoc::socket::SOCKET_FLAG_DONT_WAIT_FULL) {
            take_available_only_ = true;
        }

        recv_done_info_ = complete_info;

        flags &= ~(epoc::socket::SOCKET_FLAG_DONT_WAIT_FULL);

        if (flags != 0) {
            LOG_TRACE(SERVICE_INTERNET, "Receive data with non-zero flags, please notice! (flag={})", flags);
        }

        if (recv_addr_) {
            recv_addr_->family_ = epoc::socket::INVALID_FAMILY_ID;
        }

        if (protocol_ == INET_TCP_PROTOCOL_ID) {
            if (stream_data_buffer_ && stream_data_buffer_->size()) {
                if (take_available_only_ || (data_size <= stream_data_buffer_->size())) {
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
                    return;
                }
            }
        }
        
        looper_->post_task(recv_task_);
    }

    void inet_socket::tcp_cancel_recv_impl_async() {
        if (recv_done_info_.empty()) {
            return;
        }

        uv_read_stop(reinterpret_cast<uv_stream_t*>(opaque_handle_));

        // Don't call
        receive_done_cb_ = nullptr;

        kernel_system *kern = recv_done_info_.requester->get_kernel_object_owner();

        kern->lock();
        recv_done_info_.complete(epoc::error_cancel);
        kern->unlock();
    }

    void inet_socket::udp_cancel_recv_impl_async() {
        if (recv_done_info_.empty()) {
            return;
        }

        uv_udp_recv_stop(reinterpret_cast<uv_udp_t *>(opaque_handle_));
        
        kernel_system *kern = recv_done_info_.requester->get_kernel_object_owner();

        kern->lock();
        recv_done_info_.complete(epoc::error_cancel);
        kern->unlock();
    }

    void inet_socket::cancel_receive() {
        if (recv_done_info_.empty()) {
            return;
        }

        looper_->post_task(cancel_recv_task_);
    }

    void inet_socket::cancel_send() {
        send_done_info_.complete(epoc::error_cancel);
    }

    void inet_socket::cancel_connect() {
        connect_done_info_.complete(epoc::error_cancel);
    }

    void inet_socket::complete_shutdown_info(const int err) {
        if (shutdown_info_.empty()) {
            return;
        }

        kernel_system *kern = shutdown_info_.requester->get_kernel_object_owner();
        kern->lock();

        if (err < 0) {
            LOG_ERROR(SERVICE_INTERNET, "Shutdown encountered libuv error code {}", err);
            shutdown_info_.complete(epoc::error_general);
        } else {
            shutdown_info_.complete(epoc::error_none);
        }

        kern->unlock();
    }

    void inet_socket::tcp_shutdown_impl_async() {
        uv_stream_t *stream = reinterpret_cast<uv_stream_t *>(opaque_handle_);
        uv_shutdown_t *shut = new uv_shutdown_t;

        int res = uv_shutdown(shut, stream, [](uv_shutdown_t *shut, int status) {
            reinterpret_cast<inet_socket*>(shut->handle->data)->complete_shutdown_info(status);
            delete shut;
        });

        if (res < 0) {
            complete_shutdown_info(res);
            delete shut;
        }
    }

    void inet_socket::udp_shutdown_impl_async() {
        uv_udp_t *udp_h = reinterpret_cast<uv_udp_t*>(opaque_handle_);
        uv_udp_recv_stop(udp_h);
        
        complete_shutdown_info(0);
    }

    void inet_socket::shutdown(epoc::notify_info &complete_info, int reason) {
        if (!shutdown_info_.empty()) {
            complete_info.complete(epoc::error_in_use);
            return;
        }

        shutdown_info_ = complete_info;
        looper_->post_task(shutdown_task_);
    }

    std::size_t inet_socket::get_option(const std::uint32_t option_id, const std::uint32_t option_family,
        std::uint8_t *buffer, const std::size_t avail_size) {
        if (option_family == INET_INTERFACE_CONTROL_OPT_FAMILY) {
            switch (option_id) {
            case INET_NEXT_INTERFACE_OPT: {
                return retrieve_next_interface_info(buffer, avail_size);

            default:
                break;
            }
            }
        }

        return socket::get_option(option_id, option_family, buffer, avail_size);
    }

    bool inet_socket::set_option(const std::uint32_t option_id, const std::uint32_t option_family,
        std::uint8_t *buffer, const std::size_t avail_size) {
        if (option_family == INET_INTERFACE_CONTROL_OPT_FAMILY) {
            switch (option_id) {
            case INET_ENUM_INTERFACES_OPT: {
                return interface_iterator_.start();

            default:
                break;
            }
            }
        } else if (option_family == INET_TCP_SOCK_OPT_LEVEL) {
            if (protocol_ != INET_TCP_PROTOCOL_ID) {
                LOG_TRACE(SERVICE_ESOCK, "Calling TCP socket option while the socket is UDP!");
                return false;
            }
            switch (option_id) {
            case INET_TCP_NO_DELAY_OPT: {
                int value = *reinterpret_cast<int *>(buffer);
                looper_->one_shot([this, value]() {
                    uv_tcp_nodelay(reinterpret_cast<uv_tcp_t*>(opaque_handle_), value);
                });
                return true;
            }

            default:
                break;
            }
        } else if (option_family == INET_IP_SOCK_OPT_LEVEL) {
            switch (option_id) {
            case INET_REUSE_ADDR: {
                int value = *reinterpret_cast<int *>(buffer);

                if (reuse_addr_ != static_cast<bool>(value)) {
                    reuse_addr_changed_ = true;
                    reuse_addr_ = static_cast<bool>(value);
                }

                return true;
            }

            default:
                break;
            }
        }

        return socket::set_option(option_id, option_family, buffer, avail_size);
    }

    std::size_t inet_socket::retrieve_next_interface_info(std::uint8_t *buffer, const std::size_t avail_size) {
        if (avail_size != sizeof(inet_interface_info)) {
            LOG_ERROR(SERVICE_ESOCK, "Size of buffer is not correct!");
            return MAKE_SOCKET_GETOPT_ERROR(epoc::error_argument);
        }

        return interface_iterator_.next(*reinterpret_cast<inet_interface_info*>(buffer));
    }

    std::size_t inet_socket_interface_iterator::next(inet_interface_info &info) {
        if (!opaque_interface_info_) {
            return MAKE_SOCKET_GETOPT_ERROR(epoc::error_not_ready);
        }

        if (!pending_interface_infos_.empty()) {
            info = pending_interface_infos_.front();
            pending_interface_infos_.pop();

            return sizeof(inet_interface_info);
        }

        if (!opaque_interface_info_current_) {
            return MAKE_SOCKET_GETOPT_ERROR(epoc::error_eof);
        }

#if EKA2L1_PLATFORM(WIN32)
        IP_ADAPTER_ADDRESSES *adapter_info_current = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(opaque_interface_info_current_);
        CHAR interface_name[IF_NAMESIZE];
        
        // Some games just grab interfaces that is just there...
        do {
            if (adapter_info_current == nullptr) {    
                return MAKE_SOCKET_GETOPT_ERROR(epoc::error_eof);
            }
            bool skip_this_interface = false;
            if (((adapter_info_current->IfIndex != 0) && if_indextoname(adapter_info_current->IfIndex, interface_name)) ||
                ((adapter_info_current->IfIndex != 0) && if_indextoname(adapter_info_current->Ipv6IfIndex, interface_name))) {
                if (strncmp(interface_name, "iftype", 6) == 0) {
                    skip_this_interface = true;
                }
            } else {
                skip_this_interface = true;
            }
            if (!skip_this_interface && (adapter_info_current->OperStatus == IfOperStatusDown)) {
                skip_this_interface = true;
            }
            std::u16string description = std::u16string(reinterpret_cast<const char16_t*>(adapter_info_current->Description));
            if (!skip_this_interface && ((description.find(u"Virtual") != std::string::npos) ||
                (description.find(u"Virtual") != std::string::npos))) {
                skip_this_interface = true;
            }
            if (!skip_this_interface) {
                break;
            }
            opaque_interface_info_current_ =  adapter_info_current->Next;
            adapter_info_current = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(opaque_interface_info_current_);
        } while (true);

        info.name_.assign(nullptr, std::u16string(reinterpret_cast<const char16_t*>(adapter_info_current->FriendlyName)));
        info.status_ = (adapter_info_current->OperStatus == IfOperStatusDown) ? inet_interface_status_down : inet_interface_status_up;
        info.mtu_ = adapter_info_current->Mtu;
        info.speed_metric_ = static_cast<std::int32_t>(adapter_info_current->ReceiveLinkSpeed / 1024);   // In kbps
        info.features_ = 0;
        std::memcpy(info.hardware_addr_.user_data_, adapter_info_current->PhysicalAddress, adapter_info_current->PhysicalAddressLength);     // Should not be able to overflow, Windows max is 8
        info.hardware_addr_len_ = 8 + adapter_info_current->PhysicalAddressLength;

        auto fill_interface_addr_info = [](inet_interface_info &info, PIP_ADAPTER_UNICAST_ADDRESS_LH addr_from_raw) {
            host_sockaddr_to_guest_saddress(addr_from_raw->Address.lpSockaddr, info.addr_, &info.addr_len_, true);
            // TODO: For IPv6 too!
            if (addr_from_raw->Address.lpSockaddr->sa_family == AF_INET) {
                ULONG mask_value = 0;
                ConvertLengthToIpv4Mask(addr_from_raw->OnLinkPrefixLength, &mask_value);

                *info.netmask_addr_.addr_long() = static_cast<std::uint32_t>(mask_value);
                *info.broadcast_addr_.addr_long() = *info.addr_.addr_long() | (~*info.netmask_addr_.addr_long());
            
                info.netmask_addr_.family_ = INET_ADDRESS_FAMILY;
                info.broadcast_addr_.family_ = INET_ADDRESS_FAMILY;

                epoc::set_descriptor_length_variable(info.netmask_addr_len_, sinet_address::DATA_SIZE);
                epoc::set_descriptor_length_variable(info.broadcast_addr_len_, sinet_address::DATA_SIZE);

                info.netmask_addr_max_len_ = sinet_address::DATA_SIZE;
                info.broadcast_addr_max_len_ = sinet_address::DATA_SIZE;

                info.addr_max_len_ = sinet_address::DATA_SIZE;
            }
        };

        fill_interface_addr_info(info, adapter_info_current->FirstUnicastAddress);

        if (adapter_info_current->FirstDnsServerAddress) {
            host_sockaddr_to_guest_saddress(adapter_info_current->FirstDnsServerAddress->Address.lpSockaddr, info.primary_name_server_, &info.primary_name_server_len_, true);
        }
        
        if (adapter_info_current->FirstGatewayAddress) {
            host_sockaddr_to_guest_saddress(adapter_info_current->FirstGatewayAddress->Address.lpSockaddr, info.default_gateway_, &info.default_gateway_len_, true);
        }

        if (adapter_info_current->FirstUnicastAddress->Next != nullptr) {
            inet_interface_info info_temp = info;
            IP_ADAPTER_UNICAST_ADDRESS_LH *unicast_addr = adapter_info_current->FirstUnicastAddress->Next;
        
            while (unicast_addr) {
                fill_interface_addr_info(info_temp, unicast_addr);
                pending_interface_infos_.push(info_temp);

                unicast_addr = unicast_addr->Next;
            }
        }

        opaque_interface_info_current_ = adapter_info_current->Next;
#else
        ifaddrs *current_addr_info_posix = reinterpret_cast<ifaddrs*>(opaque_interface_info_current_);
        while (current_addr_info_posix && ((strncmp(current_addr_info_posix->ifa_name, "vmnet", 5) == 0) ||
                ((current_addr_info_posix->ifa_flags & IFF_RUNNING) != IFF_RUNNING))) {
            current_addr_info_posix = current_addr_info_posix->ifa_next;
            opaque_interface_info_current_ = current_addr_info_posix;
            continue;
        }
        info.name_.assign(nullptr, common::utf8_to_ucs2(current_addr_info_posix->ifa_name));
        host_sockaddr_to_guest_saddress(current_addr_info_posix->ifa_addr, info.addr_, &info.addr_len_, true);
        host_sockaddr_to_guest_saddress(current_addr_info_posix->ifa_netmask, info.netmask_addr_, &info.netmask_addr_len_, true);

#if !EKA2L1_PLATFORM(ANDROID)
        host_sockaddr_to_guest_saddress(current_addr_info_posix->ifa_broadaddr, info.broadcast_addr_, &info.broadcast_addr_len_, true);
#else
        if (info.addr_.family_ == INET_ADDRESS_FAMILY) {
            epoc::set_descriptor_length_variable(info.broadcast_addr_len_, sinet_address::DATA_SIZE);
            info.broadcast_addr_max_len_ = sinet_address::DATA_SIZE;
            info.broadcast_addr_.family_ = INET_ADDRESS_FAMILY;
            *info.broadcast_addr_.addr_long() = *info.addr_.addr_long() | (~*info.netmask_addr_.addr_long());
        }
#endif

        opaque_interface_info_current_ = current_addr_info_posix->ifa_next;
#endif

        return sizeof(inet_interface_info);
    }

    inet_socket_interface_iterator::~inet_socket_interface_iterator() {
#if EKA2L1_PLATFORM(WIN32)
        if (opaque_interface_info_) {
            free(opaque_interface_info_);
        }
#else
        freeifaddrs(reinterpret_cast<struct ifaddrs*>(opaque_interface_info_));
#endif
    }

    bool inet_socket_interface_iterator::start() {
        if (opaque_interface_info_) {
            // Restart existing info
#if EKA2L1_PLATFORM(WIN32)
            free(opaque_interface_info_);
#else
            freeifaddrs(reinterpret_cast<struct ifaddrs*>(opaque_interface_info_));
#endif
            opaque_interface_info_ = nullptr;
        }

#if EKA2L1_PLATFORM(WIN32)
        // Recommended by examples
        static constexpr std::size_t INITIAL_INTERFACE_INFO_BUFFER_SIZE = common::KB(15);
        opaque_interface_info_ = malloc(INITIAL_INTERFACE_INFO_BUFFER_SIZE);

        ULONG needed_size = INITIAL_INTERFACE_INFO_BUFFER_SIZE;
        PIP_ADAPTER_ADDRESSES addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(opaque_interface_info_);

        do {
            const DWORD result = GetAdaptersAddresses(AF_UNSPEC, 0, 0, addrs, &needed_size);
            if (result == ERROR_SUCCESS) {
                break;
            }

            if (result == ERROR_BUFFER_OVERFLOW) {
                opaque_interface_info_ = realloc(opaque_interface_info_, INITIAL_INTERFACE_INFO_BUFFER_SIZE);
            } else {
                LOG_ERROR(SERVICE_ESOCK, "Encounter error while trying to retrieve adapter addresses. Error = 0x{:X}", result);
                free(opaque_interface_info_);

                return false;
            }
        } while (true);
#else
        const int result = getifaddrs(reinterpret_cast<struct ifaddrs**>(&opaque_interface_info_));
        if (result < 0) {
            LOG_ERROR(SERVICE_ESOCK, "Encounter error while trying to retrieve interface addresses. Error={}", errno);
            return false;
        }
#endif

        opaque_interface_info_current_ = opaque_interface_info_;
        return true;
    }
}

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

#include <services/bluetooth/protocols/asker_inet.h>

extern "C" {
#include <uv.h>
}

namespace eka2l1::epoc::bt {
    asker_inet::asker_inet()
        : bt_asker_(nullptr)
        , bt_asker_retry_timer_(nullptr) {

    }
    
    asker_inet::~asker_inet() {
        if (bt_asker_retry_timer_) {
            uv_timer_stop(reinterpret_cast<uv_timer_t*>(bt_asker_retry_timer_));

            uv_close(reinterpret_cast<uv_handle_t*>(bt_asker_retry_timer_), [](uv_handle_t *hh) {
                delete hh;
            });
        }

        if (bt_asker_) {
            uv_udp_recv_stop(reinterpret_cast<uv_udp_t*>(bt_asker_));
            uv_close(reinterpret_cast<uv_handle_t*>(bt_asker_), [](uv_handle_t *hh) {
                delete hh;
            });
        }
    }

    static constexpr std::uint32_t SEND_TIMEOUT_RETRY = 100;
    struct send_data_vars {
        uv_buf_t buf_;
        sockaddr_in6 addr_;
        asker_inet::response_callback response_cb_;
        char *response_ptr_ = nullptr;
        std::size_t response_size_;
        uv_udp_send_t *send_req;
        uv_udp_t *send_udp_handle_;
        uv_timer_t *timeout_timer_;
        uv_async_t *async_workload_;
        char *dynamic_buf_data_ = nullptr;
        int times_ = 0;
    };

    enum {
        BT_COMM_INET_ERROR_SEND_FAILED = -1,
        BT_COMM_INET_ERROR_RECV_FAILED = -2,
        BT_COMM_INET_INVALID_ADDR = -3
    };

    static void free_async_workload(send_data_vars *vars_ptr) {
        if (vars_ptr->async_workload_) {
            uv_close(reinterpret_cast<uv_handle_t*>(vars_ptr->async_workload_), [](uv_handle_t *hh) {
                uv_async_t *async_hh = reinterpret_cast<uv_async_t*>(hh);
                delete async_hh;
            });

            vars_ptr->async_workload_ = nullptr;
        }
    }

    static void free_send_data_vars_struct(send_data_vars *vars_ptr) {
        if (vars_ptr->response_ptr_) {
            free(vars_ptr->response_ptr_);
        }

        if (vars_ptr->send_req) {
            delete vars_ptr->send_req;
        }

        if (vars_ptr->dynamic_buf_data_) {
            delete vars_ptr->dynamic_buf_data_;
        }

        free_async_workload(vars_ptr);

        delete vars_ptr;
    }

    static void keep_sending_data(uv_udp_t *handle, send_data_vars *vars_ptr) {
        uv_udp_send_t *send_req = new uv_udp_send_t;
        send_req->data = vars_ptr;
        handle->data = vars_ptr;
        vars_ptr->timeout_timer_->data = vars_ptr;
        vars_ptr->send_req = send_req;
        vars_ptr->send_udp_handle_ = handle;

        uv_async_t *async = new uv_async_t;
        vars_ptr->async_workload_ = async;
        async->data = vars_ptr;

        uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
            send_data_vars *vars_ptr = reinterpret_cast<send_data_vars*>(async->data);

            uv_udp_recv_start(vars_ptr->send_udp_handle_, [](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
                send_data_vars *vars = reinterpret_cast<send_data_vars*>(handle->data);
                if (!vars->response_ptr_) {
                    vars->response_ptr_ = reinterpret_cast<char*>(malloc(suggested_size));
                    vars->response_size_ = suggested_size;
                } else {
                    if (suggested_size > vars->response_size_) {
                        vars->response_ptr_ = reinterpret_cast<char*>(realloc(vars->response_ptr_, suggested_size));
                        vars->response_size_ = suggested_size;
                    }
                }
                buf->base = vars->response_ptr_;
                buf->len = static_cast<std::uint32_t>(suggested_size);
            }, [](uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags) {
                send_data_vars *vars = reinterpret_cast<send_data_vars*>(handle->data);

                uv_timer_stop(vars->timeout_timer_);
                uv_udp_recv_stop(handle);

                if (nread < 0) {
                    if (vars->times_ >= 3) {
                        vars->response_cb_(nullptr, BT_COMM_INET_ERROR_RECV_FAILED);
                        free_send_data_vars_struct(vars);
                    } else {
                        free_async_workload(vars);
                        keep_sending_data(handle, vars);
                    }
                    return;
                }

                vars->response_cb_(buf->base, nread);
                free_send_data_vars_struct(vars);

                return;
            });
            
            const int result = uv_udp_send(vars_ptr->send_req, vars_ptr->send_udp_handle_, &vars_ptr->buf_, 1, reinterpret_cast<sockaddr*>(&vars_ptr->addr_), [](uv_udp_send_t *send_info, int status) {
                send_data_vars *vars = reinterpret_cast<send_data_vars*>(send_info->data);
                vars->times_++;

                if (status >= 0) {
                    uv_timer_start(vars->timeout_timer_, [](uv_timer_t *timer) {
                        send_data_vars *vars = reinterpret_cast<send_data_vars*>(timer->data);

                        uv_udp_recv_stop(vars->send_req->handle);
                        if (vars->times_ >= 3) {
                            vars->response_cb_(nullptr, BT_COMM_INET_ERROR_RECV_FAILED);
                            free_send_data_vars_struct(vars);
                        } else {
                            keep_sending_data(vars->send_req->handle, vars);
                        }
                    }, SEND_TIMEOUT_RETRY, 0);
                } else {
                    uv_udp_recv_stop(send_info->handle);

                    if (vars->times_ >= 3) {
                        vars->response_cb_(nullptr, BT_COMM_INET_ERROR_SEND_FAILED);
                        free_send_data_vars_struct(vars);

                        return;
                    } else {
                        free_async_workload(vars);
                        keep_sending_data(vars->send_req->handle, vars);
                    }
                }
            });

            if (result != 0) {
                LOG_TRACE(SERVICE_BLUETOOTH, "Send asking host info failed with libuv code {}", result);
                if (vars_ptr->times_ >= 3) {
                    vars_ptr->response_cb_(nullptr, BT_COMM_INET_ERROR_SEND_FAILED);
                    free_send_data_vars_struct(vars_ptr);

                    return;
                } else {
                    free_async_workload(vars_ptr);
                    keep_sending_data(vars_ptr->send_udp_handle_, vars_ptr);
                }
            }
        });

        uv_async_send(async);
    }

    void asker_inet::send_request_with_retries(const epoc::socket::saddress &addr, char *request, const std::size_t request_size,
        response_callback response_cb, const bool request_dynamically_allocated) {
        sockaddr *addr_host = nullptr;
        GUEST_TO_BSD_ADDR(addr, addr_host);

        if (addr_host == nullptr) {
            response_cb(nullptr, BT_COMM_INET_INVALID_ADDR);
            if (request_dynamically_allocated) {
                free(request);
            }

            return;
        }

        if (!bt_asker_) {
            uv_udp_t *bt_asker_impl = new uv_udp_t;

            sockaddr_in6 bind;
            std::memset(&bind, 0, sizeof(sockaddr_in6));
            bind.sin6_family = AF_INET6;

            uv_udp_init(uv_default_loop(), bt_asker_impl);
            uv_udp_bind(bt_asker_impl, reinterpret_cast<const sockaddr*>(&bind), 0);

            bt_asker_ = bt_asker_impl;
        }

        if (!bt_asker_retry_timer_) {
            bt_asker_retry_timer_ = new uv_timer_t;
            uv_timer_init(uv_default_loop(), reinterpret_cast<uv_timer_t*>(bt_asker_retry_timer_));
        }

        send_data_vars *vars_ptr = new send_data_vars;

        vars_ptr->buf_ = uv_buf_init(request, static_cast<std::uint32_t>(request_size));
        vars_ptr->timeout_timer_ = reinterpret_cast<uv_timer_t*>(bt_asker_retry_timer_);
        std::memcpy(&vars_ptr->addr_, addr_host, sizeof(sockaddr_in6));

        if (request_dynamically_allocated) {
            vars_ptr->dynamic_buf_data_ = request;
        }

        vars_ptr->response_cb_ = response_cb;
        keep_sending_data(reinterpret_cast<uv_udp_t*>(bt_asker_), vars_ptr);
    }

    std::uint32_t asker_inet::ask_for_routed_port(const std::uint16_t virtual_port, const epoc::socket::saddress &dev_addr) {
        ask_routed_port_wait_evt_.reset();
        std::uint32_t local_result = 0;

        ask_for_routed_port_async(virtual_port, dev_addr, [&local_result, this](const std::int64_t res) {
            if (res <= 0) {
                local_result = 0;
            } else {
                local_result = static_cast<std::uint32_t>(res);
            }

            ask_routed_port_wait_evt_.set();
        });

        ask_routed_port_wait_evt_.wait();
        return local_result;
    }

    void asker_inet::ask_for_routed_port_async(const std::uint16_t virtual_port, const epoc::socket::saddress &dev_addr, port_ask_done_callback cb) {
        char *buf = new char[4];
        buf[0] = 'p';
        buf[1] = 'l';
        buf[2] = static_cast<char>(virtual_port & 0xFF);
        buf[3] = static_cast<char>((virtual_port >> 8) & 0xFF);

        send_request_with_retries(dev_addr, buf, 4, [cb](const char *response, const ssize_t size) {
            if (size <= 0) {
                cb(static_cast<std::int64_t>(size));
            } else {
                cb(*reinterpret_cast<const std::int64_t*>(response));
            }
        }, true);
    }
}

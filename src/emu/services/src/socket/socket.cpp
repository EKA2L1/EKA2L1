/*
 * Copyright (c) 2021 EKA2L1 Team
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

#include <services/socket/server.h>
#include <services/socket/socket.h>

#include <utils/err.h>
#include <system/epoc.h>

namespace eka2l1::epoc::socket {
    std::size_t socket::get_option(const std::uint32_t option_id, const std::uint32_t option_family,
        std::uint8_t *buffer, const std::size_t avail_size) {
        LOG_ERROR(SERVICE_ESOCK, "Unhandled base option family {} (id {})", option_family, option_id);
        return 0;
    }

    bool socket::set_option(const std::uint32_t option_id, const std::uint32_t option_family,
        std::uint8_t *buffer, const std::size_t avail_size) {
        if (option_family == epoc::socket::SOCKET_OPTION_FAMILY_BASE) {
            switch (option_id) {
            case epoc::socket::SOCKET_OPTION_ID_BLOCKING_IO:
                LOG_INFO(SERVICE_BLUETOOTH, "Set Blocking IO stubbed");
                return true;

            default:
                break;
            }
        }

        LOG_ERROR(SERVICE_ESOCK, "Unhandled base option family {} (id {})", option_family, option_id);
        return false;
    }

    void socket::bind(const saddress &addr, epoc::notify_info &info) {
        LOG_ERROR(SERVICE_ESOCK, "Binding socket unimplemented");
        info.complete(epoc::error_not_supported);
    }

    void socket::connect(const saddress &addr, epoc::notify_info &info) {
        LOG_ERROR(SERVICE_ESOCK, "Connecting socket unimplemented");
        info.complete(epoc::error_not_supported);
    }

    void socket::ioctl(const std::uint32_t command, epoc::notify_info &complete_info, std::uint8_t *buffer,
        const std::size_t available_size, const std::size_t max_buffer_size, const std::uint32_t level) {
        LOG_ERROR(SERVICE_ESOCK, "Unhandled base IOCTL (command={})", command);
    }

    void socket::send(const std::uint8_t *data, const std::uint32_t data_size, std::uint32_t *sent_size, const saddress *addr,
        std::uint32_t flags, epoc::notify_info &complete_info) {
        LOG_ERROR(SERVICE_ESOCK, "Sending data to socket unimplemented!");
    }

    void socket::receive(std::uint8_t *data, const std::uint32_t data_size, std::uint32_t *sent_size, saddress *addr,
        std::uint32_t flags, epoc::notify_info &complete_info, receive_done_callback cb) {
        LOG_ERROR(SERVICE_ESOCK, "Receiving data from socket unimplemented!");
    }

    std::int32_t socket::local_name(saddress &result, std::uint32_t &result_len) {
        LOG_ERROR(SERVICE_ESOCK, "Get socket's local name unimplemented!");
        return epoc::error_not_supported;
    }
    
    std::int32_t socket::remote_name(saddress &result, std::uint32_t &result_len) {
        LOG_ERROR(SERVICE_ESOCK, "Get socket's remote name unimplemented!");
        return epoc::error_not_supported;
    }
    
    std::int32_t socket::listen(const std::uint32_t backlog) {
        LOG_ERROR(SERVICE_ESOCK, "Socket listening unimplemented!");
        return epoc::error_not_supported;
    }

    void socket::accept(std::unique_ptr<socket> *pending_sock, epoc::notify_info &complete_info) {
        LOG_ERROR(SERVICE_ESOCK, "Socket accept unimplemented!");
        complete_info.complete(epoc::error_not_supported);
    }

    void socket::cancel_receive() {
        LOG_ERROR(SERVICE_ESOCK, "Cancel receive unimplemented!");
    }

    void socket::cancel_send() {
        LOG_ERROR(SERVICE_ESOCK, "Cancel send unimplemented!");
    }

    void socket::cancel_connect() {
        LOG_ERROR(SERVICE_ESOCK, "Cancel connect unimplemented!");
    }

    void socket::cancel_accept() {
        LOG_ERROR(SERVICE_ESOCK, "Cancel accept unimplemented!");
    }

    void socket::shutdown(epoc::notify_info &complete_info, int reason) {
        LOG_ERROR(SERVICE_ESOCK, "Shutdown not implemented!");
    }

    socket_socket::socket_socket(socket_client_session *parent, std::unique_ptr<socket> &sock)
        : socket_subsession(parent)
        , sock_(std::move(sock)) {
    }

    struct oldarch_option_description {
        std::uint32_t id_;
        eka2l1::ptr<epoc::desc8> data_;
        std::uint32_t data_size_;
    };

    void socket_socket::get_option(service::ipc_context *ctx) {
        std::optional<std::uint32_t> option_name;
        std::optional<std::uint32_t> option_fam;

        std::uint8_t *dest_buffer = nullptr;
        std::size_t max_size = 0;

        kernel::process *caller = ctx->msg->own_thr->owning_process();
        epoc::desc8 *custom_buf = nullptr;

        if (parent_->is_oldarch()) {
            auto desc_data = ctx->get_argument_data_from_descriptor<oldarch_option_description>(0);
            if (!desc_data.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            option_name = desc_data->id_;
            max_size = desc_data->data_size_;
            option_fam = ctx->get_argument_value<std::uint32_t>(1);

            epoc::desc8 *data_des_real = desc_data->data_.get(caller);
            if (!data_des_real) {
                ctx->complete(epoc::error_argument);
                return;
            }

            dest_buffer = reinterpret_cast<std::uint8_t *>(data_des_real->get_pointer(caller));
            custom_buf = data_des_real;
        } else {
            option_name = ctx->get_argument_value<std::uint32_t>(0);
            option_fam = ctx->get_argument_value<std::uint32_t>(2);

            dest_buffer = ctx->get_descriptor_argument_ptr(1);
            max_size = ctx->get_argument_max_data_size(1);
        }

        if (!option_name || !option_fam || !dest_buffer) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const std::size_t res = sock_->get_option(option_name.value(), option_fam.value(), dest_buffer, max_size);
        if (IS_SOCKET_GETOPT_ERROR(res)) {
            ctx->complete(static_cast<int>(GET_SOCKET_GETOPT_ERROR(res)));
            return;
        }

        if (custom_buf) {
            custom_buf->set_length(caller, static_cast<std::uint32_t>(res));
        } else {
            ctx->set_descriptor_argument_length(1, static_cast<std::uint32_t>(res));
        }

        ctx->complete(epoc::error_none);
    }

    void socket_socket::set_option(service::ipc_context *ctx) {
        std::optional<std::uint32_t> option_name;
        std::optional<std::uint32_t> option_fam;

        std::uint8_t *source_buffer = nullptr;
        std::size_t max_size = 0;

        kernel::process *caller = ctx->msg->own_thr->owning_process();

        if (parent_->is_oldarch()) {
            auto desc_data = ctx->get_argument_data_from_descriptor<oldarch_option_description>(0);
            if (!desc_data.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            option_name = desc_data->id_;
            max_size = desc_data->data_size_;
            option_fam = ctx->get_argument_value<std::uint32_t>(1);

            epoc::desc8 *data_des_real = desc_data->data_.get(caller);
            if (data_des_real) {
                source_buffer = reinterpret_cast<std::uint8_t *>(data_des_real->get_pointer(caller));
            }
        } else {
            option_name = ctx->get_argument_value<std::uint32_t>(0);
            option_fam = ctx->get_argument_value<std::uint32_t>(2);

            source_buffer = ctx->get_descriptor_argument_ptr(1);
            max_size = ctx->get_argument_max_data_size(1);
        }

        if (!option_name || !option_fam) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const bool res = sock_->set_option(option_name.value(), option_fam.value(), source_buffer, max_size);
        if (!res) {
            LOG_ERROR(SERVICE_ESOCK, "Fail to set value of socket option!");
            ctx->complete(epoc::error_general);

            return;
        }

        ctx->complete(epoc::error_none);
    }

    void socket_socket::bind(service::ipc_context *ctx) {
        std::optional<saddress> sockaddr = ctx->get_argument_data_from_descriptor<saddress>(0);

        if (!sockaddr.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);
        sock_->bind(sockaddr.value(), info);
    }
    
    void socket_socket::connect(service::ipc_context *ctx) {
        std::optional<saddress> sockaddr = ctx->get_argument_data_from_descriptor<saddress>(0);

        if (!sockaddr.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);
        sock_->connect(sockaddr.value(), info);
    }

    void socket_socket::write(service::ipc_context *ctx) {
        std::uint8_t *packet_buffer = ctx->get_descriptor_argument_ptr(2);
        std::size_t packet_size = ctx->get_argument_data_size(2);

        if (!packet_buffer || !packet_size) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);
        sock_->send(packet_buffer, static_cast<std::uint32_t>(packet_size), nullptr, nullptr, 0, info);
    }

    void socket_socket::read(service::ipc_context *ctx) {
        std::uint8_t *packet_buffer = ctx->get_descriptor_argument_ptr(2);
        std::size_t packet_size = ctx->get_argument_max_data_size(2);

        if (!packet_buffer || !packet_size) {
            ctx->complete(epoc::error_argument);
            return;
        }
        
        kernel::process *requester = ctx->msg->own_thr->owning_process();
        epoc::des8 *packet_des = eka2l1::ptr<epoc::des8>(ctx->msg->args.args[2]).get(requester);
        
        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);
        sock_->receive(packet_buffer, static_cast<std::uint32_t>(packet_size), nullptr, nullptr, 0, info,
            [packet_des, requester](const std::int64_t length) {
                if (length < 0) {
                    packet_des->set_length(requester, 0);
                } else {
                    packet_des->set_length(requester, common::min<std::uint32_t>(static_cast<std::uint32_t>(length), packet_des->get_max_length(requester)));
                }
            });
    }

    void socket_socket::send(service::ipc_context *ctx, const bool has_return_length, const bool has_addr) {
        std::optional<std::uint32_t> flags = std::nullopt;
        std::uint8_t *packet_buffer = ctx->get_descriptor_argument_ptr(2);
        std::size_t packet_size = ctx->get_argument_data_size(2);

        if (!packet_buffer || !packet_size) {
            ctx->complete(epoc::error_argument);
            return;
        }
        
        std::uint32_t *size_return = nullptr;
        if (has_return_length) {
            size_return = reinterpret_cast<std::uint32_t*>(ctx->get_descriptor_argument_ptr(0));
            if (!size_return) {
                ctx->complete(epoc::error_argument);
                return;
            }

            flags = *size_return;
        } else {
            flags = ctx->get_argument_value<std::uint32_t>(0);
        }

        if (!flags.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::optional<saddress> optional_addr = std::nullopt;
        if (has_addr) {
            optional_addr = ctx->get_argument_data_from_descriptor<saddress>(1, true);
            if (!optional_addr.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }
        }

        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);
        sock_->send(packet_buffer, static_cast<std::uint32_t>(packet_size), size_return, optional_addr.has_value() ? &optional_addr.value() : nullptr, flags.value(), info);
    }

    void socket_socket::recv(service::ipc_context *ctx, const bool has_return_length, const bool one_or_more, const bool has_addr) {
        std::optional<std::uint32_t> flags = std::nullopt;
        std::uint8_t *packet_buffer = ctx->get_descriptor_argument_ptr(2);
        std::size_t packet_size = ctx->get_argument_max_data_size(2);

        if (!packet_buffer || !packet_size) {
            ctx->complete(epoc::error_argument);
            return;
        }

        kernel::process *requester = ctx->msg->own_thr->owning_process();
        epoc::des8 *packet_des = eka2l1::ptr<epoc::des8>(ctx->msg->args.args[2]).get(requester);
        
        std::uint32_t *size_return = nullptr;
        if (has_return_length && (!one_or_more || has_addr)) {
            size_return = reinterpret_cast<std::uint32_t*>(ctx->get_descriptor_argument_ptr(0));
            if (!size_return) {
                ctx->complete(epoc::error_argument);
                return;
            }

            // Not in S^3 I think, but they store the flag in this variable in s60v5 and down
            // Layout in S^3 makes more sense. They still do this in some case where things are not fit though in s^3.
            flags = *size_return;
        } else {
            flags = ctx->get_argument_value<std::uint32_t>(0);

            // On S^3 and probably older version the layout is still the same like this.
            // First is flags, second is length pointer and third is buffer
            if (one_or_more && has_return_length) {
                size_return = reinterpret_cast<std::uint32_t*>(ctx->get_descriptor_argument_ptr(1));
            }
        }

        if (!flags.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        saddress *address_ptr = nullptr;
        if (has_addr) {
            address_ptr = reinterpret_cast<saddress*>(ctx->get_descriptor_argument_ptr(1));
            if (!address_ptr) {
                ctx->complete(epoc::error_argument);
                return;
            }
        }

        if (one_or_more) {
            flags.value() |= SOCKET_FLAG_DONT_WAIT_FULL;
        }

        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);
        sock_->receive(packet_buffer, static_cast<std::uint32_t>(packet_size), size_return, address_ptr, flags.value(), info,
            [packet_des, requester](const std::int64_t length) {
                if (length < 0) {
                    packet_des->set_length(requester, 0);
                } else {
                    packet_des->set_length(requester, common::min<std::uint32_t>(static_cast<std::uint32_t>(length), packet_des->get_max_length(requester)));
                }
            });
    }
    
    void socket_socket::send_old(service::ipc_context *ctx, const bool has_addr) {
        std::uint8_t *packet_buffer = ctx->get_descriptor_argument_ptr(2);
        std::size_t packet_size = ctx->get_argument_data_size(2);

        if (!packet_buffer || !packet_size) {
            ctx->complete(epoc::error_argument);
            return;
        }
        
        std::optional<socket_old_rw_req_info> req_info = ctx->get_argument_data_from_descriptor<socket_old_rw_req_info>(0);
        if (!req_info.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        kernel::process *requester = ctx->msg->own_thr->owning_process();
        epoc::des8 *size_return_des = eka2l1::ptr<epoc::des8>(req_info->size_return_).get(requester);
        saddress *optional_addr = (has_addr ? eka2l1::ptr<saddress>(req_info->sock_addr_).get(requester) : nullptr);

        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);
        sock_->send(packet_buffer, static_cast<std::uint32_t>(packet_size), reinterpret_cast<std::uint32_t*>(size_return_des->get_pointer_raw(requester)),
            optional_addr, req_info->flags_, info);
    }

    void socket_socket::recv_old(service::ipc_context *ctx, const bool one_or_more, const bool has_addr) {
        std::uint8_t *packet_buffer = ctx->get_descriptor_argument_ptr(2);
        std::size_t packet_size = ctx->get_argument_max_data_size(2);

        if (!packet_buffer || !packet_size) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::optional<socket_old_rw_req_info> req_info = ctx->get_argument_data_from_descriptor<socket_old_rw_req_info>(0);
        if (!req_info.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        kernel::process *requester = ctx->msg->own_thr->owning_process();
        epoc::des8 *packet_des = eka2l1::ptr<epoc::des8>(ctx->msg->args.args[2]).get(requester);

        if (one_or_more) {
            req_info->flags_ |= SOCKET_FLAG_DONT_WAIT_FULL;
        }

        epoc::des8 *size_return_des = eka2l1::ptr<epoc::des8>(req_info->size_return_).get(requester);
        saddress *optional_addr = (has_addr ? eka2l1::ptr<saddress>(req_info->sock_addr_).get(requester) : nullptr);

        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);
        sock_->receive(packet_buffer, static_cast<std::uint32_t>(packet_size), reinterpret_cast<std::uint32_t*>(size_return_des->get_pointer_raw(requester)), optional_addr, req_info->flags_, info,
            [packet_des, requester](const std::int64_t length) {
                if (length < 0) {
                    packet_des->set_length(requester, 0);
                } else {
                    packet_des->set_length(requester, common::min<std::uint32_t>(static_cast<std::uint32_t>(length), packet_des->get_max_length(requester)));
                }
            });
    }
    
    void socket_socket::ioctl(service::ipc_context *ctx) {
        std::optional<std::uint32_t> command = ctx->get_argument_value<std::uint32_t>(0);
        std::optional<std::uint32_t> level = ctx->get_argument_value<std::uint32_t>(2);
        std::uint8_t *buffer = ctx->get_descriptor_argument_ptr(1);
        std::size_t buffer_size = ctx->get_argument_data_size(1);
        std::size_t max_buffer_size = ctx->get_argument_max_data_size(1);

        if (!command.has_value() || !level.has_value()) {
            ctx->complete(epoc::error_none);
            return;
        }

        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);
        sock_->ioctl(command.value(), info, buffer, buffer_size, max_buffer_size, level.value());
    }

    void socket_socket::local_name(service::ipc_context *ctx) {
        epoc::socket::saddress addr;
        std::uint32_t addr_real_size = 0;

        const std::int32_t res = sock_->local_name(addr, addr_real_size);
        if (res != epoc::error_none) {
            ctx->complete(res);
            return;
        }

        ctx->write_data_to_descriptor_argument<epoc::socket::saddress>(0, addr);
        ctx->set_descriptor_argument_length(0, addr_real_size);
        ctx->complete(epoc::error_none);
    }
    
    void socket_socket::remote_name(service::ipc_context *ctx) {
        epoc::socket::saddress addr;
        std::uint32_t addr_real_size = 0;

        const std::int32_t res = sock_->remote_name(addr, addr_real_size);
        if (res != epoc::error_none) {
            ctx->complete(res);
            return;
        }

        ctx->write_data_to_descriptor_argument<epoc::socket::saddress>(0, addr);
        ctx->set_descriptor_argument_length(0, addr_real_size);
        ctx->complete(epoc::error_none);
    }

    void socket_socket::listen(service::ipc_context *ctx) {
        std::optional<std::uint32_t> backlog = ctx->get_argument_value<std::uint32_t>(0);
        if (!backlog.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        ctx->complete(sock_->listen(backlog.value()));
    }

    void socket_socket::accept(service::ipc_context *ctx) {
        std::optional<std::uint32_t> subsess_id = ctx->get_argument_value<std::uint32_t>(1);

        if (subsess_id .has_value() && (subsess_id.value() > 0)) {
            socket_subsession_instance *inst = parent_->subsessions_.get(subsess_id.value());

            if (inst) {
                socket_socket *empty_socket = reinterpret_cast<socket_socket*>(inst->get());
                if (empty_socket->type() != socket_subsession_type_socket) {
                    LOG_ERROR(SERVICE_ESOCK, "Accepting provide non-socket handle!");

                    ctx->complete(epoc::error_bad_handle);
                    return;
                }

                epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);
                sock_->accept(&empty_socket->sock_, info);
            } else {
                ctx->complete(epoc::error_bad_handle);
                return;
            }
        } else {
            ctx->complete(epoc::error_argument);
        }
    }

    void socket_socket::cancel_send(service::ipc_context *ctx) {
        sock_->cancel_send();
        ctx->complete(epoc::error_none);
    }

    void socket_socket::cancel_recv(service::ipc_context *ctx) {
        sock_->cancel_receive();
        ctx->complete(epoc::error_none);
    }

    void socket_socket::cancel_accept(service::ipc_context *ctx) {
        sock_->cancel_accept();
        ctx->complete(epoc::error_none);
    }

    void socket_socket::cancel_all(service::ipc_context *ctx) {
        sock_->cancel_all();
        ctx->complete(epoc::error_none);
    }

    void socket_socket::close(service::ipc_context *ctx) {
        parent_->subsessions_.remove(id_);
        ctx->complete(epoc::error_none);
    }

    void socket_socket::shutdown(service::ipc_context *ctx) {
        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);
        std::optional<int> reason = ctx->get_argument_value<int>(0);

        sock_->shutdown(info, reason.value());
    }

    void socket_socket::dispatch(service::ipc_context *ctx) {
        if (parent_->is_oldarch()) {
            switch (ctx->msg->function) {
            case socket_old_so_set_opt:
                set_option(ctx);
                return;

            case socket_old_so_get_opt:
                get_option(ctx);
                return;

            case socket_old_so_close:
                close(ctx);
                return;

            case socket_old_so_ioctl:
                ioctl(ctx);
                return;

            case socket_old_so_bind:
                bind(ctx);
                return;

            case socket_old_so_listen:
                listen(ctx);
                return;

            case socket_old_so_accept:
                accept(ctx);
                return;

            case socket_old_so_cancel_accept:
                cancel_accept(ctx);
                return;

            case socket_old_so_local_name:
                local_name(ctx);
                return;

            case socket_old_so_remote_name:
                remote_name(ctx);
                return;

            case socket_old_so_write:
                write(ctx);
                return;

            case socket_old_so_read:
                read(ctx);
                return;

            case socket_old_so_send:
                send_old(ctx, false);
                return;

            case socket_old_so_recv:
                recv_old(ctx, false, false);
                return;

            case socket_old_so_recv_one_or_more:
                recv_old(ctx, true, false);
                return;

            case socket_old_so_recvfrom:
                recv_old(ctx, false, true);
                return;

            case socket_old_so_sendto:
                send_old(ctx, true);
                return;

            case socket_old_so_cancel_send:
                cancel_send(ctx);
                return;

            case socket_old_so_cancel_recv:
                cancel_recv(ctx);
                return;

            case socket_old_so_cancel_connect:
                sock_->cancel_connect();
                ctx->complete(epoc::error_none);

                return;

            case socket_old_so_cancel_all:
                cancel_all(ctx);
                return;

            case socket_old_so_connect:
                connect(ctx);
                return;

            case socket_old_so_shutdown:
                shutdown(ctx);
                return;

            default:
                break;
            }
        } else {
            if (ctx->sys->get_symbian_version_use() >= epocver::epoc95) {
                switch (ctx->msg->function) {
                case socket_reform_so_get_opt:
                    get_option(ctx);
                    return;

                case socket_reform_so_close:
                    close(ctx);
                    return;

                case socket_reform_so_connect:
                    connect(ctx);
                    return;

                case socket_reform_so_bind:
                    bind(ctx);
                    return;

                case socket_reform_so_write:
                    write(ctx);
                    return;

                case socket_reform_so_send:
                    send(ctx, true, false);
                    return;

                case socket_reform_so_send_no_len:
                    send(ctx, false, false);
                    return;

                case socket_reform_so_recv:
                    recv(ctx, false, false, false);
                    return;

                case socket_reform_so_recv_no_len:
                    recv(ctx, true, false, false);
                    return;

                case socket_reform_so_recv_one_or_more:
                    recv(ctx, true, true, false);
                    return;

                case socket_reform_so_ioctl:
                    ioctl(ctx);
                    return;

                case socket_reform_so_cancel_send:
                    cancel_send(ctx);
                    return;

                case socket_reform_so_cancel_recv:
                    cancel_recv(ctx);
                    return;

                case socket_reform_so_cancel_connect:
                    sock_->cancel_connect();
                    ctx->complete(epoc::error_none);

                    return;

                case socket_reform_so_set_opt:
                    set_option(ctx);
                    return;

                case socket_reform_so_local_name:
                    local_name(ctx);
                    return;

                case socket_reform_so_remote_name:
                    remote_name(ctx);
                    return;

                case socket_reform_so_cancel_all:
                    cancel_all(ctx);
                    return;

                case socket_reform_so_send_to_no_len:
                    send(ctx, false, true);
                    return;

                case socket_reform_so_recv_from:
                    recv(ctx, true, true, true);
                    return;

                case socket_reform_so_accept:
                    accept(ctx);
                    return;

                case socket_reform_so_listen:
                    listen(ctx);
                    return;

                default:
                    break;
                }
            } else {
                switch (ctx->msg->function) {
                case socket_so_get_opt:
                    get_option(ctx);
                    return;

                case socket_so_close:
                    close(ctx);
                    return;

                case socket_so_connect:
                    connect(ctx);
                    return;

                case socket_so_bind:
                    bind(ctx);
                    return;

                case socket_so_write:
                    write(ctx);
                    return;

                case socket_so_send:
                    send(ctx, true, false);
                    return;

                case socket_so_send_no_len:
                    send(ctx, false, false);
                    return;

                case socket_so_recv:
                    recv(ctx, false, false, false);
                    return;

                case socket_so_recv_no_len:
                    recv(ctx, true, false, false);
                    return;

                case socket_so_recv_one_or_more:
                    recv(ctx, true, true, false);
                    return;

                case socket_so_ioctl:
                    ioctl(ctx);
                    return;

                case socket_so_cancel_send:
                    cancel_send(ctx);
                    return;

                case socket_so_cancel_recv:
                    cancel_recv(ctx);
                    return;

                case socket_so_cancel_connect:
                    sock_->cancel_connect();
                    ctx->complete(epoc::error_none);

                    return;

                case socket_so_local_name:
                    local_name(ctx);
                    return;

                case socket_so_remote_name:
                    remote_name(ctx);
                    return;

                default:
                    break;
                }
            }
        }

        LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket opcode: {}", ctx->msg->function);
    }
}
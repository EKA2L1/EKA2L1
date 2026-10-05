/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include <control/dispatcher.h>
#include <control/rpc_server.h>

#include <common/log.h>
#include <common/platform.h>
#include <common/thread.h>

#include <uvw.hpp>

#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#if !EKA2L1_PLATFORM(WIN32)
#include <cerrno>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace eka2l1::control {
    // A line longer than this is not a message anyone meant to send.
    static constexpr std::size_t MAX_MESSAGE_SIZE = 4 * 1024 * 1024;

    // Until a connection passes auth (when a token is required), its lines are held to this.
    static constexpr std::size_t MAX_UNAUTHORIZED_MESSAGE_SIZE = 64 * 1024;

    // A connection whose unsent answers exceed this is neither read nor served, and misses its
    // notifications, until the client has read them down to half of it.
    static constexpr std::size_t MAX_QUEUED_BYTES = 8 * 1024 * 1024;

    // How long stop() lets connections flush their answers before it closes them anyway.
    static constexpr uvw::timer_handle::time SHUTDOWN_GRACE{ 2000 };

    namespace {
        struct connection {
            session client;
            std::string pending;
            bool closing = false;
            bool throttled = false;

            std::function<void(std::string)> write_line;
            std::function<std::size_t()> queued_bytes;
            std::function<void(bool)> set_reading;

            // Flush what is queued, then close.
            std::function<void()> finish;

            // Close now, dropping what is queued.
            std::function<void()> close_now;
        };

        std::string describe(const int libuv_error) {
            return std::string(uv_err_name(libuv_error)) + ": " + uv_strerror(libuv_error);
        }

#if !EKA2L1_PLATFORM(WIN32)
        // Remove a socket file whose server is gone, so a crashed run does not block the next one.
        bool clear_stale_socket(const std::string &path, std::string &error) {
            sockaddr_un address{};

            if (path.size() >= sizeof(address.sun_path)) {
                error = "The control socket path is longer than " + std::to_string(sizeof(address.sun_path) - 1) + " bytes: " + path;
                return false;
            }

            struct stat info;
            if (lstat(path.c_str(), &info) != 0) {
                return true;
            }

            if (!S_ISSOCK(info.st_mode)) {
                error = path + " exists and is not a socket";
                return false;
            }

            address.sun_family = AF_UNIX;
            std::memcpy(address.sun_path, path.c_str(), path.size() + 1);

            const int probe = socket(AF_UNIX, SOCK_STREAM, 0);
            if (probe < 0) {
                error = std::string("Cannot create a socket: ") + std::strerror(errno);
                return false;
            }

            const int result = connect(probe, reinterpret_cast<sockaddr *>(&address), sizeof(address));
            const int connect_error = errno;
            close(probe);

            if (result == 0) {
                error = "Another process is already listening on " + path;
                return false;
            }

            if (connect_error != ECONNREFUSED) {
                error = "Cannot tell whether " + path + " is in use: " + std::strerror(connect_error);
                return false;
            }

            unlink(path.c_str());
            return true;
        }
#endif
    }

    class rpc_server_impl {
        dispatcher &rpc_;

        std::shared_ptr<uvw::loop> loop_;
        std::shared_ptr<uvw::async_handle> wakeup_;
        std::function<void()> close_listener_;
        std::set<std::shared_ptr<connection>> connections_;
        std::shared_ptr<uvw::timer_handle> grace_timer_;
        std::thread thread_;
        std::string socket_path_;
        std::string listening_on_;

        std::mutex outbox_mut_;
        std::vector<std::pair<std::string, std::string>> outbox_;
        bool accepting_ = false;
        bool stop_requested_ = false;

        std::size_t line_limit(const connection &conn) const {
            return rpc_.is_authorized(conn.client) ? MAX_MESSAGE_SIZE : MAX_UNAUTHORIZED_MESSAGE_SIZE;
        }

        void refuse_long_line(connection &conn) {
            const char *message = rpc_.is_authorized(conn.client)
                ? R"({"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"A message must end with a newline within 4 MiB"}})"
                : R"({"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Before auth, a message must end with a newline within 64 KiB"}})";

            conn.write_line(message);
            conn.finish();
        }

        void send(connection &conn, const std::string &line) {
            conn.write_line(line);

            if (!conn.throttled && (conn.queued_bytes() > MAX_QUEUED_BYTES)) {
                conn.throttled = true;
                conn.set_reading(false);
            }
        }

        // Handle the complete lines received so far, unless the connection is throttled.
        void serve_pending(connection &conn) {
            std::size_t start = 0;
            std::size_t newline = 0;

            while (!conn.throttled && ((newline = conn.pending.find('\n', start)) != std::string::npos)) {
                std::string_view line(conn.pending.data() + start, newline - start);
                start = newline + 1;

                if (line.size() > line_limit(conn)) {
                    refuse_long_line(conn);
                    return;
                }

                if (!line.empty() && (line.back() == '\r')) {
                    line.remove_suffix(1);
                }

                if (line.find_first_not_of(" \t") == std::string_view::npos) {
                    continue;
                }

                const std::string response = rpc_.handle(conn.client, line);

                if (!response.empty()) {
                    send(conn, response);
                }

                // One wrong token per connection.
                if (conn.client.rejected) {
                    conn.finish();
                }

                if (conn.closing) {
                    return;
                }
            }

            conn.pending.erase(0, start);

            // A throttled connection is not read, so what is left cannot grow.
            if (!conn.throttled && (conn.pending.size() > line_limit(conn))) {
                refuse_long_line(conn);
            }
        }

        void handle_data(connection &conn, const char *data, const std::size_t length) {
            if (conn.closing) {
                return;
            }

            conn.pending.append(data, length);
            serve_pending(conn);
        }

        // Some answers went out: serve a throttled connection again once its client caught up.
        void handle_written(connection &conn) {
            if (!conn.throttled || conn.closing || (conn.queued_bytes() > MAX_QUEUED_BYTES / 2)) {
                return;
            }

            conn.throttled = false;
            serve_pending(conn);

            if (!conn.throttled && !conn.closing) {
                conn.set_reading(true);
            }
        }

        std::size_t open_connections() const {
            std::size_t count = 0;

            for (const std::shared_ptr<connection> &conn : connections_) {
                count += conn->closing ? 0 : 1;
            }

            return count;
        }

        template <typename H>
        void accept(H &listener) {
            std::shared_ptr<H> client = listener.parent().template resource<H>();

            if (!client || (listener.accept(*client) != 0)) {
                LOG_WARN(FRONTEND_CONTROL, "Failed to accept a control connection");
                return;
            }

            auto conn = std::make_shared<connection>();
            std::weak_ptr<connection> weak_conn = conn;

            conn->write_line = [client](std::string line) {
                line.push_back('\n');

                const std::size_t size = line.size();
                std::unique_ptr<char[]> data(new char[size]);
                std::memcpy(data.get(), line.data(), size);

                client->write(std::move(data), static_cast<unsigned int>(size));
            };

            conn->queued_bytes = [client]() {
                return client->write_queue_size();
            };

            conn->set_reading = [client](const bool reading) {
                if (reading) {
                    client->read();
                } else {
                    client->stop();
                }
            };

            conn->finish = [client, weak_conn]() {
                if (std::shared_ptr<connection> self = weak_conn.lock()) {
                    self->closing = true;
                }

                client->stop();

                if (client->shutdown() != 0) {
                    client->close();
                }
            };

            conn->close_now = [client]() {
                client->close();
            };

            client->template on<uvw::data_event>([this, weak_conn](const uvw::data_event &event, H &) {
                if (std::shared_ptr<connection> self = weak_conn.lock()) {
                    handle_data(*self, event.data.get(), event.length);
                }
            });

            // The client sent all it will send: finish writing the answers, then close.
            client->template on<uvw::end_event>([weak_conn](const uvw::end_event &, H &handle) {
                if (std::shared_ptr<connection> self = weak_conn.lock()) {
                    self->finish();
                } else {
                    handle.close();
                }
            });
            client->template on<uvw::write_event>([this, weak_conn](const uvw::write_event &, H &) {
                if (std::shared_ptr<connection> self = weak_conn.lock()) {
                    handle_written(*self);
                }
            });
            client->template on<uvw::shutdown_event>([](const uvw::shutdown_event &, H &handle) { handle.close(); });
            client->template on<uvw::error_event>([](const uvw::error_event &, H &handle) { handle.close(); });

            client->template on<uvw::close_event>([this, weak_conn](const uvw::close_event &, H &) {
                if (std::shared_ptr<connection> self = weak_conn.lock()) {
                    connections_.erase(self);
                }

                // Shutting down: the loop can end before the grace period is over.
                if (grace_timer_ && connections_.empty()) {
                    grace_timer_->close();
                }
            });

            const bool over_limit = open_connections() >= rpc_server::MAX_CONNECTIONS;
            connections_.insert(conn);

            if (over_limit) {
                conn->write_line(R"({"jsonrpc":"2.0","id":null,"error":{"code":-32003,"message":"The control server serves )"
                    + std::to_string(rpc_server::MAX_CONNECTIONS) + R"( connections at most; close one first"}})");
                conn->finish();
                return;
            }

            client->read();
        }

        template <typename H>
        void serve(std::shared_ptr<H> listener) {
            listener->template on<uvw::listen_event>([this](const uvw::listen_event &, H &handle) {
                accept(handle);
            });

            listener->template on<uvw::error_event>([](const uvw::error_event &event, H &) {
                LOG_ERROR(FRONTEND_CONTROL, "Control server listener failed: {}", event.what());
            });

            close_listener_ = [listener]() {
                listener->close();
            };
        }

        void handle_wakeup() {
            std::vector<std::pair<std::string, std::string>> outbox;
            bool stop = false;

            {
                const std::lock_guard<std::mutex> guard(outbox_mut_);
                outbox.swap(outbox_);
                stop = stop_requested_;
            }

            for (const auto &[topic, line] : outbox) {
                for (const std::shared_ptr<connection> &conn : connections_) {
                    if (!conn->closing && !conn->throttled && conn->client.subscriptions.count(topic)) {
                        send(*conn, line);
                    }
                }
            }

            if (!stop) {
                return;
            }

            if (close_listener_) {
                close_listener_();
                close_listener_ = nullptr;
            }

            // finish() can drop the connection from the set, so walk a copy.
            const std::set<std::shared_ptr<connection>> open = connections_;
            for (const std::shared_ptr<connection> &conn : open) {
                conn->finish();
            }

            // A client that stopped reading would hold its connection, and so the loop and stop(),
            // open for good: close what is left after a grace period.
            if (!connections_.empty()) {
                grace_timer_ = loop_->resource<uvw::timer_handle>();

                if (grace_timer_) {
                    grace_timer_->on<uvw::timer_event>([this](const uvw::timer_event &, uvw::timer_handle &timer) {
                        const std::set<std::shared_ptr<connection>> left = connections_;
                        for (const std::shared_ptr<connection> &conn : left) {
                            conn->close_now();
                        }

                        timer.close();
                    });

                    grace_timer_->start(SHUTDOWN_GRACE, uvw::timer_handle::time{ 0 });
                } else {
                    for (const std::shared_ptr<connection> &conn : open) {
                        conn->close_now();
                    }
                }
            }

            wakeup_->close();
        }

        bool listen(const endpoint &where, std::string &error) {
            if (where.kind == endpoint::local) {
                std::string path = where.address;

#if !EKA2L1_PLATFORM(WIN32)
                // The emulator changes its working directory as it runs; stop() must still find the file.
                std::error_code path_error;
                path = std::filesystem::absolute(path, path_error).string();

                if (path_error) {
                    error = "Cannot tell where " + where.address + " is: " + path_error.message();
                    return false;
                }

                if (!clear_stale_socket(path, error)) {
                    return false;
                }
#endif

                std::shared_ptr<uvw::pipe_handle> pipe = loop_->resource<uvw::pipe_handle>();
                if (!pipe) {
                    error = "Cannot create a local socket";
                    return false;
                }

                int result = pipe->bind(path);

#if !EKA2L1_PLATFORM(WIN32)
                // The file's permissions are the only access control a local socket has: owner only,
                // whatever the umask. Nobody can connect before listen(), so there is no window.
                if ((result == 0) && (chmod(path.c_str(), S_IRUSR | S_IWUSR) != 0)) {
                    const int chmod_error = errno;
                    pipe->close();
                    unlink(path.c_str());
                    error = "Cannot make " + path + " private: " + std::strerror(chmod_error);
                    return false;
                }
#endif

                if (result == 0) {
                    result = pipe->listen();
                }

                if (result != 0) {
                    pipe->close();
                    error = "Cannot listen on " + path + ": " + describe(result);
                    return false;
                }

#if !EKA2L1_PLATFORM(WIN32)
                socket_path_ = path;
#endif
                listening_on_ = path;
                serve(pipe);
                return true;
            }

            std::shared_ptr<uvw::tcp_handle> tcp = loop_->resource<uvw::tcp_handle>();
            if (!tcp) {
                error = "Cannot create a TCP socket";
                return false;
            }

            // Build the address here: uvw squeezes an IPv6 address into a plain sockaddr.
            sockaddr_storage address{};
            if ((uv_ip4_addr(where.address.c_str(), where.port, reinterpret_cast<sockaddr_in *>(&address)) != 0)
                && (uv_ip6_addr(where.address.c_str(), where.port, reinterpret_cast<sockaddr_in6 *>(&address)) != 0)) {
                tcp->close();
                error = "Not an IP address: " + where.address;
                return false;
            }

            int result = tcp->bind(reinterpret_cast<const sockaddr &>(address));

            if (result == 0) {
                result = tcp->listen();
            }

            if (result != 0) {
                tcp->close();
                error = "Cannot listen on " + where.to_string() + ": " + describe(result);
                return false;
            }

            listening_on_ = where.to_string();
            serve(tcp);
            return true;
        }

    public:
        explicit rpc_server_impl(dispatcher &rpc)
            : rpc_(rpc) {
        }

        ~rpc_server_impl() {
            stop();
        }

        bool start(const endpoint &where, std::string &error) {
            if (thread_.joinable()) {
                error = "The control server is already running";
                return false;
            }

            loop_ = uvw::loop::create();
            wakeup_ = loop_->resource<uvw::async_handle>();

            if (!wakeup_) {
                error = "Cannot create the control server's wakeup handle";
                return false;
            }

            wakeup_->on<uvw::async_event>([this](const uvw::async_event &, uvw::async_handle &) {
                handle_wakeup();
            });

            if (!listen(where, error)) {
                wakeup_->close();
                loop_->run();
                loop_->close();
                loop_.reset();

                return false;
            }

            {
                const std::lock_guard<std::mutex> guard(outbox_mut_);
                accepting_ = true;
                stop_requested_ = false;
            }

            thread_ = std::thread([this]() {
                common::set_thread_name("Control server");
                loop_->run();
            });

            LOG_INFO(FRONTEND_CONTROL, "Control server listening on {}", listening_on_);
            return true;
        }

        void stop() {
            {
                const std::lock_guard<std::mutex> guard(outbox_mut_);

                if (!accepting_) {
                    return;
                }

                accepting_ = false;
                stop_requested_ = true;
                wakeup_->send();
            }

            thread_.join();

            grace_timer_.reset();
            loop_->close();
            loop_.reset();
            wakeup_.reset();

#if !EKA2L1_PLATFORM(WIN32)
            if (!socket_path_.empty()) {
                unlink(socket_path_.c_str());
                socket_path_.clear();
            }
#endif
        }

        void publish(const std::string &topic, const std::string &line) {
            const std::lock_guard<std::mutex> guard(outbox_mut_);

            if (!accepting_) {
                return;
            }

            outbox_.emplace_back(topic, line);
            wakeup_->send();
        }
    };

    rpc_server::rpc_server(dispatcher &rpc)
        : impl_(std::make_unique<rpc_server_impl>(rpc)) {
    }

    rpc_server::~rpc_server() = default;

    bool rpc_server::start(const endpoint &where, std::string &error) {
        return impl_->start(where, error);
    }

    void rpc_server::stop() {
        impl_->stop();
    }

    void rpc_server::publish(const std::string &topic, const std::string &line) {
        impl_->publish(topic, line);
    }
}

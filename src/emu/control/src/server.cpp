// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <boost/asio.hpp>
#include <charconv>
#include <control/api.h>
#include <set>
#include <sstream>
#include <thread>

namespace eka2l1::control {
    namespace net = boost::asio;
    using tcp = net::ip::tcp;

    static std::string decode(const std::string &text) {
        std::string out;
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '%') {
                auto digit = [](char c) -> int {
                    if (c >= '0' && c <= '9')
                        return c - '0';
                    if (c >= 'a' && c <= 'f')
                        return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F')
                        return c - 'A' + 10;
                    return -1;
                };
                if (i + 2 >= text.size() || digit(text[i + 1]) < 0 || digit(text[i + 2]) < 0)
                    throw api_error(400, "bad_request", "invalid URL escape");
                char c = static_cast<char>(digit(text[i + 1]) * 16 + digit(text[i + 2]));
                if (!c)
                    throw api_error(400, "bad_request", "NUL in URL");
                out += c;
                i += 2;
            } else
                out += text[i];
        }
        return out;
    }

    struct server::impl : std::enable_shared_from_this<impl> {
        net::io_context io;
        tcp::acceptor acceptor{ io };
        handler handle;
        std::thread thread;
        struct connection;
        std::set<std::shared_ptr<connection>> connections;
        std::size_t body_bytes = 0;

        void accept();
        void shutdown();
    };

    struct server::impl::connection : std::enable_shared_from_this<connection> {
        tcp::socket socket;
        net::steady_timer timer;
        std::string buffer, output, response_body;
        request req;
        std::size_t body_length = 0, reserved_body_bytes = 0;
        std::weak_ptr<impl> owner;
        bool responding = false;

        connection(tcp::socket socket, std::shared_ptr<impl> owner)
            : socket(std::move(socket))
            , timer(this->socket.get_executor())
            , owner(owner) {}
        void close() {
            boost::system::error_code ec;
            timer.cancel();
            socket.shutdown(tcp::socket::shutdown_both, ec);
            socket.close(ec);
            if (auto server = owner.lock()) {
                server->body_bytes -= reserved_body_bytes;
                reserved_body_bytes = 0;
                server->connections.erase(shared_from_this());
            }
        }
        void deadline(int seconds) {
            timer.expires_after(std::chrono::seconds(seconds));
            timer.async_wait([self = shared_from_this()](boost::system::error_code ec) { if (!ec) self->close(); });
        }
        void start() {
            deadline(30);
            net::async_read_until(socket, net::dynamic_buffer(buffer, 16384), "\r\n\r\n",
                [self = shared_from_this()](boost::system::error_code ec, std::size_t count) {
                    if (ec)
                        return self->close();
                    try {
                        self->header(count);
                    } catch (const api_error &e) {
                        self->send(response::error(e.status, e.code, e.what()));
                    } catch (const std::exception &e) {
                        self->send(response::error(400, "bad_request", e.what()));
                    }
                });
        }
        void header(std::size_t count) {
            std::istringstream lines(buffer.substr(0, count - 2));
            std::string line, target, version, extra;
            std::getline(lines, line);
            std::istringstream first(line);
            if (!(first >> req.method >> target >> version) || (first >> extra)
                || (version != "HTTP/1.1" && version != "HTTP/1.0") || target.empty() || target.front() != '/')
                throw api_error(400, "bad_request", "invalid request line");
            while (std::getline(lines, line) && line != "\r") {
                auto colon = line.find(':');
                if (colon == std::string::npos || colon == 0)
                    throw api_error(400, "bad_request", "invalid header");
                auto name = lower(line.substr(0, colon));
                auto start = line.find_first_not_of(" \t", colon + 1);
                auto end = line.find_last_not_of(" \t\r");
                auto value = start == std::string::npos || end < start ? "" : line.substr(start, end - start + 1);
                if (req.headers.count(name))
                    throw api_error(400, "bad_request", "duplicate header");
                req.headers.emplace(name, value);
            }
            if (req.headers.count("origin"))
                throw api_error(403, "forbidden", "browser origins are not accepted");
            if (req.headers.count("transfer-encoding"))
                throw api_error(400, "bad_request", "Content-Length required; transfer encoding unsupported");
            if (auto it = req.headers.find("content-length"); it != req.headers.end()) {
                const auto &value = it->second;
                auto parsed = std::from_chars(value.data(), value.data() + value.size(), body_length);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
                    throw api_error(400, "bad_request", "invalid Content-Length");
                if (body_length > (1ULL << 30))
                    throw api_error(413, "too_large", "request body exceeds 1 GiB");
            }
            if (auto server = owner.lock()) {
                if (body_length > (1ULL << 30) - server->body_bytes)
                    throw api_error(503, "busy", "control upload capacity exceeded");
                server->body_bytes += body_length;
                reserved_body_bytes = body_length;
            }
            auto question = target.find('?');
            const auto path = target.substr(0, question);
            for (std::size_t start = 1; start < path.size();) {
                auto end = path.find('/', start);
                auto part = path.substr(start, end - start);
                if (!part.empty())
                    req.path.push_back(decode(part));
                if (end == std::string::npos)
                    break;
                start = end + 1;
            }
            if (question != std::string::npos) {
                for (std::size_t start = question + 1; start < target.size();) {
                    auto end = target.find('&', start);
                    auto pair = target.substr(start, end - start);
                    auto equals = pair.find('=');
                    req.query[decode(pair.substr(0, equals))] = equals == std::string::npos ? "" : decode(pair.substr(equals + 1));
                    if (end == std::string::npos)
                        break;
                    start = end + 1;
                }
            }
            buffer.erase(0, count);
            if (lower(req.headers["expect"]) == "100-continue" && buffer.size() < body_length) {
                output = "HTTP/1.1 100 Continue\r\n\r\n";
                net::async_write(socket, net::buffer(output), [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
                    if (ec)
                        self->close();
                    else
                        self->read_body();
                });
            } else
                read_body();
        }
        void read_body() {
            deadline(300);
            if (buffer.size() >= body_length)
                return dispatch();
            net::async_read(socket, net::dynamic_buffer(buffer, 1ULL << 30), net::transfer_exactly(body_length - buffer.size()),
                [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
                    if (ec)
                        self->close();
                    else
                        self->dispatch();
                });
        }
        void dispatch() {
            buffer.resize(body_length);
            req.body = std::move(buffer);
            timer.cancel();
            auto server = owner.lock();
            if (!server)
                return close();
            std::weak_ptr<connection> weak = shared_from_this();
            try {
                server->handle(std::move(req), [weak, owner = owner](response res) {
                    if (auto server = owner.lock())
                        net::post(server->io, [weak, res = std::move(res)]() mutable {
                            if (auto self = weak.lock())
                                self->send(std::move(res));
                        });
                });
            } catch (const std::exception &e) {
                send(response::error(500, "internal", e.what()));
            }
        }
        void send(response res) {
            if (responding || !socket.is_open())
                return;
            responding = true;
            output = "HTTP/1.1 " + std::to_string(res.status) + (res.status < 400 ? " OK\r\n" : " Error\r\n");
            res.headers["Content-Type"] = res.content_type;
            res.headers["Content-Length"] = std::to_string(res.body.size());
            res.headers["Connection"] = "close";
            for (const auto &[name, value] : res.headers) {
                if (name.find_first_of("\r\n") == std::string::npos && value.find_first_of("\r\n") == std::string::npos)
                    output += name + ": " + value + "\r\n";
            }
            output += "\r\n";
            response_body = std::move(res.body);
            deadline(30);
            const std::array<net::const_buffer, 2> buffers{ net::buffer(output), net::buffer(response_body) };
            net::async_write(socket, buffers, [self = shared_from_this()](boost::system::error_code, std::size_t) { self->close(); });
        }
    };

    void server::impl::accept() {
        acceptor.async_accept([self = shared_from_this()](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                auto conn = std::make_shared<connection>(std::move(socket), self);
                if (self->connections.size() >= 32)
                    conn->send(response::error(503, "busy", "too many control connections"));
                else {
                    self->connections.insert(conn);
                    conn->start();
                }
            }
            if (self->acceptor.is_open())
                self->accept();
        });
    }
    void server::impl::shutdown() {
        net::post(io, [self = shared_from_this()] {
            boost::system::error_code ec;
            self->acceptor.close(ec);
            auto active = self->connections;
            for (const auto &connection : active)
                connection->close();
        });
        if (thread.joinable())
            thread.join();
    }
    server::server() = default;
    server::~server() { stop(); }
    bool server::start(std::uint16_t port, handler handle, std::string &error) {
        stop();
        if (!port)
            return true;
        auto state = std::make_shared<impl>();
        state->handle = std::move(handle);
        boost::system::error_code ec;
        state->acceptor.open(tcp::v4(), ec);
        if (!ec)
            state->acceptor.set_option(tcp::acceptor::reuse_address(true), ec);
        if (!ec)
            state->acceptor.bind({ net::ip::address_v4::loopback(), port }, ec);
        if (!ec)
            state->acceptor.listen(16, ec);
        if (ec) {
            error = ec.message();
            return false;
        }
        impl_ = state;
        state->accept();
        state->thread = std::thread([state] { state->io.run(); });
        return true;
    }
    void server::stop() {
        if (impl_) {
            impl_->shutdown();
            impl_.reset();
        }
    }
}

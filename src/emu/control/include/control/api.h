// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace eka2l1::control {
    class json {
    public:
        using object = std::map<std::string, json>;
        using array = std::vector<json>;

    private:
        std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, object, array> value_;

    public:
        json()
            : value_(nullptr) {}
        json(std::nullptr_t)
            : value_(nullptr) {}
        json(bool value)
            : value_(value) {}
        template <typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
        json(T value)
            : value_(static_cast<std::int64_t>(value)) {}
        json(double value)
            : value_(value) {}
        json(const char *value)
            : value_(std::string(value)) {}
        json(std::string value)
            : value_(std::move(value)) {}
        json(object value)
            : value_(std::move(value)) {}
        json(array value)
            : value_(std::move(value)) {}
        json(std::initializer_list<object::value_type> value)
            : value_(object(value)) {}

        bool is_null() const { return std::holds_alternative<std::nullptr_t>(value_); }
        bool is_object() const { return std::holds_alternative<object>(value_); }
        bool is_array() const { return std::holds_alternative<array>(value_); }
        bool is_boolean() const { return std::holds_alternative<bool>(value_); }
        bool is_string() const { return std::holds_alternative<std::string>(value_); }
        bool is_number() const { return std::holds_alternative<double>(value_) || std::holds_alternative<std::int64_t>(value_); }
        bool contains(const std::string &key) const;
        const json &operator[](const std::string &key) const;
        json &operator[](const std::string &key);
        const object &members() const;
        const array &items() const;
        std::string string(const std::string &fallback = {}) const;
        double number(double fallback = 0) const;
        bool boolean(bool fallback = false) const;
        std::string dump() const;
        static json parse(const std::string &text);
    };

    struct request {
        std::string method;
        std::vector<std::string> path;
        std::map<std::string, std::string> query;
        std::map<std::string, std::string> headers;
        std::string body;
        std::function<void(double)> progress;
        std::string param(const std::string &key, const std::string &fallback = {}) const;
        json data() const;
    };

    struct response {
        int status = 200;
        std::string content_type = "application/json";
        std::string body;
        std::map<std::string, std::string> headers;
        static response from_json(const json &value, int status = 200);
        static response error(int status, const std::string &code, const std::string &message);
    };

    struct api_error : std::runtime_error {
        int status;
        std::string code;
        api_error(int status, std::string code, const std::string &message)
            : std::runtime_error(message)
            , status(status)
            , code(std::move(code)) {}
    };

    std::string required_string(const json &value, const std::string &key);
    double required_number(const json &value, const std::string &key);
    std::uint32_t parse_uid(const std::string &value);
    std::string hex(std::uint32_t value);
    std::string lower(std::string value);

    using completion = std::function<void(response)>;
    using handler = std::function<void(request, completion)>;

    class server {
        struct impl;
        std::shared_ptr<impl> impl_;

    public:
        server();
        ~server();
        server(const server &) = delete;
        server &operator=(const server &) = delete;
        // Only loopback is exposed. Port zero disables the listener.
        bool start(std::uint16_t port, handler handle, std::string &error);
        void stop();
    };
}

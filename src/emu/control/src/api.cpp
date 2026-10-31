// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cctype>
#include <algorithm>
#include <cmath>
#include <control/api.h>
#include <cstdio>
#include <limits>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace eka2l1::control {
    bool json::contains(const std::string &key) const {
        return is_object() && members().count(key);
    }
    const json &json::operator[](const std::string &key) const {
        static const json missing;
        if (!is_object())
            return missing;
        const auto found = members().find(key);
        return found == members().end() ? missing : found->second;
    }
    json &json::operator[](const std::string &key) {
        if (is_null())
            value_ = object{};
        return std::get<object>(value_)[key];
    }
    const json::object &json::members() const { return std::get<object>(value_); }
    const json::array &json::items() const { return std::get<array>(value_); }
    std::string json::string(const std::string &fallback) const {
        return is_string() ? std::get<std::string>(value_) : fallback;
    }
    double json::number(double fallback) const {
        if (std::holds_alternative<std::int64_t>(value_))
            return static_cast<double>(std::get<std::int64_t>(value_));
        return std::holds_alternative<double>(value_) ? std::get<double>(value_) : fallback;
    }
    bool json::boolean(bool fallback) const {
        return std::holds_alternative<bool>(value_) ? std::get<bool>(value_) : fallback;
    }
    std::string json::dump() const {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        std::function<void(const json &)> write = [&](const json &v) {
            if (v.is_null())
                writer.Null();
            else if (v.is_object()) {
                writer.StartObject();
                for (const auto &[key, value] : v.members()) {
                    writer.Key(key.data(), static_cast<rapidjson::SizeType>(key.size()));
                    write(value);
                }
                writer.EndObject();
            } else if (v.is_array()) {
                writer.StartArray();
                for (const auto &item : v.items())
                    write(item);
                writer.EndArray();
            } else if (v.is_string()) {
                const auto &s = std::get<std::string>(v.value_);
                writer.String(s.data(), static_cast<rapidjson::SizeType>(s.size()));
            } else if (std::holds_alternative<std::int64_t>(v.value_))
                writer.Int64(std::get<std::int64_t>(v.value_));
            else if (std::holds_alternative<double>(v.value_))
                writer.Double(std::get<double>(v.value_));
            else
                writer.Bool(std::get<bool>(v.value_));
        };
        write(*this);
        return { buffer.GetString(), buffer.GetSize() };
    }
    json json::parse(const std::string &text) {
        rapidjson::Document doc;
        doc.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag>(text.data(), text.size());
        if (doc.HasParseError())
            throw api_error(400, "bad_request", "invalid JSON");
        std::function<json(const rapidjson::Value &, int)> read = [&](const rapidjson::Value &v, int depth) -> json {
            if (depth > 64)
                throw api_error(400, "bad_request", "JSON nesting too deep");
            if (v.IsObject()) {
                object obj;
                for (auto i = v.MemberBegin(); i != v.MemberEnd(); ++i)
                    obj.emplace(std::string(i->name.GetString(), i->name.GetStringLength()), read(i->value, depth + 1));
                return obj;
            }
            if (v.IsArray()) {
                array arr;
                for (const auto &item : v.GetArray())
                    arr.push_back(read(item, depth + 1));
                return arr;
            }
            if (v.IsString())
                return std::string(v.GetString(), v.GetStringLength());
            if (v.IsBool())
                return v.GetBool();
            if (v.IsInt64())
                return v.GetInt64();
            if (v.IsNumber())
                return v.GetDouble();
            return nullptr;
        };
        return read(doc, 0);
    }
    std::string request::param(const std::string &key, const std::string &fallback) const {
        const auto i = query.find(key);
        return i == query.end() ? fallback : i->second;
    }
    json request::data() const {
        auto result = body.empty() ? json(json::object{}) : json::parse(body);
        if (!result.is_object())
            throw api_error(400, "bad_request", "body must be a JSON object");
        return result;
    }
    response response::from_json(const json &value, int status) { return { status, "application/json", value.dump() + "\n", {} }; }
    response response::error(int status, const std::string &code, const std::string &message) {
        return from_json({ { "error", { { "code", code }, { "message", message } } } }, status);
    }
    std::string required_string(const json &value, const std::string &key) {
        auto result = value[key].string();
        if (result.empty())
            throw api_error(400, "bad_request", "`" + key + "` is required");
        return result;
    }
    double required_number(const json &value, const std::string &key) {
        if (!value[key].is_number() || !std::isfinite(value[key].number()))
            throw api_error(400, "bad_request", "`" + key + "` must be a finite number");
        return value[key].number();
    }
    std::uint32_t parse_uid(const std::string &value) {
        try {
            std::size_t end = 0;
            const auto n = std::stoull(value, &end, value.size() > 2 && value.substr(0, 2) == "0x" ? 16 : 10);
            if (end == value.size() && value.front() != '-' && n <= UINT32_MAX)
                return static_cast<std::uint32_t>(n);
        } catch (...) {
        }
        throw api_error(400, "bad_request", "invalid UID: " + value);
    }
    std::string hex(std::uint32_t value) {
        char buffer[11];
        std::snprintf(buffer, sizeof(buffer), "0x%08X", value);
        return buffer;
    }
    std::string lower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }
}

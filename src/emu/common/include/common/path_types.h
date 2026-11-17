// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace eka2l1 {
    class guest_path;

    // Native filesystem path or Android content URI; identity uses the stored spelling.
    class host_path {
        std::string value_;

    public:
        using string_type = std::string;
        host_path() = default;
        explicit host_path(std::string value);

        static host_path from_utf8(std::string_view value);
        static host_path from_utf16(std::u16string_view value);
        const std::string &utf8() const noexcept { return value_; }
        std::u16string utf16() const;
        const char *c_str() const noexcept { return value_.c_str(); }
        bool empty() const noexcept { return value_.empty(); }
        void clear() noexcept { value_.clear(); }

        static constexpr std::size_t npos = std::string::npos;
        std::size_t size() const noexcept { return value_.size(); }
        std::size_t length() const noexcept { return value_.size(); }
        char operator[](std::size_t index) const { return value_[index]; }
        char front() const { return value_.front(); }
        char back() const { return value_.back(); }
        auto begin() const noexcept { return value_.cbegin(); }
        auto end() const noexcept { return value_.cend(); }
        host_path substr(std::size_t pos, std::size_t count = npos) const { return host_path(value_.substr(pos, count)); }
        template <typename Pattern> std::size_t find(const Pattern &pattern, std::size_t pos = 0) const { return value_.find(pattern, pos); }
        template <typename Pattern> std::size_t rfind(const Pattern &pattern, std::size_t pos = npos) const { return value_.rfind(pattern, pos); }
        template <typename Pattern> std::size_t find_first_of(const Pattern &pattern, std::size_t pos = 0) const { return value_.find_first_of(pattern, pos); }
        template <typename Pattern> std::size_t find_last_of(const Pattern &pattern, std::size_t pos = npos) const { return value_.find_last_of(pattern, pos); }
        template <typename Pattern> std::size_t find_first_not_of(const Pattern &pattern, std::size_t pos = 0) const { return value_.find_first_not_of(pattern, pos); }
        host_path &erase(std::size_t pos = 0, std::size_t count = npos) { value_.erase(pos, count); return *this; }
        host_path &replace(std::size_t pos, std::size_t count, const std::string &text) { auto next = value_; next.replace(pos, count, text); return *this = host_path(std::move(next)); }
        host_path &operator+=(char text) { return *this += std::string(1, text); }
        host_path &operator+=(const std::string &text) { return *this = host_path(value_ + text); }
        host_path &operator+=(const host_path &text) { return *this += text.value_; }
        friend host_path operator+(host_path left, const host_path &right) { left += right; return left; }
        friend host_path operator+(host_path left, const std::string &right) { left += right; return left; }
        friend host_path operator+(const std::string &left, const host_path &right) { return host_path(left + right.value_); }
        friend host_path operator+(host_path left, char right) { left += right; return left; }
        friend host_path operator+(char left, const host_path &right) { return host_path(std::string(1, left) + right.value_); }

        host_path parent() const;
        host_path filename() const;
        host_path extension() const;
        host_path root_name() const;
        host_path root_path() const;
        host_path root_directory() const;
        host_path relative() const;
        host_path absolute(const host_path &base) const;
        host_path with_extension(std::string_view extension) const;
        host_path join(const host_path &relative) const;
        host_path resolve_guest(const guest_path &relative) const;
        host_path resolve_guest(std::u16string_view relative) const;
        bool is_content_uri() const;
        bool has_extension(std::string_view extension) const;

        host_path join(std::string_view relative) const { return join(host_path(std::string(relative))); }
        friend host_path operator/(const host_path &base, std::string_view relative) { return base.join(relative); }
        friend bool operator==(const host_path &value, std::string_view text) { return value.value_.compare(text) == 0; }
        host_path &operator/=(const host_path &relative) { return *this = join(relative); }
        friend host_path operator/(const host_path &base, const host_path &relative) { return base.join(relative); }
        friend bool operator==(const host_path &a, const host_path &b) { return a.value_ == b.value_; }
        friend bool operator!=(const host_path &a, const host_path &b) { return !(a == b); }
        friend bool operator!=(const host_path &value, std::string_view text) { return !(value == text); }
        friend bool operator<(const host_path &a, const host_path &b) { return a.value_ < b.value_; }
    };

    // Symbian spelling is preserved; comparison, ordering and hashing share a folded key.
    class guest_path {
        std::u16string value_;
        std::u16string key_;
        static std::u16string make_lookup_key(std::u16string_view value);
        static std::u16string pattern_key(std::u16string_view value) { return make_lookup_key(value); }
        static std::u16string pattern_key(char16_t value) { return make_lookup_key(std::u16string_view(&value, 1)); }
        static const std::u16string &pattern_key(const guest_path &value) { return value.key_; }

    public:
        using string_type = std::u16string;
        guest_path() = default;
        guest_path(const guest_path &) = default;
        guest_path(guest_path &&other) noexcept : value_(std::move(other.value_)), key_(std::move(other.key_)) { other.clear(); }
        guest_path &operator=(guest_path other) noexcept {
            value_.swap(other.value_);
            key_.swap(other.key_);
            return *this;
        }
        explicit guest_path(std::u16string value);

        static guest_path from_utf8(std::string_view value);
        static guest_path from_utf16(std::u16string_view value);
        const std::u16string &utf16() const noexcept { return value_; }
        const char16_t *data() const noexcept { return value_.data(); }
        std::string utf8() const;
        bool empty() const noexcept { return value_.empty(); }
        void clear() noexcept { value_.clear(); key_.clear(); }

        static constexpr std::size_t npos = std::u16string::npos;
        std::size_t size() const noexcept { return value_.size(); }
        std::size_t length() const noexcept { return value_.size(); }
        char16_t operator[](std::size_t index) const { return value_[index]; }
        char16_t front() const { return value_.front(); }
        char16_t back() const { return value_.back(); }
        auto begin() const noexcept { return value_.cbegin(); }
        auto end() const noexcept { return value_.cend(); }
        guest_path substr(std::size_t pos, std::size_t count = npos) const { return guest_path(value_.substr(pos, count)); }
        template <typename Pattern> std::size_t find(const Pattern &pattern, std::size_t pos = 0) const { return key_.find(pattern_key(pattern), pos); }
        template <typename Pattern> std::size_t rfind(const Pattern &pattern, std::size_t pos = npos) const { return key_.rfind(pattern_key(pattern), pos); }
        template <typename Pattern> std::size_t find_first_of(const Pattern &pattern, std::size_t pos = 0) const { return key_.find_first_of(pattern_key(pattern), pos); }
        template <typename Pattern> std::size_t find_last_of(const Pattern &pattern, std::size_t pos = npos) const { return key_.find_last_of(pattern_key(pattern), pos); }
        template <typename Pattern> std::size_t find_first_not_of(const Pattern &pattern, std::size_t pos = 0) const { return key_.find_first_not_of(pattern_key(pattern), pos); }
        bool starts_with(std::u16string_view text) const { const auto key = make_lookup_key(text); return key_.compare(0, key.size(), key) == 0; }
        guest_path &erase(std::size_t pos = 0, std::size_t count = npos) { auto next = value_; next.erase(pos, count); return *this = guest_path(std::move(next)); }
        guest_path &replace(std::size_t pos, std::size_t count, const std::u16string &text) { auto next = value_; next.replace(pos, count, text); return *this = guest_path(std::move(next)); }
        void pop_back() { erase(size() - 1, 1); }
        void push_back(char16_t text) { *this += text; }
        guest_path &append(const std::u16string &text) { return *this += text; }
        guest_path &operator+=(char16_t text) { return *this += std::u16string(1, text); }
        guest_path &operator+=(const std::u16string &text) { return *this = guest_path(value_ + text); }
        guest_path &operator+=(const guest_path &text) { return *this += text.value_; }
        friend guest_path operator+(guest_path left, const guest_path &right) { left += right; return left; }
        friend guest_path operator+(guest_path left, const std::u16string &right) { left += right; return left; }
        friend guest_path operator+(const std::u16string &left, const guest_path &right) { return guest_path(left + right.value_); }
        friend guest_path operator+(guest_path left, char16_t right) { left += right; return left; }
        friend guest_path operator+(char16_t left, const guest_path &right) { return guest_path(std::u16string(1, left) + right.value_); }

        guest_path parent() const;
        guest_path filename() const;
        guest_path extension() const;
        guest_path with_extension(std::u16string_view extension) const;
        guest_path join(const guest_path &relative) const;
        guest_path relative() const;
        guest_path root() const;
        guest_path root_name() const { return root(); }
        guest_path root_path() const;
        guest_path root_directory() const;
        guest_path absolute(const guest_path &base) const;
        bool has_extension(const guest_path &extension) const;
        bool has_extension(std::u16string_view extension) const { return has_extension(guest_path::from_utf16(extension)); }
        guest_path absolute(std::u16string_view base) const { return absolute(guest_path::from_utf16(base)); }
        const std::u16string &lookup_key() const noexcept { return key_; }

        guest_path join(std::u16string_view relative) const { return join(guest_path(std::u16string(relative))); }
        friend guest_path operator/(const guest_path &base, std::u16string_view relative) { return base.join(relative); }
        friend bool operator==(const guest_path &value, std::u16string_view text) { return value.key_ == make_lookup_key(text); }
        guest_path &operator/=(const guest_path &relative) { return *this = join(relative); }
        friend guest_path operator/(const guest_path &base, const guest_path &relative) { return base.join(relative); }
        friend bool operator==(const guest_path &a, const guest_path &b) { return a.lookup_key() == b.lookup_key(); }
        friend bool operator!=(const guest_path &a, const guest_path &b) { return !(a == b); }
        friend bool operator!=(const guest_path &value, std::u16string_view text) { return !(value == text); }
        friend bool operator<(const guest_path &a, const guest_path &b) { return a.lookup_key() < b.lookup_key(); }
    };
}

namespace std {
    template <> struct hash<eka2l1::guest_path> {
        size_t operator()(const eka2l1::guest_path &path) const { return hash<u16string>{}(path.lookup_key()); }
    };
    template <> struct hash<eka2l1::host_path> {
        size_t operator()(const eka2l1::host_path &path) const noexcept { return hash<string>{}(path.utf8()); }
    };
}

#include <fmt/format.h>
namespace fmt {
    template <> struct formatter<eka2l1::host_path> : formatter<std::string_view> {
        template <typename Context> auto format(const eka2l1::host_path &value, Context &context) const {
            return formatter<std::string_view>::format(value.utf8(), context);
        }
    };
    template <> struct formatter<eka2l1::guest_path> : formatter<std::string_view> {
        template <typename Context> auto format(const eka2l1::guest_path &value, Context &context) const {
            const auto text = value.utf8();
            return formatter<std::string_view>::format(text, context);
        }
    };
}

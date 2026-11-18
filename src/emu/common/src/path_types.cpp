// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <common/path_types.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <algorithm>
#include <stdexcept>

namespace eka2l1 {
    namespace {
        template <typename T>
        void check_path(const std::basic_string<T> &value) {
            if (value.find(T(0)) != value.npos) {
                throw std::invalid_argument("a path cannot contain NUL");
            }
        }

        struct fold_mapping { std::uint32_t source, target; };
        constexpr fold_mapping simple_case_folding[] = {
#include "path_case_folding.inl"
        };

        std::uint32_t fold(std::uint32_t value) {
            if (value < 0x80) return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
            const auto end = std::end(simple_case_folding);
            const auto found = std::lower_bound(std::begin(simple_case_folding), end, value,
                [](const fold_mapping &mapping, std::uint32_t code) { return mapping.source < code; });
            return found != end && found->source == value ? found->target : value;
        }
    }

    host_path::host_path(std::string value) : value_(std::move(value)) { check_path(value_); }
    host_path host_path::from_utf8(std::string_view value) { return host_path(std::string(value)); }
    host_path host_path::from_utf16(std::u16string_view value) { return host_path(common::ucs2_to_utf8(std::u16string(value))); }
    std::u16string host_path::utf16() const { return common::utf8_to_ucs2(value_); }
    host_path host_path::parent() const { return host_path(file_directory(value_)); }
    host_path host_path::filename() const { return host_path(eka2l1::filename(value_)); }
    host_path host_path::root_name() const { return host_path(eka2l1::root_name(value_)); }
    host_path host_path::root_path() const { return host_path(eka2l1::root_path(value_)); }
    host_path host_path::root_directory() const { return host_path(eka2l1::root_dir(value_)); }
    host_path host_path::relative() const { return host_path(relative_path(value_)); }
    host_path host_path::absolute(const host_path &base) const { return host_path(absolute_path(value_, base.value_)); }
    host_path host_path::extension() const { return host_path(path_extension(filename().value_)); }
    host_path host_path::with_extension(std::string_view value) const { return host_path(value_.substr(0, value_.size() - extension().size()) + std::string(value)); }
    host_path host_path::join(const host_path &relative) const { return host_path(add_path(value_, relative.value_)); }
    host_path host_path::resolve_guest(const guest_path &relative) const {
        return common::resolve_case_insensitive_path(*this, relative);
    }
    host_path host_path::resolve_guest(std::u16string_view relative) const { return resolve_guest(guest_path::from_utf16(relative)); }
    bool host_path::is_content_uri() const { return eka2l1::is_content_uri(value_); }
    bool host_path::has_extension(std::string_view value) const {
        return extension() == host_path(std::string(value));
    }

    guest_path::guest_path(std::u16string value) : value_(std::move(value)), key_(make_lookup_key(value_)) { check_path(value_); }
    guest_path guest_path::from_utf8(std::string_view value) { return guest_path(common::utf8_to_ucs2(std::string(value))); }
    guest_path guest_path::from_utf16(std::u16string_view value) { return guest_path(std::u16string(value)); }
    std::string guest_path::utf8() const { return common::ucs2_to_utf8(value_); }
    guest_path guest_path::parent() const { return guest_path(file_directory(value_, true)); }
    guest_path guest_path::filename() const { return guest_path(eka2l1::filename(value_, true)); }
    guest_path guest_path::extension() const { return guest_path(path_extension(filename().value_)); }
    guest_path guest_path::with_extension(std::u16string_view value) const { return guest_path(value_.substr(0, value_.size() - extension().size()) + std::u16string(value)); }
    guest_path guest_path::join(const guest_path &relative) const { return guest_path(add_path(value_, relative.value_, true)); }
    guest_path guest_path::relative() const { return guest_path(relative_path(value_, true)); }
    guest_path guest_path::root() const { return guest_path(eka2l1::root_name(value_, true)); }
    guest_path guest_path::root_path() const { return guest_path(eka2l1::root_path(value_, true)); }
    guest_path guest_path::root_directory() const { return guest_path(eka2l1::root_dir(value_, true)); }
    guest_path guest_path::absolute(const guest_path &base) const { return guest_path(absolute_path(value_, base.value_, true)); }
    bool guest_path::has_extension(const guest_path &value) const { return extension() == value; }
    std::u16string guest_path::make_lookup_key(std::u16string_view value) {
        std::u16string result;
        result.reserve(value.size());
        for (std::size_t i = 0; i < value.size(); ++i) {
            std::uint32_t code = value[i];
            if (code == '/') code = '\\';
            if (code >= 0xd800 && code <= 0xdbff && i + 1 < value.size()
                && value[i + 1] >= 0xdc00 && value[i + 1] <= 0xdfff) {
                code = 0x10000 + ((code - 0xd800) << 10) + (value[++i] - 0xdc00);
            }
            code = fold(code);
            if (code > 0xffff) {
                result.push_back(char16_t(0xd800 + ((code - 0x10000) >> 10)));
                result.push_back(char16_t(0xdc00 + ((code - 0x10000) & 0x3ff)));
            } else {
                result.push_back(char16_t(code));
            }
        }
        return result;
    }
}

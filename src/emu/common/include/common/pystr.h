/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1::common {
    template <typename T>
    struct basic_pystr {
        mutable std::basic_string<T> str_;

    public:
        basic_pystr() = default;

        basic_pystr(const T *dat_)
            : str_(dat_) {
        }

        basic_pystr(const std::basic_string<T> &std_str_)
            : str_(std_str_) {
        }

        basic_pystr(const T c_, const std::size_t times_ = 1)
            : str_(times_, c_) {
        }

        std::size_t length() const {
            return str_.length();
        }

        std::basic_string<T> std_str() const {
            return str_;
        }

        const T *data() const {
            return str_.data();
        }

        bool empty() const {
            return str_.empty();
        }

        T &operator[](const std::size_t index) const {
            return str_[index];
        }

        void operator=(const basic_pystr<T> &rhs) const {
            str_ = rhs.str_;
        }

        bool operator>(const basic_pystr<T> &rhs) const {
            return str_ > rhs.str_;
        }

        bool operator>=(const basic_pystr<T> &rhs) const {
            return str_ >= rhs.str_;
        }

        bool operator<(const basic_pystr<T> &rhs) const {
            return str_ < rhs.str_;
        }

        bool operator<=(const basic_pystr<T> &rhs) const {
            return str_ <= rhs.str_;
        }

        bool operator==(const basic_pystr<T> &rhs) const {
            return str_ == rhs.str_;
        }

        bool operator!=(const basic_pystr<T> &rhs) const {
            return str_ != rhs.str_;
        }

        basic_pystr<T> operator+(const basic_pystr<T> &rhs) const {
            return rhs.str_ + str_;
        }

        void operator+=(const basic_pystr<T> &rhs) {
            str_ += rhs.str_;
        }

        const T &back() const {
            return str_.back();
        }

        template <typename I>
        basic_pystr<T> operator*(const I times) const {
            if (times == 0) {
                return basic_pystr<T>{};
            }

            if (times == 1) {
                return *this;
            }

            std::basic_string<T> news;

            for (I i = 0; i < times; i++) {
                news += str_;
            }

            return news;
        }

        template <typename I>
        void operator*=(const I times) {
            if (times == 0) {
                str_ = std::basic_string<T>{};
                return;
            }

            if (times == 1) {
                return;
            }

            const std::basic_string<T> org_ = str_;

            for (I i = 0; i < times - 1; i++) {
                str_ += org_;
            }
        }

        basic_pystr<T> lstrip() const {
            auto news = str_;

            while (news[0] == static_cast<T>(' ')) {
                news.erase(news.begin(), news.begin() + 1);
            }

            return news;
        }

        basic_pystr<T> rstrip() const {
            auto news = str_;

            while (news.back() == static_cast<T>(' ')) {
                news.pop_back();
            }

            return news;
        }

        basic_pystr<T> strip() const {
            return lstrip().rstrip();
        }

        std::vector<basic_pystr<T>> split(const basic_pystr<T> &separator = ' ') const {
            std::vector<basic_pystr<T>> strs;
            std::basic_string<T> org_ = str_;

            std::size_t pos = org_.find(separator.str_);

            while (pos != decltype(org_)::npos) {
                if (pos > 0) {
                    strs.push_back(org_.substr(0, pos));
                }

                org_.erase(org_.begin(), org_.begin() + pos + separator.length());
                pos = org_.find(separator.str_);
            }

            if (!org_.empty()) {
                strs.push_back(org_);
            }

            return strs;
        }

        template <typename F>
        std::enable_if_t<std::is_floating_point_v<F>, F> as_fp(const F def_ = 0) const {
            const std::size_t dot_pos = str_.find('.');

            if (dot_pos == decltype(str_)::npos) {
                return static_cast<F>(as_int<std::int64_t>(static_cast<std::int64_t>(def_)));
            }

            const std::int64_t part1 = substr(0, dot_pos).template as_int<std::int64_t>();
            const std::int64_t part2 = substr(dot_pos + 1).template as_int<std::int64_t>(-1);

            if (part2 == -1) {
                return def_;
            }

            F fract = static_cast<F>(part2) * (part1 < 0 ? -1 : 1);

            for (std::size_t i = 0; i < str_.length() - dot_pos - 1; i++) {
                fract /= 10;
            }

            return static_cast<F>(part1) + fract;
        }

        template <typename I>
        std::enable_if_t<std::is_integral_v<I>, I> as_int(const I def_ = 0, int base = 10) const {
            // Determine base
            std::basic_string<T> num_str_ = str_;

            if (num_str_.length() >= 2) {
                auto prefix = num_str_.substr(0, 2);
                bool prefix_found = false;

                if (prefix == "0x") {
                    base = 16;
                    prefix_found = true;
                } else if (prefix == "0b") {
                    base = 2;
                    prefix_found = true;
                } else if (prefix == "0o") {
                    base = 8;
                    prefix_found = true;
                }

                if (prefix_found) {
                    num_str_.erase(num_str_.begin(), num_str_.begin() + 2);
                }
            }

            I num_ = 0;
            I factor = 1;

            if (num_str_[0] == static_cast<T>('-')) {
                factor *= -1;
                num_str_.erase(num_str_.begin(), num_str_.begin() + 1);
            }

            const int len_ = static_cast<const int>(num_str_.length());

            for (int i = len_ - 1; i >= 0; i--) {
                int ene_ = 0;

                if (num_str_[i] >= static_cast<T>('0') && num_str_[i] <= ((base - 1) >= 9 ? 9 : base - 1) + static_cast<T>('0')) {
                    ene_ = (static_cast<I>(num_str_[i]) - static_cast<I>('0'));
                } else if (base > 10) {
                    if (num_str_[i] >= static_cast<T>('a') && num_str_[i] <= (base - 11) + static_cast<T>('a')) {
                        ene_ = (static_cast<I>(num_str_[i]) - static_cast<I>('a')) + 10;
                    } else if (num_str_[i] >= static_cast<T>('A') && num_str_[i] <= (base - 11) + static_cast<T>('A')) {
                        ene_ = (static_cast<I>(num_str_[i]) - static_cast<I>('A')) + 10;
                    } else {
                        return def_;
                    }
                } else {
                    return def_;
                }

                num_ += ene_ * factor;
                factor *= base;
            }

            return num_;
        }

        basic_pystr<T> substr(const std::size_t start, const std::size_t count = std::basic_string<T>::npos) const {
            return str_.substr(start, count);
        }
    };

    using pystr = basic_pystr<char>;
    using pystr16 = basic_pystr<char16_t>;
}

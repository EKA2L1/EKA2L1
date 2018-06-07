/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <streambuf>
#include <string>
#include <vector>

namespace eka2l1 {
    namespace common {
        // A advance morden stream
        template <typename T>
        class advstream {
            T *data;
            uint64_t count;
            uint64_t size;

        protected:
            T *_get(uint64_t hm) {
                if (eof()) {
                    return nullptr;
                }

                uint64_t get_count = std::min(hm, length());
                T &res = data[get_count];

                count += get_count;

                return &res;
            }

            T *_peek(uint64_t hm) const {
                if (eof()) {
                    return nullptr;
                }

                return &data[count];
            }

        public:
            advstream()
                : count(0)
                , data(nullptr) {}

            advstream(T *dat, const uint64_t ssize)
                : data(dat)
                , size(ssize)
                , count(0) {}

            bool eof() const {
                return count >= size;
            }

            uint64_t length() const {
                return size - count;
            }
        };

        class advstringstream : public advstream<char> {
        public:
            advstringstream(std::string &str)
                : advstream<char>(str.data(), str.size()) {}

            char &get() {
                return *_get(1);
            }

            char peek() const {
                return *_peek(1);
            }

            std::string getstr(uint64_t nm) {
                std::string res;
                auto rescpy = _get(nm);

                res.resize(nm);
                memcpy(res.data(), rescpy, std::min(nm, length()));

                return res;
            }

            std::string peekstr(uint64_t nm) {
                std::string res;
                auto rescpy = _peek(nm);

                res.resize(nm);
                memcpy(res.data(), rescpy, std::min(nm, length()));

                return res;
            }

            friend advstringstream &operator>>(advstringstream &stream, uint32_t &res) {
                std::string num;

                while (isdigit(stream.peek())) {
                    num += stream.get();
                }

                res = std::atoi(num.data());

                return stream;
            }
        };

        struct istreambuf : public std::streambuf {
        private:
            std::vector<char> buffer;

        public:
            istreambuf(char *buf, std::streamsize buf_len) {
                buffer.resize(buf_len);
                memcpy(buffer.data(), buf, buf_len);
                this->setg(buffer.data(), buffer.data(), buffer.data() + buffer.size());
            }
        };

        struct icbuf : virtual istreambuf, std::istream {
        public:
            explicit icbuf(char *buf, std::streamsize buf_len)
                : istreambuf(buf, buf_len)
                , std::istream(this) {
            }
        };
    }
}

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

#include <common/algorithm.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>

namespace eka2l1 {
    namespace common {
        enum seek_where {
            beg,
            cur,
            end
        };

        class basic_stream {
        public:
            virtual void seek(const std::uint64_t amount, seek_where wh) = 0;
            virtual bool valid() = 0;
            virtual std::uint64_t left() = 0;
            virtual uint64_t tell() const = 0;
            virtual uint64_t size() = 0;
        };

        class wo_stream: public basic_stream {
        public:
            virtual void write(const void *buf, uint32_t size) = 0;
            
            void write_string(const std::string &str) {
                const std::size_t len = str.length();

                write(&len, sizeof(len));
                write(&str[0], static_cast<std::uint32_t>(len));
            }
        };

        class ro_stream : public basic_stream {
        public:
            virtual std::uint64_t read(void *buf, const std::uint64_t read_size) = 0;
            
            std::string read_string() {
                std::string str;
                std::size_t len;

                read(&len, sizeof(len));
                str.resize(len);

                read(&str[0], static_cast<std::uint32_t>(len));

                return str;
            }
            
            std::uint64_t read(const std::uint64_t pos, void *buf, const std::uint64_t size) {
                seek(pos, seek_where::beg);
                return read(buf, size);
            }
        };

        /*! \brief Another buffer stream, base on LLVM's Buffer 
        */
        class buffer_stream_base {
        protected:
            uint8_t *beg;
            uint8_t *end;

            std::uint64_t crr_pos;

        public:
            buffer_stream_base()
                : beg(nullptr)
                , end(nullptr)
                , crr_pos(0) {}

            buffer_stream_base(uint8_t *data, uint64_t size)
                : beg(data)
                , end(beg + size)
                , crr_pos(0) {}

            std::uint8_t *get_current() {
                return beg + crr_pos;
            }
        };

        /*! A read only buffer stream */
        class ro_buf_stream : public buffer_stream_base, public ro_stream {
        public:
            ro_buf_stream(uint8_t *beg, uint64_t size)
                : buffer_stream_base(beg, size) {}

            bool valid() override {
                return beg + crr_pos < end;
            }

            std::uint64_t size() override {
                return end - beg;
            }
            
            uint64_t tell() const override {
                return crr_pos;
            }
            
            uint64_t left() override {
                return end - beg - crr_pos;
            }

            void seek(const std::uint64_t amount, seek_where wh) override {
                if (wh == seek_where::beg) {
                    crr_pos = amount;
                    return;
                }

                if (wh == seek_where::cur) {
                    crr_pos += amount;
                    return;
                }

                crr_pos = (end - beg) + amount;
            }

            std::uint64_t read(void *buf, const std::uint64_t read_size) override {
                std::uint64_t actual_read_size = common::min(read_size,
                    static_cast<std::uint64_t>(end - beg - crr_pos));

                memcpy(buf, beg + crr_pos, actual_read_size);
                crr_pos += actual_read_size;

                return actual_read_size;
            }
        };

        class wo_buf_stream : public buffer_stream_base, public wo_stream {
        public:
            wo_buf_stream(uint8_t *beg, std::size_t max_size = 0xFFFFFFFF)
                : buffer_stream_base(beg, max_size) {}

            bool valid() override {
                return beg + crr_pos < end;
            }

            std::uint64_t size() override {
                return end - beg;
            }
            
            uint64_t tell() const override {
                return crr_pos;
            }
            
            uint64_t left() override {
                return end - beg - crr_pos;
            }
            
            void seek(const std::uint64_t amount, seek_where wh) override {
                if (wh == seek_where::beg) {
                    crr_pos = amount;
                    return;
                }

                if (wh == seek_where::cur) {
                    crr_pos += amount;
                    return;
                }

                crr_pos = (end - beg) + amount;
            }

            void write(const void *buf, uint32_t size) override {
                if (beg) {
                    memcpy(beg + crr_pos, buf, size);
                }

                if (beg + crr_pos > end) {
                    end = beg + crr_pos;
                }

                crr_pos += size;
            }
        };
        
        class wo_std_file_stream: public common::wo_stream {
            mutable std::ofstream fo_;

        public:
            explicit wo_std_file_stream(const std::string &path)
                : fo_(path) {
            }
            
            void write(const void *buf, uint32_t size) override {
                fo_.write(reinterpret_cast<const char*>(buf), size);
            }
            
            void seek(const std::uint64_t amount, common::seek_where wh) override{
                fo_.seekp(amount, common::beg ? std::ios::beg : (common::cur ? std::ios::cur : std::ios::end));
            }

            bool valid() override {
                return !fo_.fail();
            }

            std::uint64_t left() override {
                return 0xFFFFFFFF;
            }

            std::uint64_t tell() const override {
                return fo_.tellp();
            }

            std::uint64_t size() override {
                const std::uint64_t cur_pos = tell();
                seek(0, common::end);
                const std::uint64_t s = tell();

                seek(cur_pos, common::beg);
                return s;
            }
        };
    }
}

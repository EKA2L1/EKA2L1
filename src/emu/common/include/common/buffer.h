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

#include <cstdint>
#include <cstring>

namespace eka2l1 {
    namespace common {
		/*! \brief Another buffer stream, base on LLVM's Buffer */
		*/
        class buffer_stream_base {
        protected:
            uint8_t *beg;
            uint8_t *end;

            uint64_t crr_pos;

        public:
            buffer_stream_base()
                : beg(nullptr)
                , end(nullptr)
                , crr_pos(0) {}

            buffer_stream_base(uint8_t *data, uint64_t size)
                : beg(data)
                , end(beg + size) {}
        };

        enum seek_where {
            beg,
            cur,
            end
        };

		/*! A read only buffer stream */
        class ro_buf_stream : public buffer_stream_base {
        public:
            ro_buf_stream(uint8_t *beg, uint64_t size)
                : buffer_stream_base(beg, size) {}

            void seek(uint32_t amount, seek_where wh) {
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

            void read(void *buf, uint32_t size) {
                memcpy(buf, beg + crr_pos, size);
                crr_pos += size;
            }

            uint64_t tell() const {
                return crr_pos;
            }
        };
    }
}

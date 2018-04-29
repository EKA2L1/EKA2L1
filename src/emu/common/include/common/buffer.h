#pragma once

#include <cstdint>
#include <cstring>

namespace eka2l1 {
	namespace common {
		// Hate MSVC istringstream since 1998

		// This header contains a lightweight class for streaming buffer,
		// since std contains nothing helpful for this need

		class buffer_stream_base {
		protected:
			uint8_t* beg;
			uint8_t* end;

			uint64_t crr_pos;

		public:
			buffer_stream_base()
				: beg(nullptr), end(nullptr), crr_pos(0) {}

			buffer_stream_base(uint8_t* data, uint64_t size)
				: beg(data), end(beg + size) {}
		};

		enum seek_where {
			beg,
			cur,
			end
		};

		class ro_buf_stream: public buffer_stream_base {
		public:
			ro_buf_stream(uint8_t* beg, uint64_t size)
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

			void read(void* buf, uint32_t size) {
				memcpy(buf, beg + crr_pos, size);
				crr_pos += size;
			}

			uint64_t tell() const {
				return crr_pos;
			}
		};
	}
}
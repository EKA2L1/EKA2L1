#pragma once

#include <common/vecx.h>
#include <cstdint>

namespace eka2l1 {
	namespace driver {
		class emu_window {
			// EPOC9 and down
			virtual void init() = 0;

			virtual void set_framebuffer_pixel(const point& pix, uint8_t color) = 0;
			virtual void clear(const uint8_t color) = 0;
		
			virtual void render() = 0;
			virtual void swap_buffer() = 0;

			virtual void shutdown() = 0;
		};
	}
}
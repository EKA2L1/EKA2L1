#pragma once

#include <common/vecx.h>
#include <memory>
#include <string>
#include <vector>

namespace eka2l1 {
	class emu_window;
	using emu_window_ptr = std::shared_ptr<emu_window>;

	// Symbian 9.x and lower driver
	// For Symbian^3 (EPOC10), it uses ScreenPlay, that access directly to GPU
	// This is using the CPU

	// TODO: Support GL draw calls
	using pixel_line =
		std::vector<char> pixels;

	namespace driver {
		class screen_driver {
			emu_window_ptr win;
		public:
			void init(emu_window_ptr emu) = 0;

			void init(object_size& screen_size, object_size& font_size) = 0;
			void blit(const std::string& text, int len, const point& where) = 0;
			bool scroll_up(rect& trect) = 0;

			void clear(rect& trect) = 0;

			void set_pixel(const point& pos, uint8_t color) = 0;
			int get_pixel(const point& pos) = 0;

			void set_word(const point& pos, int word) = 0;
			int get_word(const point& pos) = 0;

			void set_line(const point& pos, const pixel_line& pl) = 0;
			void get_line(const point& pos, pixel_line& pl) = 0;
		};

		enum class driver_type {
			opengl,
			vulkan,
			directx
		};

		using screen_driver_ptr = std::unique_ptr<screen_driver>;

		screen_driver_ptr new_screen_driver(driver_type dr_type);
	}
}
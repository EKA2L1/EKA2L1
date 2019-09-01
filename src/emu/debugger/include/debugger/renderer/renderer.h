#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <drivers/graphics/common.h>
#include <drivers/graphics/imgui_renderer.h>

#include <debugger/debugger.h>

namespace eka2l1 {
    namespace drivers {
        class graphics_driver;
    }

    class debugger_renderer {
    protected:
        debugger_base *debugger_;
        drivers::handle background_tex_;
        std::unique_ptr<drivers::imgui_renderer> irenderer_;

        std::string background_change_path_;

    protected:
        bool change_background_internal(drivers::graphics_driver *driver, drivers::graphics_command_list_builder *builder, const char *path);

    public:
        void init(drivers::graphics_driver *driver, drivers::graphics_command_list_builder *builder,
            debugger_base *debugger);

        void draw(drivers::graphics_driver *driver, drivers::graphics_command_list_builder *builder,
            const std::uint32_t width, const std::uint32_t height, const std::uint32_t fb_width,
            const std::uint32_t fb_height);

        void deinit(drivers::graphics_command_list_builder *builder);

        bool change_background(const char *path);
    };
}

#pragma once

#include <cstdint>
#include <memory>

#include <drivers/graphics/common.h>
#include <drivers/graphics/imgui_renderer.h>

#include <debugger/debugger.h>

namespace eka2l1 {
    using debugger_ptr = std::shared_ptr<debugger_base>;

    namespace drivers {
        class graphics_driver;
        class input_driver;
        class imgui_renderer_base;

        using graphics_driver_ptr = std::shared_ptr<graphics_driver>;
        using input_driver_ptr = std::shared_ptr<drivers::input_driver>;
        using imgui_renderer_instance = std::unique_ptr<imgui_renderer_base>;
    }

    class debugger_renderer {
    protected:
        debugger_ptr debugger;

        drivers::graphics_driver_ptr gr_driver_;
        drivers::input_driver_ptr inp_driver_;

        drivers::imgui_renderer_instance irenderer;

    public:
        void init(drivers::graphic_api gr_api, drivers::graphics_driver_ptr graphic_driver, drivers::input_driver_ptr input_driver,
            debugger_ptr debugger);

        void draw(std::uint32_t width, std::uint32_t height, std::uint32_t fb_width, std::uint32_t fb_height);
        void deinit();
    };

    using debugger_renderer_ptr = std::shared_ptr<debugger_renderer>;
}

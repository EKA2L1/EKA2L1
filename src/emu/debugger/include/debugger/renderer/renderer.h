#pragma once

#include <cstdint>
#include <memory>

namespace eka2l1 {
    class debugger;
    using debugger_ptr = std::shared_ptr<debugger>;

    class debugger_renderer {
    protected:
        debugger_ptr debugger;

    public:
        virtual void init(debugger_ptr &debugger);
        virtual void draw(std::uint32_t width, std::uint32_t height
            , std::uint32_t fb_width, std::uint32_t fb_height) = 0;
        virtual void deinit() = 0;
    };

    using debugger_renderer_ptr = std::shared_ptr<debugger_renderer>;

    enum class debugger_renderer_type {
        opengl
    };

    debugger_renderer_ptr new_debugger_renderer(const debugger_renderer_type rtype);
}
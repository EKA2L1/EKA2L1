#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

namespace eka2l1 {
    struct debug_breakpoint {
        std::uint32_t addr;
        bool is_hit;
    };

    namespace manager {
        struct config_state;
    }

    class debugger_renderer;

    class debugger_base {
    protected:
        friend class debugger_renderer;

        debugger_renderer *renderer;
        std::mutex lock;

    public:
        explicit debugger_base();

        std::function<void(bool)> on_pause_toogle;

        virtual manager::config_state *get_config() = 0;
        virtual bool should_emulate_stop() = 0;
        virtual void wait_for_debugger() = 0;
        virtual void notify_clients() = 0;

        virtual void show_debugger(std::uint32_t width, std::uint32_t height, std::uint32_t fb_width,
            std::uint32_t fb_height)
            = 0;
    };
}

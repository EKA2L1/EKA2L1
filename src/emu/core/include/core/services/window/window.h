#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include <services/server.h>
#include <vector>

#include <ptr.h>

#include <common/queue.h>
#include <drivers/screen_driver.h>

enum {
    cmd_slot = 0,
    reply_slot = 1
};

namespace eka2l1 {
    struct ws_cmd_header {
        uint16_t op;
        uint16_t cmd_len;
        uint32_t obj_handle;
    };

    struct ws_cmd {
        ws_cmd_header header;
        void *data_ptr;
    };

    struct ws_cmd_screen_device_header {
        int num_screen;
        uint32_t screen_dvc_ptr;
    };

    struct ws_cmd_window_group_header {
        uint32_t client_handle;
        bool focus;
        uint32_t parent_id;
        uint32_t screen_device_handle;
    };
}

/*! \brief Namespace for HLE EPOC implementation.
 *
 */
namespace epoc {
    struct window;
    using window_ptr = std::shared_ptr<epoc::window>;

    enum class window_type {
        normal,
        group,
        top_client,
        client
    };

    /*! \brief Base class for all window. */
    struct window {
        eka2l1::cp_queue<window_ptr> childs;
        window_ptr parent;
        uint16_t priority;
        uint64_t id;

        window_type type;

        bool operator==(const window &rhs) {
            return priority == rhs.priority;
        }

        bool operator!=(const window &rhs) {
            return priority != rhs.priority;
        }

        bool operator>(const window &rhs) {
            return priority > rhs.priority;
        }

        bool operator<(const window &rhs) {
            return priority < rhs.priority;
        }

        bool operator>=(const window &rhs) {
            return priority >= rhs.priority;
        }

        bool operator<=(const window &rhs) {
            return priority <= rhs.priority;
        }

        window(uint64_t id)
            : type(window_type::normal), id(id) {}

        window(uint64_t id, window_type type)
            : type(type), id(id) {}
    };

    struct screen_device {
        uint64_t id;
        eka2l1::driver::screen_driver_ptr driver;

        screen_device(uint64_t id, eka2l1::driver::screen_driver_ptr driver);

        void execute_command(eka2l1::service::ipc_context ctx, eka2l1::ws_cmd cmd);
    };

    using screen_device_ptr = std::shared_ptr<epoc::screen_device>;

    struct window_group : public epoc::window {
        screen_device_ptr dvc;

        window_group(uint64_t id, screen_device_ptr &dvc)
            : window(id, window_type::group), dvc(dvc) {}

        eka2l1::vec2 get_screen_size() const {
            return dvc->driver->get_window_size();
        }

        void adjust_screen_size(const eka2l1::object_size scr_size) const {
        }
    };

    using window_group_ptr = std::shared_ptr<epoc::window_group>;
}

namespace eka2l1 {
    class window_server : public service::server {
        std::atomic<uint64_t> id_counter;
        std::unordered_map<uint64_t, epoc::screen_device_ptr> devices;

        epoc::screen_device_ptr primary_device;

        epoc::window_ptr root;

        uint64_t new_id() {
            id_counter++;
            return id_counter.load();
        }

        epoc::window_ptr find_window_obj(epoc::window_ptr &root, uint64_t id);

        void execute_command(service::ipc_context ctx, ws_cmd cmd);
        void execute_commands(service::ipc_context ctx, std::vector<ws_cmd> cmds);

        void init(service::ipc_context ctx);
        void parse_command_buffer(service::ipc_context ctx);

        void create_screen_device(service::ipc_context ctx, ws_cmd cmd);
        void create_window_group(service::ipc_context ctx, ws_cmd cmd);
        void restore_hotkey(service::ipc_context ctx, ws_cmd cmd);

        void init_device(epoc::window_ptr &win);

    public:
        window_server(system *sys);
    };
}
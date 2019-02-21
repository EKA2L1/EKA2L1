#include <epoc/services/window/classes/plugins/sprite.h>

namespace eka2l1::epoc {
    void sprite::execute_command(service::ipc_context &context, ws_cmd cmd) {
    }

    sprite::sprite(window_server_client_ptr client, window_ptr attached_window,
        eka2l1::vec2 pos)
        : window_client_obj(client)
        , position(pos)
        , attached_window(attached_window) {
    }
}
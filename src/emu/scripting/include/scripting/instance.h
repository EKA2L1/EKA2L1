#pragma once

namespace eka2l1 {
    class system;
}

namespace eka2l1::scripting {
    void set_current_instance(eka2l1::system *sys);
    eka2l1::system *get_current_instance();
}
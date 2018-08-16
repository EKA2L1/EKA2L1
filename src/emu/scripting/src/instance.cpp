#include <mutex>
#include <scripting/instance.h>

namespace eka2l1::scripting {
    eka2l1::system *instance;
    std::mutex scripting_mut;

    void set_current_instance(eka2l1::system *sys) {
        std::lock_guard<std::mutex> guard(scripting_mut);
        instance = sys;
    }

    eka2l1::system *get_current_instance() {
        return instance;
    }
}
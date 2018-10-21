#include <debugger/renderer/renderer.h>
#include <debugger/renderer/opengl/gl_renderer.h>

namespace eka2l1 {
    void debugger_renderer::init(debugger_ptr &dbg) {
        debugger = dbg;
    }

    debugger_renderer_ptr new_debugger_renderer(const debugger_renderer_type rtype) {
        switch (rtype) {
        case debugger_renderer_type::opengl: 
            return std::make_shared<debugger_gl_renderer>();

        default:
            break;
        }

        return nullptr;
    }
}
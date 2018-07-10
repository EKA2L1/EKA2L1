#include <drivers/backend/ogl/instance_ogl.h>
#include <glad/glad.h>

namespace eka2l1 {
    namespace driver {
        void instance_ogl::init(emu_window_ptr &win) {
            win_ptr = win;
            win_ptr->make_current();

            int res = gladLoadGL();
        }
    }
}
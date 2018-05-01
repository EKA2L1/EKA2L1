#include "window.h"

namespace eka2l1 {
    namespace imgui {
        GLFWwindow *open_window(const std::string &title, const uint32_t width,
            const uint32_t height) {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

            GLFWwindow *win = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
            glfwMakeContextCurrent(win);

            gladLoadGL();

            return win;
        }

        void close_window(GLFWwindow *window) {
            glfwSetWindowShouldClose(window, true);
        }

        void destroy_window(GLFWwindow *window) {
            glfwDestroyWindow(window);
        }
    }
}

#include <string>
#include "internal.h"

namespace eka2l1 {
    namespace imgui {
         GLFWwindow* open_window(const std::string& title, const uint32_t width,
                                 const uint32_t height);

         void close_window(GLFWwindow* window);
         void destroy_window(GLFWwindow* window);
    }
}

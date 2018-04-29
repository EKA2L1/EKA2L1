add_library(imguirdr-gl imguirdr.cpp)
target_link_libraries(imguirdr-gl PUBLIC glfw imgui glad common)

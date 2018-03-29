set (CMAKE_CXX_STANDARD 14)

add_subdirectory(io)
add_subdirectory(loader)
add_subdirectory(common)
add_subdirectory(imgui)

add_executable(EKA2L1 EKA2L1.cpp)

target_link_libraries(EKA2L1 io loader)

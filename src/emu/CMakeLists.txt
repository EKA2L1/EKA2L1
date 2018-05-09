project (EKA2L1)

add_subdirectory(common)
add_subdirectory(imgui)
add_subdirectory(core)

add_executable(EKA2L1_CONS console/main.cpp)
target_link_libraries(EKA2L1_CONS PRIVATE common core)
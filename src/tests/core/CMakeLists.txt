set(CMAKE_CXX_STANDARD 14)

add_library(coretest
	core_timing.cpp)

target_link_libraries(coretest
        PRIVATE googletest coretiming)

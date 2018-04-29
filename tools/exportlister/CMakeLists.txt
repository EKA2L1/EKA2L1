add_executable(exportlister src/main.cpp)
target_link_libraries(exportlister PRIVATE coreloader common)

if (NOT MSVC)
 target_link_libraries(exportlister PRIVATE stdc++fs)
endif()

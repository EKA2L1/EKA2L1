add_library(disasm
        include/disasm/disasm.h
        src/disasm.cpp)

target_include_directories(disasm PUBLIC include)
target_include_directories(disasm PUBLIC ${capstone_INCLUDE_DIRS})

target_link_libraries(disasm PRIVATE capstone-static common)

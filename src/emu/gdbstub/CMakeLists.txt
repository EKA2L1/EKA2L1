add_library(gdbstub
    include/gdbstub/gdbstub.h
    src/gdbstub.cpp)

target_include_directories(gdbstub PUBLIC include)
target_link_libraries(gdbstub PRIVATE
    arm
    epockern)
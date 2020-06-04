add_library(gdbstub
        include/gdbstub/gdbstub.h
        src/gdbstub.cpp)

set(LIBRARIES
        cpu
        epoc
        epockern)

if (WIN32)
    set(LIBRARIES ${LIBRARIES} wsock32)
endif ()

target_include_directories(gdbstub PUBLIC include)
target_link_libraries(gdbstub PRIVATE ${LIBRARIES})

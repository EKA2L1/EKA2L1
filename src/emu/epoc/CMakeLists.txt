add_library(epoc
        include/epoc/epoc.h
        include/epoc/hal.h
        src/epoc.cpp
        src/hal.cpp)

target_include_directories(epoc PUBLIC include)
target_link_libraries(epoc PRIVATE
        arm
        common
        disasm
        drivers
        epocdispatch
        epockern
        epocio
        epocloader
        epocservs
        hle
        gdbstub
        manager)

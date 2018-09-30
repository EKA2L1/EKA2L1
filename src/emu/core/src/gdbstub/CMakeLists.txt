add_library(gdbstub
    ${CORE_INCLUDE_DIR}/gdbstub/gdbstub.h
    gdbstub.cpp)

target_link_libraries(gdbstub PRIVATE corekernel)
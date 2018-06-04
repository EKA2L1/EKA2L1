add_library(epoc9
    ${CORE_INCLUDE_DIR}/hle/epoc9/allocator.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/base.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/cons.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/des.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/err.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/lock.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/mem.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/sts.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/thread.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/types.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/user.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/register.h
    allocator.cpp
    base.cpp
    char.cpp
    des.cpp
    mem.cpp
    thread.cpp
    user.cpp
    register.cpp)

target_include_directories(epoc9 PUBLIC ${CORE_INCLUDE_DIR}/hle)
target_link_libraries(epoc9 PUBLIC common)
target_link_libraries(epoc9 PRIVATE corekernel hle)


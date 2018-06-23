add_library(epoc9
    ${CORE_INCLUDE_DIR}/hle/epoc9/allocator.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/base.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/cons.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/des.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/dll.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/e32loader.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/err.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/lock.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/hal.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/mem.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/sts.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/thread.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/types.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/user.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/trap.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/register.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/svc.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/svr.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/pr.h
    ${CORE_INCLUDE_DIR}/hle/epoc9/prop.h
    allocator.cpp
    base.cpp
    char.cpp
    cons.cpp
    des.cpp
    dll.cpp
    lock.cpp
    e32loader.cpp
    hal.cpp
    mem.cpp
    thread.cpp
    user.cpp
    trap.cpp
    register.cpp
    svc.cpp
    svr.cpp
    pr.cpp
    prop.cpp)

target_include_directories(epoc9 PUBLIC ${CORE_INCLUDE_DIR}/hle)
target_link_libraries(epoc9 PUBLIC common)
target_link_libraries(epoc9 PRIVATE corekernel hle)


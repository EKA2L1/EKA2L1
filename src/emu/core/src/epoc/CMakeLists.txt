add_library(epoc
    ${CORE_INCLUDE_DIR}/epoc/des.h
    ${CORE_INCLUDE_DIR}/epoc/tl.h
    ${CORE_INCLUDE_DIR}/epoc/svc.h
    ${CORE_INCLUDE_DIR}/epoc/handle.h
    ${CORE_INCLUDE_DIR}/epoc/reg.h
    ${CORE_INCLUDE_DIR}/epoc/hal.h
    ${CORE_INCLUDE_DIR}/epoc/dll.h
    tl.cpp
    des.cpp
    svc.cpp
    dll.cpp
    handle.cpp
    reg.cpp
    hal.cpp)

target_include_directories(epoc PUBLIC ${CORE_OUTER_INCLUDE_DIR})

target_link_libraries(epoc PUBLIC common)
target_link_libraries(epoc PRIVATE corekernel services hle)
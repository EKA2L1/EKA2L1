add_library(services 
    ${CORE_INCLUDE_DIR}/services/context.h
    ${CORE_INCLUDE_DIR}/services/init.h
    ${CORE_INCLUDE_DIR}/services/fast_heap.h
    ${CORE_INCLUDE_DIR}/services/session.h
    ${CORE_INCLUDE_DIR}/services/server.h
    ${CORE_INCLUDE_DIR}/services/property.h
    ${CORE_INCLUDE_DIR}/services/applist/op.h
    ${CORE_INCLUDE_DIR}/services/applist/applist.h
    ${CORE_INCLUDE_DIR}/services/featmgr/featmgr.h
    #${CORE_INCLUDE_DIR}/services/fontbitmap/fontbitmap.h
    ${CORE_INCLUDE_DIR}/services/fs/fs.h
    ${CORE_INCLUDE_DIR}/services/window/window.h
    ${CORE_INCLUDE_DIR}/services/loader/loader.h
    ${CORE_INCLUDE_DIR}/services/loader/op.h
    ${CORE_INCLUDE_DIR}/ipc.h
    context.cpp
    init.cpp
    fast_heap.cpp
    property.cpp
    server.cpp
    session.cpp
    applist/applist.cpp
    featmgr/featmgr.cpp
    fs/fs.cpp
    loader/loader.cpp
    #fontbitmap/fontbitmap.cpp
    window/window.cpp
    ../ipc.cpp)

target_include_directories(services PUBLIC ${CORE_OUTER_INCLUDE_DIR} ${EPOC32_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIR}) 
target_link_libraries(services PRIVATE epoc common core freetype)
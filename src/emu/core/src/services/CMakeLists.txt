add_library(services 
    ${CORE_INCLUDE_DIR}/services/context.h
    ${CORE_INCLUDE_DIR}/services/init.h
    ${CORE_INCLUDE_DIR}/services/session.h
    ${CORE_INCLUDE_DIR}/services/server.h
    ${CORE_INCLUDE_DIR}/services/property.h
    ${CORE_INCLUDE_DIR}/services/applist/op.h
    ${CORE_INCLUDE_DIR}/services/applist/applist.h
    ${CORE_INCLUDE_DIR}/services/domain/database.h
    ${CORE_INCLUDE_DIR}/services/domain/defs.h
    ${CORE_INCLUDE_DIR}/services/domain/domain.h
    ${CORE_INCLUDE_DIR}/services/drm/helper.h
    ${CORE_INCLUDE_DIR}/services/drm/rights.h
    ${CORE_INCLUDE_DIR}/services/featmgr/featmgr.h
    ${CORE_INCLUDE_DIR}/services/fs/fs.h
    ${CORE_INCLUDE_DIR}/services/loader/loader.h
    ${CORE_INCLUDE_DIR}/services/install/install.h
    ${CORE_INCLUDE_DIR}/services/loader/op.h
    ${CORE_INCLUDE_DIR}/services/posix/posix.h
    ${CORE_INCLUDE_DIR}/services/posix/op.h
    ${CORE_INCLUDE_DIR}/services/ui/oom_app.h
    ${CORE_INCLUDE_DIR}/services/window/window.h
    ${CORE_INCLUDE_DIR}/ipc.h
    context.cpp
    init.cpp
    property.cpp
    server.cpp
    session.cpp
    applist/applist.cpp
    domain/domain.cpp
    drm/helper.cpp
    drm/rights.cpp
    featmgr/featmgr.cpp
    fs/fs.cpp
    install/install.cpp
    loader/loader.cpp
    posix/posix.cpp
    ui/oom_app.cpp
    window/window.cpp
    ../ipc.cpp)

target_include_directories(services PUBLIC
    ${CORE_OUTER_INCLUDE_DIR}
    ${EPOC32_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIR}) 

target_link_libraries(services PRIVATE epoc common core freetype)

if (NOT MSVC)
    target_link_libraries(services PRIVATE stdc++fs)
endif()
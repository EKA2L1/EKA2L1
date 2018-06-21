add_library(services 
    ${CORE_INCLUDE_DIR}/services/context.h
    ${CORE_INCLUDE_DIR}/services/session.h
    ${CORE_INCLUDE_DIR}/services/server.h
    ${CORE_INCLUDE_DIR}/ipc.h
    context.cpp
    server.cpp
    session.cpp
    ../ipc.cpp)

target_include_directories(services PUBLIC ${CORE_INCLUDE_DIR})
target_link_libraries(services PRIVATE epoc9 common core)
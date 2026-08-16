# discord-rpc ships a CMake project that downloads RapidJSON at configure time.
# Compile the sources directly against the vendored copy instead, the way
# PPSSPP does, so a build needs nothing from the network.

set(DISCORD_RPC_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../discord-rpc/src")

set(DISCORD_RPC_SOURCES
        ${DISCORD_RPC_SRC_DIR}/backoff.h
        ${DISCORD_RPC_SRC_DIR}/connection.h
        ${DISCORD_RPC_SRC_DIR}/msg_queue.h
        ${DISCORD_RPC_SRC_DIR}/rpc_connection.h
        ${DISCORD_RPC_SRC_DIR}/serialization.h
        ${DISCORD_RPC_SRC_DIR}/discord_rpc.cpp
        ${DISCORD_RPC_SRC_DIR}/rpc_connection.cpp
        ${DISCORD_RPC_SRC_DIR}/serialization.cpp)

if (WIN32)
    list(APPEND DISCORD_RPC_SOURCES
            ${DISCORD_RPC_SRC_DIR}/connection_win.cpp
            ${DISCORD_RPC_SRC_DIR}/discord_register_win.cpp)
elseif (APPLE)
    # discord_register_osx is Objective-C, which the top-level project does not
    # enable on its own.
    enable_language(OBJC)
    list(APPEND DISCORD_RPC_SOURCES
            ${DISCORD_RPC_SRC_DIR}/connection_unix.cpp
            ${DISCORD_RPC_SRC_DIR}/discord_register_osx.m)
else()
    list(APPEND DISCORD_RPC_SOURCES
            ${DISCORD_RPC_SRC_DIR}/connection_unix.cpp
            ${DISCORD_RPC_SRC_DIR}/discord_register_linux.cpp)
endif()

add_library(discord-rpc STATIC ${DISCORD_RPC_SOURCES})

target_include_directories(discord-rpc PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../discord-rpc/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../discord-rpc/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/../rapidjson/include")

if (UNIX AND NOT ANDROID AND NOT APPLE)
    target_link_libraries(discord-rpc PUBLIC pthread)
endif()

if (APPLE)
    target_link_libraries(discord-rpc PRIVATE "-framework AppKit")
endif()

set_property(TARGET discord-rpc PROPERTY FOLDER externals)

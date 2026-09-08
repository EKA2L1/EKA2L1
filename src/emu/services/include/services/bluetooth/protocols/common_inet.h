#pragma once

#include <cstdint>
#include <cstring>
#include <memory>

#include <uv.h>

namespace eka2l1::epoc::bt {
    // libuv leaves UDP reads armed after errors. Stop invalid sockets to avoid a
    // busy loop, but keep receiving after transient network or per-send failures.
    inline bool is_socket_dead_error(const int libuv_error) {
        switch (libuv_error) {
        case UV_EBADF:
        case UV_ENOTCONN:
        case UV_ENOTSOCK:
        case UV_EPIPE:
            return true;

        default:
            return false;
        }
    }

    // uvw raw-pointer sends borrow the bytes until asynchronous completion.
    inline std::unique_ptr<char[]> copy_control_packet(const char *data, const std::size_t size) {
        auto packet = std::make_unique<char[]>(size);
        std::memcpy(packet.get(), data, size);
        return packet;
    }

    enum query_opcode : std::uint8_t {
        QUERY_OPCODE_GET_NAME,
        QUERY_OPCODE_GET_VIRTUAL_BLUETOOTH_ADDRESS,
        QUERY_OPCODE_IS_REAL_PORT_MAPPED_TO_VIRTUAL_PORT,
        QUERY_OPCODE_GET_REAL_PORT_FROM_VIRTUAL_PORT,
        QUERY_OPCODE_GET_PLAYERS,
        QUERY_OPCODE_NOTIFY_PLAYER_EXISTENCE,
        QUERY_OPCODE_ALLOW_MATCHING,
        QUERY_OPCODE_MATCHING_CONFIRMED,
        QUERY_OPCODE_MATCHING_DENIED,

        QUERY_OPCODE_SERVER_LOGIN,
        QUERY_OPCODE_SERVER_LOGOUT,
        QUERY_OPCODE_SERVER_PORT_EXTENSION,
        QUERY_OPCODE_RESULT_START = 100
    };
}

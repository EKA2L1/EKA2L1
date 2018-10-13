// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>

#ifdef _WIN32
#include <winsock2.h>
// winsock2.h needs to be included first to prevent winsock.h being included by other includes
#include <io.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#define SHUT_RDWR 2
#else
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace eka2l1 {
    class system;

    namespace kernel {
        class thread;
    }

    using thread_ptr = std::shared_ptr<kernel::thread>;

    constexpr int GDB_BUFFER_SIZE = 10000;

    constexpr char GDB_STUB_START = '$';
    constexpr char GDB_STUB_END = '#';
    constexpr char GDB_STUB_ACK = '+';
    constexpr char GDB_STUB_NACK = '-';

#ifndef SIGTRAP
    constexpr std::uint32_t SIGTRAP = 5;
#endif

#ifndef SIGTERM
    constexpr std::uint32_t SIGTERM = 15;
#endif

#ifndef MSG_WAITALL
    constexpr std::uint32_t MSG_WAITALL = 8;
#endif

    constexpr std::uint32_t SP_REGISTER = 13;
    constexpr std::uint32_t LR_REGISTER = 14;
    constexpr std::uint32_t PC_REGISTER = 15;
    constexpr std::uint32_t CPSR_REGISTER = 25;
    constexpr std::uint32_t D0_REGISTER = 26;
    constexpr std::uint32_t FPSCR_REGISTER = 42;

    /// Breakpoint Method
    enum class breakpoint_type {
        None, ///< None
        Execute, ///< Execution Breakpoint
        Read, ///< Read Breakpoint
        Write, ///< Write Breakpoint
        Access ///< Access (R/W) Breakpoint
    };

    struct breakpoint_address {
        std::uint32_t address;
        breakpoint_type type;
    };

    struct breakpoint {
        bool active;
        std::uint32_t addr;
        std::uint32_t len;
        std::array<std::uint8_t, 4> inst;
    };

    using breakpoint_map = std::map<std::uint32_t, breakpoint>;
        
    class gdbstub {
        int gdbserver_socket = -1;

        std::uint8_t command_buffer[GDB_BUFFER_SIZE];
        std::uint32_t command_length;

        std::uint32_t latest_signal = 0;
        bool memory_break = false;

        thread_ptr current_thread = nullptr;

        // Binding to a port within the reserved ports range (0-1023) requires root permissions,
        // so default to a port outside of that range.
        std::uint16_t gdbstub_port = 24689;

        bool halt_loop = true;
        bool step_loop = false;
        bool send_trap = false;

        // If set to false, the server will never be started and no
        // gdbstub-related functions will be executed.
        std::atomic<bool> server_enabled;

#ifdef _WIN32
        WSADATA InitData;
#endif
        breakpoint_map breakpoints_execute;
        breakpoint_map breakpoints_read;
        breakpoint_map breakpoints_write;

        system *sys;

    protected:
        bool is_data_available();

        std::uint8_t read_byte();

        void read_command();
        void read_register();
        void read_registers();
        void read_memory();

        void write_register();
        void write_registers();
        void write_memory();

        breakpoint_map &get_breakpoint_map(breakpoint_type type);

        void remove_breakpoint(breakpoint_type type, std::uint32_t addr);
        
        void send_packet(const char packet);
        void send_reply(const char *reply);
        void send_signal(thread_ptr thread, std::uint32_t signal, bool full = true);

        void handle_query();
        void handle_set_thread();
        void handle_thread_alive();

        void step();
        void continue_exec();

        bool commit_breakpoint(breakpoint_type type, std::uint32_t addr, std::uint32_t len);
        void add_breakpoint();
        void remove_breakpoint();

        void init(const std::uint16_t port);

    public:

        /**
         * Set the port the gdbstub should use to listen for connections.
         *
         * @param port Port to listen for connection
         */
        void set_server_port(const std::uint16_t port);

        /**
         * Starts or stops the server if possible.
         *
         * @param status Set the server to enabled or disabled.
         */
        void toggle_server(bool status);

        /// Start the gdbstub server.
        void init(eka2l1::system *sys);

        /// Stop gdbstub server.
        void shutdown_gdb();

        /// Checks if the gdbstub server is enabled.
        bool is_server_enabled();

        /// Returns true if there is an active socket connection.
        bool is_connected();

        /**
         * Signal to the gdbstub server that it should halt CPU execution.
         *
         * @param is_memory_break If true, the break resulted from a memory breakpoint.
         */
        void break_exec(const bool is_memory_break = false);

        /// Determine if there was a memory breakpoint.
        bool is_memory_break();

        /// Read and handle packet from gdb client.
        void handle_packet();

        breakpoint_address get_next_breakpoint_from_addr(std::uint32_t addr, 
            breakpoint_type type);

        /**
        * Check if a breakpoint of the specified type exists at the given address.
        *
        * @param addr Address of breakpoint.
        * @param type Type of breakpoint.
        */
        bool check_breakpoint(std::uint32_t addr, breakpoint_type type);

        // If set to true, the CPU will halt at the beginning of the next CPU loop.
        bool get_cpu_halt_flag();

        // If set to true and the CPU is halted, the CPU will step one instruction.
        bool get_cpu_step_flag();

        /**
         * When set to true, the CPU will step one instruction when the CPU is halted next.
         *
         * @param is_step
         */
        void set_cpu_step_flag(bool is_step);

        /**
         * Send trap signal from thread back to the gdbstub server.
         *
         * @param thread Sending thread.
         * @param trap Trap no.
         */
        void send_trap_gdb(thread_ptr thread, int trap);
    };
}
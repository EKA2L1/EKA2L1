// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#include <algorithm>
#include <atomic>
#include <climits>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <numeric>

#include <common/log.h>
#include <common/platform.h>

#include <arm/arm_interface.h>
#include <epoc/epoc.h>
#include <epoc/kernel/process.h>
#include <gdbstub/gdbstub.h>
//#include "epoc/loader/loader.h"
#include <epoc/kernel.h>
#include <epoc/mem.h>

namespace eka2l1 {
    // For sample XML files see the GDB source /gdb/features
    // GDB also wants the l character at the start
    // This XML defines what the registers are for this specific ARM device

    constexpr char target_xml[] =
        R"(l<?xml version="1.0"?>
        <!DOCTYPE target SYSTEM "gdb-target.dtd">
        <target version="1.0">
          <feature name="org.gnu.gdb.arm.core">
            <reg name="r0" bitsize="32"/>
            <reg name="r1" bitsize="32"/>
            <reg name="r2" bitsize="32"/>
            <reg name="r3" bitsize="32"/>
            <reg name="r4" bitsize="32"/>
            <reg name="r5" bitsize="32"/>
            <reg name="r6" bitsize="32"/>
            <reg name="r7" bitsize="32"/>
            <reg name="r8" bitsize="32"/>
            <reg name="r9" bitsize="32"/>
            <reg name="r10" bitsize="32"/>
            <reg name="r11" bitsize="32"/>
            <reg name="r12" bitsize="32"/>
            <reg name="sp" bitsize="32" type="data_ptr"/>
            <reg name="lr" bitsize="32"/>
            <reg name="pc" bitsize="32" type="code_ptr"/>
            <!-- The CPSR is register 25, rather than register 16, because
                 the FPA registers historically were placed between the PC
                 and the CPSR in the "g" packet.  -->
            <reg name="cpsr" bitsize="32" regnum="25"/>
          </feature>
          <feature name="org.gnu.gdb.arm.vfp">
            <reg name="d0" bitsize="64" type="float"/>
            <reg name="d1" bitsize="64" type="float"/>
            <reg name="d2" bitsize="64" type="float"/>
            <reg name="d3" bitsize="64" type="float"/>
            <reg name="d4" bitsize="64" type="float"/>
            <reg name="d5" bitsize="64" type="float"/>
            <reg name="d6" bitsize="64" type="float"/>
            <reg name="d7" bitsize="64" type="float"/>
            <reg name="d8" bitsize="64" type="float"/>
            <reg name="d9" bitsize="64" type="float"/>
            <reg name="d10" bitsize="64" type="float"/>
            <reg name="d11" bitsize="64" type="float"/>
            <reg name="d12" bitsize="64" type="float"/>
            <reg name="d13" bitsize="64" type="float"/>
            <reg name="d14" bitsize="64" type="float"/>
            <reg name="d15" bitsize="64" type="float"/>
            <reg name="fpscr" bitsize="32" type="int" group="float"/>
          </feature>
        </target>
        )";

    static thread_ptr find_thread_by_id(kernel_system *kern, const std::uint32_t id) {
        kernel_obj_ptr obj = kern->get_kernel_obj_by_id(id);

        if (!obj || obj->get_object_type() != kernel::object_type::thread) {
            return nullptr;
        }

        return std::reinterpret_pointer_cast<kernel::thread>(obj);
    }

    static std::uint32_t reg_read(std::size_t id, thread_ptr thread = nullptr) {
        if (!thread) {
            return 0;
        }

        if (id <= PC_REGISTER) {
            return thread->get_thread_context().cpu_registers[id];
        } else if (id == CPSR_REGISTER) {
            return thread->get_thread_context().cpsr;
        } else {
            return 0;
        }
    }

    static void reg_write(std::size_t id, std::uint32_t val, thread_ptr thread = nullptr) {
        if (!thread) {
            return;
        }

        if (id <= PC_REGISTER) {
            thread->get_thread_context().cpu_registers[id] = val;
        } else if (id == CPSR_REGISTER) {
            thread->get_thread_context().cpsr = val;
        }
    }

    static std::uint64_t fpu_read(std::size_t id, thread_ptr thread = nullptr) {
        if (!thread) {
            return 0;
        }

        if (id >= D0_REGISTER && id < FPSCR_REGISTER) {
            std::uint64_t ret = thread->get_thread_context().fpu_registers[2 * (id - D0_REGISTER)];
            ret |= static_cast<std::uint64_t>(thread->get_thread_context().fpu_registers[2 * (id - D0_REGISTER) + 1]) << 32;
            return ret;
        } else if (id == FPSCR_REGISTER) {
            return thread->get_thread_context().fpscr;
        } else {
            return 0;
        }
    }

    static void fpu_write(std::size_t id, std::uint64_t val, thread_ptr thread = nullptr) {
        if (!thread) {
            return;
        }

        if (id >= D0_REGISTER && id < FPSCR_REGISTER) {
            thread->get_thread_context().fpu_registers[2 * (id - D0_REGISTER)] = static_cast<std::uint32_t>(val);
            thread->get_thread_context().fpu_registers[2 * (id - D0_REGISTER) + 1] = static_cast<std::uint32_t>(val >> 32);
        } else if (id == FPSCR_REGISTER) {
            thread->get_thread_context().fpscr = static_cast<std::uint32_t>(val);
        }
    }

    static std::uint8_t hex_char_to_value(std::uint8_t hex) {
        if (hex >= '0' && hex <= '9') {
            return hex - '0';
        } else if (hex >= 'a' && hex <= 'f') {
            return hex - 'a' + 0xA;
        } else if (hex >= 'A' && hex <= 'F') {
            return hex - 'A' + 0xA;
        }

        LOG_ERROR("Invalid nibble: {:c} {:02x}", hex, hex);
        return 0;
    }

    /**
     * Turn nibble of byte into hex string character.
     *
     * @param n Nibble to be turned into hex character.
     */
    static std::uint8_t nibble_to_hex(std::uint8_t n) {
        n &= 0xF;
        if (n < 0xA) {
            return '0' + n;
        } else {
            return 'a' + n - 0xA;
        }
    }

    /**
     * Converts input hex string characters into an array of equivalent of std::uint8_t bytes.
     *
     * @param src Pointer to array of output hex string characters.
     * @param len Length of src array.
     */
    static std::uint32_t hex_to_int(const std::uint8_t *src, std::size_t len) {
        std::uint32_t output = 0;
        while (len-- > 0) {
            output = (output << 4) | hex_char_to_value(src[0]);
            src++;
        }
        return output;
    }

    /**
     * Converts input array of std::uint8_t bytes into their equivalent hex string characters.
     *
     * @param dest Pointer to buffer to store output hex string characters.
     * @param src Pointer to array of std::uint8_t bytes.
     * @param len Length of src array.
     */
    static void mem_to_gdb_hex(std::uint8_t *dest, const std::uint8_t *src, std::size_t len) {
        while (len-- > 0) {
            std::uint8_t tmp = *src++;
            *dest++ = nibble_to_hex(tmp >> 4);
            *dest++ = nibble_to_hex(tmp);
        }
    }

    /**
     * Converts input gdb-formatted hex string characters into an array of equivalent of std::uint8_t bytes.
     *
     * @param dest Pointer to buffer to store std::uint8_t bytes.
     * @param src Pointer to array of output hex string characters.
     * @param len Length of src array.
     */
    static void gdb_hex_to_mem(std::uint8_t *dest, const std::uint8_t *src, std::size_t len) {
        while (len-- > 0) {
            *dest++ = (hex_char_to_value(src[0]) << 4) | hex_char_to_value(src[1]);
            src += 2;
        }
    }

    /**
     * Convert a std::uint32_t into a gdb-formatted hex string.
     *
     * @param dest Pointer to buffer to store output hex string characters.
     * @param v    Value to convert.
     */
    static void int_to_gdb_hex(std::uint8_t *dest, std::uint32_t v) {
        for (int i = 0; i < 8; i += 2) {
            dest[i + 1] = nibble_to_hex(v >> (4 * i));
            dest[i] = nibble_to_hex(v >> (4 * (i + 1)));
        }
    }

    /**
     * Convert a gdb-formatted hex string into a std::uint32_t.
     *
     * @param src Pointer to hex string.
     */
    static std::uint32_t gdb_hex_to_int(const std::uint8_t *src) {
        std::uint32_t output = 0;

        for (int i = 0; i < 8; i += 2) {
            output = (output << 4) | hex_char_to_value(src[7 - i - 1]);
            output = (output << 4) | hex_char_to_value(src[7 - i]);
        }

        return output;
    }

    /**
     * Convert a std::uint64_t into a gdb-formatted hex string.
     *
     * @param dest Pointer to buffer to store output hex string characters.
     * @param v    Value to convert.
     */
    static void long_to_gdb_hex(std::uint8_t *dest, std::uint64_t v) {
        for (int i = 0; i < 16; i += 2) {
            dest[i + 1] = nibble_to_hex(static_cast<std::uint8_t>(v >> (4 * i)));
            dest[i] = nibble_to_hex(static_cast<std::uint8_t>(v >> (4 * (i + 1))));
        }
    }

    /**
     * Convert a gdb-formatted hex string into a std::uint64_t.
     *
     * @param src Pointer to hex string.
     */
    static std::uint64_t gdb_hex_to_long(const std::uint8_t *src) {
        std::uint64_t output = 0;

        for (int i = 0; i < 16; i += 2) {
            output = (output << 4) | hex_char_to_value(src[15 - i - 1]);
            output = (output << 4) | hex_char_to_value(src[15 - i]);
        }

        return output;
    }

    /// Read a byte from the gdb client.
    std::uint8_t gdbstub::read_byte() {
        std::uint8_t c;
        std::size_t received_size = recv(gdbserver_socket, reinterpret_cast<char *>(&c), 1, MSG_WAITALL);

        if (received_size != 1) {
            LOG_ERROR("recv failed : {}", received_size);
            this->shutdown_gdb();
        }

        return c;
    }

    /// Calculate the checksum of the current command buffer.
    static std::uint8_t calculate_checksum(const std::uint8_t *buffer, std::size_t length) {
        return static_cast<std::uint8_t>(std::accumulate(buffer, buffer + length, 0, std::plus<std::uint8_t>()));
    }

    /**
     * Get the map of breakpoints for a given breakpoint type.
     *
     * @param type Type of breakpoint map.
     */
    breakpoint_map &gdbstub::get_breakpoint_map(breakpoint_type type) {
        switch (type) {
        case breakpoint_type::Execute:
            return breakpoints_execute;
        case breakpoint_type::Read:
            return breakpoints_read;
        case breakpoint_type::Write:
            return breakpoints_write;
        default:
            return breakpoints_read;
        }
    }

    /**
     * Remove the breakpoint from the given address of the specified type.
     *
     * @param type Type of breakpoint.
     * @param addr Address of breakpoint.
     */
    void gdbstub::remove_breakpoint(breakpoint_type type, std::uint32_t addr) {
        breakpoint_map &p = get_breakpoint_map(type);

        const auto bp = p.find(addr);
        if (bp == p.end()) {
            return;
        }

        LOG_DEBUG("gdb: removed a breakpoint: {:08x} bytes at {:08x} of type {}",
            bp->second.len, bp->second.addr, static_cast<int>(type));

        sys->get_memory_system()->write(bp->second.addr, &(bp->second.inst[0]),
            static_cast<std::uint32_t>(bp->second.inst.size()));
        // TODO: Clear instruction cache

        p.erase(addr);
    }

    breakpoint_address gdbstub::get_next_breakpoint_from_addr(std::uint32_t addr, breakpoint_type type) {
        const breakpoint_map &p = get_breakpoint_map(type);
        const auto next_breakpoint = p.lower_bound(addr);
        breakpoint_address breakpoint;

        if (next_breakpoint != p.end()) {
            breakpoint.address = next_breakpoint->first;
            breakpoint.type = type;
        } else {
            breakpoint.address = 0;
            breakpoint.type = breakpoint_type::None;
        }

        return breakpoint;
    }

    bool gdbstub::check_breakpoint(std::uint32_t addr, breakpoint_type type) {
        if (!is_connected()) {
            return false;
        }

        const breakpoint_map &p = get_breakpoint_map(type);
        const auto bp = p.find(addr);

        if (bp == p.end()) {
            return false;
        }

        std::uint32_t len = bp->second.len;

        // IDA Pro defaults to 4-byte breakpoints for all non-hardware breakpoints
        // no matter if it's a 4-byte or 2-byte instruction. When you execute a
        // Thumb instruction with a 4-byte breakpoint set, it will set a breakpoint on
        // two instructions instead of the single instruction you placed the breakpoint
        // on. So, as a way to make sure that execution breakpoints are only breaking
        // on the instruction that was specified, set the length of an execution
        // breakpoint to 1. This should be fine since the CPU should never begin executing
        // an instruction anywhere except the beginning of the instruction.
        if (type == breakpoint_type::Execute) {
            len = 1;
        }

        if (bp->second.active && (addr >= bp->second.addr && addr < bp->second.addr + len)) {
            LOG_DEBUG(
                "Found breakpoint type {} @ {:08x}, range: {:08x}"
                " - {:08x} ({:x} bytes)",
                static_cast<int>(type), addr, bp->second.addr, bp->second.addr + len, len);
            return true;
        }

        return false;
    }

    /**
     * Send packet to gdb client.
     *
     * @param packet Packet to be sent to client.
     */
    void gdbstub::send_packet(const char packet) {
        std::size_t sent_size = send(gdbserver_socket, &packet, 1, 0);
        if (sent_size != 1) {
            LOG_ERROR("send failed");
        }
    }

    /**
     * Send reply to gdb client.
     *
     * @param reply Reply to be sent to client.
     */
    void gdbstub::send_reply(const char *reply) {
        if (!is_connected()) {
            return;
        }

        memset(command_buffer, 0, sizeof(command_buffer));

        command_length = static_cast<std::uint32_t>(strlen(reply));
        if (command_length + 4 > sizeof(command_buffer)) {
            LOG_ERROR("command_buffer overflow in send_reply");
            return;
        }

        memcpy(command_buffer + 1, reply, command_length);

        std::uint8_t checksum = calculate_checksum(command_buffer, command_length + 1);
        command_buffer[0] = GDB_STUB_START;
        command_buffer[command_length + 1] = GDB_STUB_END;
        command_buffer[command_length + 2] = nibble_to_hex(checksum >> 4);
        command_buffer[command_length + 3] = nibble_to_hex(checksum);

        std::uint8_t *ptr = command_buffer;
        std::uint32_t left = command_length + 4;
        while (left > 0) {
            int sent_size = send(gdbserver_socket, reinterpret_cast<char *>(ptr), left, 0);
            if (sent_size < 0) {
                LOG_ERROR("gdb: send failed");
                return shutdown_gdb();
            }

            left -= sent_size;
            ptr += sent_size;
        }
    }

    /// Handle query command from gdb client.
    void gdbstub::handle_query() {
        LOG_DEBUG("gdb: query '{}'", command_buffer + 1);

        const char *query = reinterpret_cast<const char *>(command_buffer + 1);

        if (strcmp(query, "TStatus") == 0) {
            send_reply("T0");
        } else if (strncmp(query, "Supported", strlen("Supported")) == 0) {
            // PacketSize needs to be large enough for target xml
            send_reply("PacketSize=2000;qXfer:features:read+;qXfer:threads:read+");
        } else if (strncmp(query, "Xfer:features:read:target.xml:",
                       strlen("Xfer:features:read:target.xml:"))
            == 0) {
            send_reply(target_xml);
        } else if (strncmp(query, "fThreadInfo", strlen("fThreadInfo")) == 0) {
            std::string val = "m";
            // TODO: Get list of threads
            val.pop_back();
            send_reply(val.c_str());
        } else if (strncmp(query, "sThreadInfo", strlen("sThreadInfo")) == 0) {
            send_reply("l");
        } else if (strncmp(query, "Xfer:threads:read", strlen("Xfer:threads:read")) == 0) {
            std::string buffer;
            buffer += "l<?xml version=\"1.0\"?>";
            buffer += "<threads>";
            /*
            const auto &threads = Kernel::GetThreadList();
            for (const auto &thread : threads) {
                buffer += fmt::format(R"*(<thread id="{:x}" name="Thread {:x}"></thread>)*",
                    thread->GetThreadId(), thread->GetThreadId());
            }*/
            buffer += "</threads>";
            send_reply(buffer.c_str());
        } else {
            send_reply("");
        }
    }

    /// Handle set thread command from gdb client.
    void gdbstub::handle_set_thread() {
        int thread_id = -1;
        if (command_buffer[2] != '-') {
            thread_id = static_cast<int>(hex_to_int(command_buffer + 2, command_length - 2));
        }
        if (thread_id >= 1) {
            current_thread = find_thread_by_id(sys->get_kernel_system(), thread_id);
        }
        if (!current_thread) {
            thread_id = 1;
            current_thread = find_thread_by_id(sys->get_kernel_system(), thread_id);
        }
        if (current_thread) {
            send_reply("OK");
            return;
        }
        send_reply("E01");
    }

    /// Handle thread alive command from gdb client.
    void gdbstub::handle_thread_alive() {
        int thread_id = static_cast<int>(hex_to_int(command_buffer + 1, command_length - 1));
        if (thread_id == 0) {
            thread_id = 1;
        }
        if (find_thread_by_id(sys->get_kernel_system(), thread_id)) {
            send_reply("OK");
            return;
        }
        send_reply("E01");
    }

    /**
     * Send signal packet to client.
     *
     * @param signal Signal to be sent to client.
     */
    void gdbstub::send_signal(thread_ptr thread, std::uint32_t signal, bool full) {
        if (gdbserver_socket == -1) {
            return;
        }

        latest_signal = signal;

        if (!thread) {
            full = false;
        }

        std::string buffer;
        if (full) {
            buffer = fmt::format("T{:02x}{:02x}:{:08x};{:02x}:{:08x};{:02x}:{:08x}", latest_signal,
                PC_REGISTER, htonl(sys->get_cpu()->get_pc()), SP_REGISTER,
                htonl(sys->get_cpu()->get_sp()), LR_REGISTER,
                htonl(sys->get_cpu()->get_lr()));
        } else {
            buffer = fmt::format("T{:02x}", latest_signal);
        }

        if (thread) {
            buffer += fmt::format(";thread:{:x};", thread->unique_id());
        }

        LOG_DEBUG("Response: {}", buffer);
        send_reply(buffer.c_str());
    }

    /// Read command from gdb client.
    void gdbstub::read_command() {
        command_length = 0;
        memset(command_buffer, 0, sizeof(command_buffer));

        std::uint8_t c = read_byte();
        if (c == '+') {
            // ignore ack
            return;
        } else if (c == 0x03) {
            LOG_INFO("gdb: found break command");
            halt_loop = true;
            send_signal(current_thread, SIGTRAP);
            return;
        } else if (c != GDB_STUB_START) {
            LOG_DEBUG("gdb: read invalid byte {:02x}", c);
            return;
        }

        while ((c = read_byte()) != GDB_STUB_END) {
            if (command_length >= sizeof(command_buffer)) {
                LOG_ERROR("gdb: command_buffer overflow");
                send_packet(GDB_STUB_NACK);
                return;
            }
            command_buffer[command_length++] = c;
        }

        std::uint8_t checksum_received = hex_char_to_value(read_byte()) << 4;
        checksum_received |= hex_char_to_value(read_byte());

        std::uint8_t checksum_calculated = calculate_checksum(command_buffer, command_length);

        if (checksum_received != checksum_calculated) {
            LOG_ERROR(
                "gdb: invalid checksum: calculated {:02x} and read {:02x} for ${}# (length: {})",
                checksum_calculated, checksum_received, command_buffer, command_length);

            command_length = 0;

            send_packet(GDB_STUB_NACK);
            return;
        }

        send_packet(GDB_STUB_ACK);
    }

    /// Check if there is data to be read from the gdb client.
    bool gdbstub::is_data_available() {
        if (!is_connected()) {
            return false;
        }

        fd_set fd_socket;

        FD_ZERO(&fd_socket);
        FD_SET(gdbserver_socket, &fd_socket);

        struct timeval t;
        t.tv_sec = 0;
        t.tv_usec = 0;

        if (select(gdbserver_socket + 1, &fd_socket, nullptr, nullptr, &t) < 0) {
            LOG_ERROR("select failed");
            return false;
        }

        return FD_ISSET(gdbserver_socket, &fd_socket) != 0;
    }

    /// Send requested register to gdb client.
    void gdbstub::read_register() {
        static std::uint8_t reply[64];
        memset(reply, 0, sizeof(reply));

        std::uint32_t id = hex_char_to_value(command_buffer[1]);
        if (command_buffer[2] != '\0') {
            id <<= 4;
            id |= hex_char_to_value(command_buffer[2]);
        }

        if (id <= PC_REGISTER) {
            int_to_gdb_hex(reply, reg_read(id, current_thread));
        } else if (id == CPSR_REGISTER) {
            int_to_gdb_hex(reply, reg_read(id, current_thread));
        } else if (id >= D0_REGISTER && id < FPSCR_REGISTER) {
            long_to_gdb_hex(reply, fpu_read(id, current_thread));
        } else if (id == FPSCR_REGISTER) {
            int_to_gdb_hex(reply, static_cast<std::uint32_t>(fpu_read(id, current_thread)));
        } else {
            return send_reply("E01");
        }

        send_reply(reinterpret_cast<char *>(reply));
    }

    /// Send all registers to the gdb client.
    void gdbstub::read_registers() {
        static std::uint8_t buffer[GDB_BUFFER_SIZE - 4];
        memset(buffer, 0, sizeof(buffer));

        std::uint8_t *bufptr = buffer;

        for (std::uint32_t reg = 0; reg <= PC_REGISTER; reg++) {
            int_to_gdb_hex(bufptr + reg * 8, reg_read(reg, current_thread));
        }

        bufptr += 16 * 8;

        int_to_gdb_hex(bufptr, reg_read(CPSR_REGISTER, current_thread));

        bufptr += 8;

        for (std::uint32_t reg = D0_REGISTER; reg < FPSCR_REGISTER; reg++) {
            long_to_gdb_hex(bufptr + reg * 16, fpu_read(reg, current_thread));
        }

        bufptr += 16 * 16;

        int_to_gdb_hex(bufptr, static_cast<std::uint32_t>(fpu_read(FPSCR_REGISTER, current_thread)));

        send_reply(reinterpret_cast<char *>(buffer));
    }

    /// Modify data of register specified by gdb client.
    void gdbstub::write_register() {
        const std::uint8_t *buffer_ptr = command_buffer + 3;

        std::uint32_t id = hex_char_to_value(command_buffer[1]);
        if (command_buffer[2] != '=') {
            ++buffer_ptr;
            id <<= 4;
            id |= hex_char_to_value(command_buffer[2]);
        }

        if (id <= PC_REGISTER) {
            reg_write(id, gdb_hex_to_int(buffer_ptr), current_thread);
        } else if (id == CPSR_REGISTER) {
            reg_write(id, gdb_hex_to_int(buffer_ptr), current_thread);
        } else if (id >= D0_REGISTER && id < FPSCR_REGISTER) {
            fpu_write(id, gdb_hex_to_long(buffer_ptr), current_thread);
        } else if (id == FPSCR_REGISTER) {
            fpu_write(id, gdb_hex_to_int(buffer_ptr), current_thread);
        } else {
            return send_reply("E01");
        }

        sys->get_cpu()->load_context(current_thread->get_thread_context());

        send_reply("OK");
    }

    /// Modify all registers with data received from the client.
    void gdbstub::write_registers() {
        const std::uint8_t *buffer_ptr = command_buffer + 1;

        if (command_buffer[0] != 'G')
            return send_reply("E01");

        for (std::uint32_t i = 0, reg = 0; reg <= FPSCR_REGISTER; i++, reg++) {
            if (reg <= PC_REGISTER) {
                reg_write(reg, gdb_hex_to_int(buffer_ptr + i * 8));
            } else if (reg == CPSR_REGISTER) {
                reg_write(reg, gdb_hex_to_int(buffer_ptr + i * 8));
            } else if (reg == CPSR_REGISTER - 1) {
                // Dummy FPA register, ignore
            } else if (reg < CPSR_REGISTER) {
                // Dummy FPA registers, ignore
                i += 2;
            } else if (reg >= D0_REGISTER && reg < FPSCR_REGISTER) {
                fpu_write(reg, gdb_hex_to_long(buffer_ptr + i * 16));
                i++; // Skip padding
            } else if (reg == FPSCR_REGISTER) {
                fpu_write(reg, gdb_hex_to_int(buffer_ptr + i * 8));
            }
        }

        sys->get_cpu()->load_context(current_thread->get_thread_context());
        send_reply("OK");
    }

    /// Read location in memory specified by gdb client.
    void gdbstub::read_memory() {
        static std::uint8_t reply[GDB_BUFFER_SIZE - 4];

        auto start_offset = command_buffer + 1;
        auto addr_pos = std::find(start_offset, command_buffer + command_length, ',');
        std::uint32_t addr = hex_to_int(start_offset, static_cast<std::uint32_t>(addr_pos - start_offset));

        start_offset = addr_pos + 1;
        std::uint32_t len = hex_to_int(start_offset, static_cast<std::uint32_t>((command_buffer + command_length) - start_offset));

        LOG_DEBUG("gdb: addr: {:08x} len: {:08x}", addr, len);

        if (len * 2 > sizeof(reply)) {
            send_reply("E01");
        }

        if (!sys->get_memory_system()->get_real_pointer(addr)) {
            return send_reply("E00");
        }

        std::vector<std::uint8_t> data(len);
        sys->get_memory_system()->read(addr, &data[0], len);

        mem_to_gdb_hex(reply, data.data(), len);
        reply[len * 2] = '\0';
        send_reply(reinterpret_cast<char *>(reply));
    }

    /// Modify location in memory with data received from the gdb client.
    void gdbstub::write_memory() {
        auto start_offset = command_buffer + 1;
        auto addr_pos = std::find(start_offset, command_buffer + command_length, ',');
        std::uint32_t addr = hex_to_int(start_offset, static_cast<std::uint32_t>(addr_pos - start_offset));

        start_offset = addr_pos + 1;
        auto len_pos = std::find(start_offset, command_buffer + command_length, ':');
        std::uint32_t len = hex_to_int(start_offset, static_cast<std::uint32_t>(len_pos - start_offset));

        if (!sys->get_memory_system()->get_real_pointer(addr)) {
            return send_reply("E00");
        }

        std::vector<std::uint8_t> data(len);

        gdb_hex_to_mem(data.data(), len_pos + 1, len);
        sys->get_memory_system()->write(addr, &data[0], len);
        // TODO: Clear instruction cache

        send_reply("OK");
    }

    void gdbstub::break_exec(bool is_memory_break) {
        send_trap = true;
        memory_break = is_memory_break;
    }

    /// Tell the CPU that it should perform a single step.
    void gdbstub::step() {
        if (command_length > 1) {
            reg_write(PC_REGISTER, gdb_hex_to_int(command_buffer + 1), current_thread);
            sys->get_cpu()->load_context(current_thread->get_thread_context());
        }

        step_loop = true;
        halt_loop = true;
        send_trap = true;
        // TODO: Clear instruction cache
    }

    bool gdbstub::is_memory_break() {
        if (is_connected()) {
            return false;
        }

        return memory_break;
    }

    /// Tell the CPU to continue executing.
    void gdbstub::continue_exec() {
        memory_break = false;
        step_loop = false;
        halt_loop = false;

        // TODO: Clear inst cache
    }

    /**
     * Commit breakpoint to list of breakpoints.
     *
     * @param type Type of breakpoint.
     * @param addr Address of breakpoint.
     * @param len Length of breakpoint.
     */
    bool gdbstub::commit_breakpoint(breakpoint_type type, std::uint32_t addr, std::uint32_t len) {
        breakpoint_map &p = get_breakpoint_map(type);

        breakpoint br;
        br.active = true;
        br.addr = addr;
        br.len = static_cast<std::uint32_t>(len);

        sys->get_memory_system()->read(addr, &br.inst[0],
            static_cast<std::uint32_t>(br.inst.size()));

        std::array<std::uint8_t, 4> btrap{ 0x70, 0x00, 0x20, 0xe1 };
        sys->get_memory_system()->write(addr, &(btrap[0]),
            static_cast<std::uint32_t>(btrap.size()));

        // TODO: Clear instructions cache
        p.insert({ addr, br });

        LOG_DEBUG("gdb: added {} breakpoint: {:08x} bytes at {:08x}",
            static_cast<int>(type), br.len, br.addr);

        return true;
    }

    /// Handle add breakpoint command from gdb client.
    void gdbstub::add_breakpoint() {
        breakpoint_type type;

        std::uint8_t type_id = hex_char_to_value(command_buffer[1]);
        switch (type_id) {
        case 0:
        case 1:
            type = breakpoint_type::Execute;
            break;
        case 2:
            type = breakpoint_type::Write;
            break;
        case 3:
            type = breakpoint_type::Read;
            break;
        case 4:
            type = breakpoint_type::Access;
            break;
        default:
            return send_reply("E01");
        }

        auto start_offset = command_buffer + 3;
        auto addr_pos = std::find(start_offset, command_buffer + command_length, ',');
        std::uint32_t addr = hex_to_int(start_offset, static_cast<std::uint32_t>(addr_pos - start_offset));

        start_offset = addr_pos + 1;
        std::uint32_t len = hex_to_int(start_offset, static_cast<std::uint32_t>((command_buffer + command_length) - start_offset));

        if (type == breakpoint_type::Access) {
            // Access is made up of Read and Write types, so add both breakpoints
            type = breakpoint_type::Read;

            if (!commit_breakpoint(type, addr, len)) {
                return send_reply("E02");
            }

            type = breakpoint_type::Write;
        }

        if (!commit_breakpoint(type, addr, len)) {
            return send_reply("E02");
        }

        send_reply("OK");
    }

    /// Handle remove breakpoint command from gdb client.
    void gdbstub::remove_breakpoint() {
        breakpoint_type type;

        std::uint8_t type_id = hex_char_to_value(command_buffer[1]);
        switch (type_id) {
        case 0:
        case 1:
            type = breakpoint_type::Execute;
            break;
        case 2:
            type = breakpoint_type::Write;
            break;
        case 3:
            type = breakpoint_type::Read;
            break;
        case 4:
            type = breakpoint_type::Access;
            break;
        default:
            return send_reply("E01");
        }

        auto start_offset = command_buffer + 3;
        auto addr_pos = std::find(start_offset, command_buffer + command_length, ',');
        std::uint32_t addr = hex_to_int(start_offset, static_cast<std::uint32_t>(addr_pos - start_offset));

        if (type == breakpoint_type::Access) {
            // Access is made up of Read and Write types, so add both breakpoints
            type = breakpoint_type::Read;
            remove_breakpoint(type, addr);

            type = breakpoint_type::Write;
        }

        remove_breakpoint(type, addr);
        send_reply("OK");
    }

    void gdbstub::handle_packet() {
        if (!is_connected()) {
            return;
        }

        if (!is_data_available()) {
            return;
        }

        read_command();
        if (command_length == 0) {
            return;
        }

        LOG_DEBUG("Packet: {}", command_buffer);

        switch (command_buffer[0]) {
        case 'q':
            handle_query();
            break;
        case 'H':
            handle_set_thread();
            break;
        case '?':
            send_signal(current_thread, latest_signal);
            break;
        case 'k':
            this->shutdown_gdb();
            LOG_INFO("killed by gdb");
            return;
        case 'g':
            read_registers();
            break;
        case 'G':
            write_registers();
            break;
        case 'p':
            read_register();
            break;
        case 'P':
            write_register();
            break;
        case 'm':
            read_memory();
            break;
        case 'M':
            write_memory();
            break;
        case 's':
            step();
            return;
        case 'C':
        case 'c':
            continue_exec();
            return;
        case 'z':
            remove_breakpoint();
            break;
        case 'Z':
            add_breakpoint();
            break;
        case 'T':
            handle_thread_alive();
            break;
        default:
            send_reply("");
            break;
        }
    }

    void gdbstub::set_server_port(const std::uint16_t port) {
        gdbstub_port = port;
    }

    void gdbstub::toggle_server(bool status) {
        if (status) {
            server_enabled = status;

            // Start server
            if (!is_connected()) {
                init(sys);
            }
        } else {
            // Stop server
            if (is_connected()) {
                shutdown_gdb();
            }

            server_enabled = status;
        }
    }

    void gdbstub::init(const std::uint16_t port) {
        if (!server_enabled) {
            // Set the halt loop to false in case the user enabled the gdbstub mid-execution.
            // This way the CPU can still execute normally.
            halt_loop = false;
            step_loop = false;
            return;
        }

        // Setup initial gdbstub status
        halt_loop = true;
        step_loop = false;

        breakpoints_execute.clear();
        breakpoints_read.clear();
        breakpoints_write.clear();

        // Start gdb server
        LOG_INFO("Starting GDB server on port {}...", port);

        sockaddr_in saddr_server = {};
        saddr_server.sin_family = AF_INET;
        saddr_server.sin_port = htons(port);
        saddr_server.sin_addr.s_addr = INADDR_ANY;

#if EKA2L1_PLATFORM(WIN32)
        WSAStartup(MAKEWORD(2, 2), &InitData);
#endif

        int tmpsock = static_cast<int>(socket(PF_INET, SOCK_STREAM, 0));
        if (tmpsock == -1) {
            LOG_ERROR("Failed to create gdb socket");
        }

        // Set socket to SO_REUSEADDR so it can always bind on the same port
        int reuse_enabled = 1;
        if (setsockopt(tmpsock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse_enabled,
                sizeof(reuse_enabled))
            < 0) {
            LOG_ERROR("Failed to set gdb socket option");
        }

        const sockaddr *server_addr = reinterpret_cast<const sockaddr *>(&saddr_server);
        socklen_t server_addrlen = sizeof(saddr_server);
        if (bind(tmpsock, server_addr, server_addrlen) < 0) {
            LOG_ERROR("Failed to bind gdb socket");
        }

        if (listen(tmpsock, 1) < 0) {
            LOG_ERROR("Failed to listen to gdb socket");
        }

        // Wait for gdb to connect
        LOG_INFO("Waiting for gdb to connect...");
        sockaddr_in saddr_client;
        sockaddr *client_addr = reinterpret_cast<sockaddr *>(&saddr_client);
        socklen_t client_addrlen = sizeof(saddr_client);
        gdbserver_socket = static_cast<int>(accept(tmpsock, client_addr, &client_addrlen));
        if (gdbserver_socket < 0) {
            // In the case that we couldn't start the server for whatever reason, just start CPU
            // execution like normal.
            halt_loop = false;
            step_loop = false;

            LOG_ERROR("Failed to accept gdb client");
        } else {
            LOG_INFO("Client connected.");
            saddr_client.sin_addr.s_addr = ntohl(saddr_client.sin_addr.s_addr);
        }

        // Clean up temporary socket if it's still alive at this point.
        if (tmpsock != -1) {
            shutdown(tmpsock, SHUT_RDWR);
        }
    }

    void gdbstub::init(eka2l1::system *nsys) {
        sys = nsys;
        init(gdbstub_port);
    }

    void gdbstub::shutdown_gdb() {
        if (!server_enabled) {
            return;
        }

        LOG_INFO("Stopping GDB ...");
        if (gdbserver_socket != -1) {
            shutdown(gdbserver_socket, SHUT_RDWR);
            gdbserver_socket = -1;
        }

#if EKA2L1_PLATFORM(WIN32)
        WSACleanup();
#endif

        LOG_INFO("GDB stopped.");
    }

    bool gdbstub::is_server_enabled() {
        return server_enabled;
    }

    bool gdbstub::is_connected() {
        return is_server_enabled() && gdbserver_socket != -1;
    }

    bool gdbstub::get_cpu_halt_flag() {
        return halt_loop;
    }

    bool gdbstub::get_cpu_step_flag() {
        return step_loop;
    }

    void gdbstub::set_cpu_step_flag(bool is_step) {
        step_loop = is_step;
    }

    void gdbstub::send_trap_gdb(thread_ptr thread, int trap) {
        if (!send_trap) {
            return;
        }

        if (!halt_loop || current_thread == thread) {
            current_thread = thread;
            send_signal(thread, trap);
        }
        halt_loop = true;
        send_trap = false;
    }
};
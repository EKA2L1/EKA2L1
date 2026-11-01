/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <common/buffer.h>
#include <common/log.h>
#include <loader/e32img.h>

#include <cstdint>
#include <fstream>
#include <string_view>

namespace {
    constexpr std::uint32_t BYTE_PAIR_COMPRESSION = 0x102822AA;
    constexpr std::uint32_t DEFLATE_COMPRESSION = 0x101F7AFC;

    struct cpu_info {
        const char *name;
        const char *ghidra_language;
    };

    cpu_info get_cpu_info(const eka2l1::loader::e32_cpu cpu) {
        using eka2l1::loader::e32_cpu;

        switch (cpu) {
        case e32_cpu::armv4:
            return { "ARMv4", "ARM:LE:32:v4t" };
        case e32_cpu::armv5:
            return { "ARMv5", "ARM:LE:32:v5t" };
        case e32_cpu::armv6:
            return { "ARMv6", "ARM:LE:32:v6" };
        default:
            return { nullptr, nullptr };
        }
    }

    void print_usage() {
        LOG_INFO(eka2l1::SYSTEM, "Usage: e32uncompress <input E32Image> <output raw binary>");
        LOG_INFO(eka2l1::SYSTEM, "Extracts the decompressed code section and prints Ghidra raw-import settings.");
    }
}

int main(int argc, char **argv) {
    eka2l1::log::setup_log(nullptr);
    eka2l1::log::toggle_console();

    if ((argc == 2) && ((std::string_view(argv[1]) == "--help") || (std::string_view(argv[1]) == "-h"))) {
        print_usage();
        return 0;
    }

    if (argc != 3) {
        print_usage();
        return 1;
    }

    eka2l1::common::ro_std_file_stream stream(argv[1], true);
    if (!stream.valid()) {
        LOG_ERROR(eka2l1::SYSTEM, "Unable to open input file: {}", argv[1]);
        return 2;
    }

    eka2l1::loader::e32img_header header{};
    eka2l1::loader::e32img_header_extended extended{};
    epocver version = epocver::eka1;
    std::uint32_t uncompressed_size = 0;

    if (eka2l1::loader::parse_e32img_header(&stream, header, extended, uncompressed_size, version) != 0) {
        LOG_ERROR(eka2l1::SYSTEM, "Input is not a valid E32Image: {}", argv[1]);
        return 3;
    }

    const cpu_info cpu = get_cpu_info(header.cpu);
    if (!cpu.name) {
        LOG_ERROR(eka2l1::SYSTEM, "Only ARM E32Images are supported (CPU identifier: 0x{:X})", static_cast<std::uint16_t>(header.cpu));
        return 4;
    }

    switch (header.compression_type) {
    case 0:
        LOG_INFO(eka2l1::SYSTEM, "Input is uncompressed");
        break;
    case BYTE_PAIR_COMPRESSION:
        LOG_INFO(eka2l1::SYSTEM, "Input uses byte-pair compression");
        break;
    case DEFLATE_COMPRESSION:
        LOG_INFO(eka2l1::SYSTEM, "Input uses deflate compression");
        break;
    default:
        LOG_ERROR(eka2l1::SYSTEM, "Unsupported E32Image compression type: 0x{:08X}", header.compression_type);
        return 5;
    }

    stream.seek(0, eka2l1::common::beg);
    auto image = eka2l1::loader::parse_e32img(&stream, false);
    if (!image) {
        LOG_ERROR(eka2l1::SYSTEM, "Failed to decompress or parse E32Image: {}", argv[1]);
        return 6;
    }

    if (image->header.code_size == 0) {
        LOG_ERROR(eka2l1::SYSTEM, "E32Image has an empty code section");
        return 7;
    }

    const char *code = image->data.data() + image->header.code_offset;
    std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
    if (!output) {
        LOG_ERROR(eka2l1::SYSTEM, "Unable to open output file: {}", argv[2]);
        return 8;
    }

    output.write(code, static_cast<std::streamsize>(image->header.code_size));
    output.close();
    if (!output) {
        LOG_ERROR(eka2l1::SYSTEM, "Failed to write output file: {}", argv[2]);
        return 9;
    }

    LOG_INFO(eka2l1::SYSTEM, "Wrote {} bytes to {}", image->header.code_size, argv[2]);
    LOG_INFO(eka2l1::SYSTEM, "Ghidra raw import: language = {}, image base = 0x{:08X}", cpu.ghidra_language, image->header.code_base);
    LOG_INFO(eka2l1::SYSTEM, "Entry point: file offset 0x{:X}, address 0x{:08X}", image->header.entry_point,
        image->header.code_base + image->header.entry_point);
    LOG_INFO(eka2l1::SYSTEM, "Text size: {} bytes; remaining code-section bytes may contain constants, imports, or exports", image->header.text_size);

    return 0;
}

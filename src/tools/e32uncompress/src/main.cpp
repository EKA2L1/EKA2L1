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

    const char *get_relocation_type_name(const std::uint16_t info) {
        using eka2l1::loader::relocation_type;

        switch (static_cast<relocation_type>(info & 0xF000)) {
        case relocation_type::reserved:
            return "reserved";
        case relocation_type::text:
            return "text";
        case relocation_type::data:
            return "data";
        case relocation_type::inferred:
            return "inferred";
        default:
            return "unknown";
        }
    }

    void print_relocations(const char *name, const eka2l1::loader::e32_reloc_section &section,
        const std::uint32_t offset) {
        std::size_t relocation_count = 0;
        for (const auto &entry : section.entries) {
            relocation_count += entry.rels_info.size();
        }

        LOG_INFO(eka2l1::SYSTEM, "{} relocations: offset = 0x{:08X}, size = {} bytes, header count = {}, parsed count = {}, blocks = {}",
            name, offset, section.size, section.num_relocs, relocation_count, section.entries.size());

        std::size_t relocation_index = 0;
        for (std::size_t block_index = 0; block_index < section.entries.size(); block_index++) {
            const auto &entry = section.entries[block_index];
            LOG_INFO(eka2l1::SYSTEM, "  Block {}: base = 0x{:08X}, size = {} bytes, entries = {}",
                block_index, entry.base, entry.size, entry.rels_info.size());

            for (const std::uint16_t info : entry.rels_info) {
                LOG_INFO(eka2l1::SYSTEM, "    [{}] raw = 0x{:04X}, page offset = 0x{:03X}, address = 0x{:08X}, type = {}",
                    relocation_index++, info, info & 0x0FFF, entry.base + (info & 0x0FFF), get_relocation_type_name(info));
            }
        }
    }

    void print_verbose_info(const eka2l1::loader::e32img &image) {
        const auto &header = image.header;
        const std::uint32_t header_format = (header.flags >> 24) & 0xF;

        LOG_INFO(eka2l1::SYSTEM, "E32Image header:");
        LOG_INFO(eka2l1::SYSTEM, "  Format: {}, header format = {}, image type = {}", is_epocver_eka1(image.epoc_ver) ? "EKA1" : "EKA2",
            header_format, header.uid1 == eka2l1::loader::e32_img_type::dll ? "DLL" : "EXE");
        LOG_INFO(eka2l1::SYSTEM, "  UIDs: 0x{:08X} 0x{:08X} 0x{:08X}, checksum = 0x{:08X}",
            static_cast<std::uint32_t>(header.uid1), header.uid2, header.uid3, header.check);
        LOG_INFO(eka2l1::SYSTEM, "  Version: {}.{}, PETRAN version: {}.{}.{}", header.major, header.minor,
            header.petran_major, header.petran_minor, header.petran_build);
        LOG_INFO(eka2l1::SYSTEM, "  CPU: 0x{:04X}, priority = {}, flags = 0x{:08X}, header CRC = 0x{:08X}",
            static_cast<std::uint16_t>(header.cpu), header.priority, header.flags, header.header_crc);
        LOG_INFO(eka2l1::SYSTEM, "  ABI: {}, fixed address: {}, entry point type: {}",
            ((header.flags >> 3) & 3) == 1 ? "EABI" : "GCC98r2", (header.flags & 4) != 0,
            ((header.flags >> 5) & 7) == 0 ? "EKA1" : "EKA2");
        LOG_INFO(eka2l1::SYSTEM, "  Timestamp: MSB = 0x{:08X}, LSB = 0x{:08X}", header.msb, header.lsb);
        LOG_INFO(eka2l1::SYSTEM, "  Code: offset = 0x{:08X}, base = 0x{:08X}, size = {}, text size = {}, entry point = 0x{:08X}",
            header.code_offset, header.code_base, header.code_size, header.text_size, header.entry_point);
        LOG_INFO(eka2l1::SYSTEM, "  Data: offset = 0x{:08X}, base = 0x{:08X}, size = {}, BSS size = {}",
            header.data_offset, header.data_base, header.data_size, header.bss_size);
        LOG_INFO(eka2l1::SYSTEM, "  Heap: minimum = {}, maximum = {}; stack = {}", header.heap_size_min, header.heap_size_max, header.stack_size);
        LOG_INFO(eka2l1::SYSTEM, "  Compression: type = 0x{:08X}, uncompressed payload size = {}, decompressed image size = {}",
            header.compression_type, image.uncompressed_size, image.data.size());
        LOG_INFO(eka2l1::SYSTEM, "  Directories: exports = 0x{:08X}, imports = 0x{:08X}, code relocations = 0x{:08X}, data relocations = 0x{:08X}",
            header.export_dir_offset, header.import_offset, header.code_reloc_offset, header.data_reloc_offset);

        if (header_format == 2) {
            const auto &extended = image.header_extended;
            LOG_INFO(eka2l1::SYSTEM, "Extended header:");
            LOG_INFO(eka2l1::SYSTEM, "  Secure ID = 0x{:08X}, vendor ID = 0x{:08X}", extended.info.secure_id, extended.info.vendor_id);
            LOG_INFO(eka2l1::SYSTEM, "  Capabilities: 0x{:08X} 0x{:08X}", extended.info.cap1, extended.info.cap2);
            LOG_INFO(eka2l1::SYSTEM, "  Exception descriptor: raw = 0x{:08X}, present = {}, offset = 0x{:08X}",
                extended.exception_des, (extended.exception_des & 1) != 0, extended.exception_des & ~1U);
            LOG_INFO(eka2l1::SYSTEM, "  Export description: size = {}, type = {}, value = 0x{:02X}, spare2 = 0x{:08X}",
                extended.export_desc_size, extended.export_desc_type, extended.export_desc, extended.spare2);
        }

        LOG_INFO(eka2l1::SYSTEM, "Export table: offset = 0x{:08X}, entries = {}", header.export_dir_offset, image.ed.syms.size());
        for (std::size_t i = 0; i < image.ed.syms.size(); i++) {
            const std::uint32_t export_offset = image.ed.syms[i];
            LOG_INFO(eka2l1::SYSTEM, "  [{}] ordinal = {}, offset = 0x{:08X}, address = 0x{:08X}",
                i, i + 1, export_offset, header.code_base + export_offset);
        }

        LOG_INFO(eka2l1::SYSTEM, "Import table: offset = 0x{:08X}, size = {} bytes, libraries = {}",
            header.import_offset, image.import_section.size, image.import_section.imports.size());
        for (std::size_t i = 0; i < image.import_section.imports.size(); i++) {
            const auto &import = image.import_section.imports[i];
            LOG_INFO(eka2l1::SYSTEM, "  [{}] {}: name offset = 0x{:08X}, imports = {}",
                i, import.dll_name, import.dll_name_offset, import.ordinals.size());
            for (std::size_t j = 0; j < import.ordinals.size(); j++) {
                if (is_epocver_eka1(image.epoc_ver)) {
                    LOG_INFO(eka2l1::SYSTEM, "    [{}] ordinal = {}", j, import.ordinals[j]);
                } else {
                    LOG_INFO(eka2l1::SYSTEM, "    [{}] code offset = 0x{:08X}", j, import.ordinals[j]);
                }
            }
        }

        LOG_INFO(eka2l1::SYSTEM, "Import address table: entries = {}", image.iat.its.size());
        for (std::size_t i = 0; i < image.iat.its.size(); i++) {
            LOG_INFO(eka2l1::SYSTEM, "  [{}] value = 0x{:08X}", i, image.iat.its[i]);
        }

        print_relocations("Code", image.code_reloc_section, header.code_reloc_offset);
        print_relocations("Data", image.data_reloc_section, header.data_reloc_offset);
    }

    void print_usage() {
        LOG_INFO(eka2l1::SYSTEM, "Usage: e32uncompress [--verbose] <input E32Image> <output raw binary>");
        LOG_INFO(eka2l1::SYSTEM, "Extracts the decompressed code section and prints Ghidra raw-import settings.");
        LOG_INFO(eka2l1::SYSTEM, "  -v, --verbose  Print headers, libraries, imports, exports, IAT entries, and relocations.");
    }
}

int main(int argc, char **argv) {
    eka2l1::log::setup_log(nullptr);
    eka2l1::log::toggle_console();

    bool verbose = false;
    const char *input_path = nullptr;
    const char *output_path = nullptr;

    for (int i = 1; i < argc; i++) {
        const std::string_view argument(argv[i]);
        if ((argument == "--help") || (argument == "-h")) {
            print_usage();
            return 0;
        }

        if ((argument == "--verbose") || (argument == "-v")) {
            verbose = true;
        } else if (!argument.empty() && (argument.front() == '-')) {
            LOG_ERROR(eka2l1::SYSTEM, "Unknown option: {}", argument);
            print_usage();
            return 1;
        } else if (!input_path) {
            input_path = argv[i];
        } else if (!output_path) {
            output_path = argv[i];
        } else {
            LOG_ERROR(eka2l1::SYSTEM, "Unexpected argument: {}", argument);
            print_usage();
            return 1;
        }
    }

    if (!input_path || !output_path) {
        print_usage();
        return 1;
    }

    eka2l1::common::ro_std_file_stream stream(input_path, true);
    if (!stream.valid()) {
        LOG_ERROR(eka2l1::SYSTEM, "Unable to open input file: {}", input_path);
        return 2;
    }

    eka2l1::loader::e32img_header header{};
    eka2l1::loader::e32img_header_extended extended{};
    epocver version = epocver::eka1;
    std::uint32_t uncompressed_size = 0;

    if (eka2l1::loader::parse_e32img_header(&stream, header, extended, uncompressed_size, version) != 0) {
        LOG_ERROR(eka2l1::SYSTEM, "Input is not a valid E32Image: {}", input_path);
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
    auto image = eka2l1::loader::parse_e32img(&stream, verbose);
    if (!image) {
        LOG_ERROR(eka2l1::SYSTEM, "Failed to decompress or parse E32Image: {}", input_path);
        return 6;
    }

    if (image->header.code_size == 0) {
        LOG_ERROR(eka2l1::SYSTEM, "E32Image has an empty code section");
        return 7;
    }

    const char *code = image->data.data() + image->header.code_offset;
    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        LOG_ERROR(eka2l1::SYSTEM, "Unable to open output file: {}", output_path);
        return 8;
    }

    output.write(code, static_cast<std::streamsize>(image->header.code_size));
    output.close();
    if (!output) {
        LOG_ERROR(eka2l1::SYSTEM, "Failed to write output file: {}", output_path);
        return 9;
    }

    LOG_INFO(eka2l1::SYSTEM, "Wrote {} bytes to {}", image->header.code_size, output_path);
    LOG_INFO(eka2l1::SYSTEM, "Ghidra raw import: language = {}, image base = 0x{:08X}", cpu.ghidra_language, image->header.code_base);
    LOG_INFO(eka2l1::SYSTEM, "Entry point: file offset 0x{:X}, address 0x{:08X}", image->header.entry_point,
        image->header.code_base + image->header.entry_point);
    LOG_INFO(eka2l1::SYSTEM, "Text size: {} bytes; remaining code-section bytes may contain constants, imports, or exports", image->header.text_size);

    if (verbose) {
        print_verbose_info(*image);
    }

    return 0;
}

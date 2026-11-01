/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <kernel/libmanager.h>
#include <loader/e32img.h>

#include <vfs/vfs.h>

#include <catch2/catch.hpp>
#include <common/buffer.h>
#include <common/crypt.h>
#include <common/fileutils.h>
#include <config/app_settings.h>
#include <config/config.h>
#include <cpu/arm_factory.h>
#include <kernel/codeseg.h>
#include <kernel/kernel.h>
#include <kernel/timing.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

using namespace eka2l1;

namespace {
    struct loader_fixture {
        config::state conf;
        config::app_settings settings{ &conf };
        ntimer timing{ 1000000 };
        io_system io;
        arm::exclusive_monitor_instance monitor = arm::create_exclusive_monitor(arm_emulator_type::dyncom, 1);
        arm::core_instance cpu = arm::create_core(monitor.get(), arm_emulator_type::dyncom);
        kernel_system kern{ nullptr, &timing, &io, &conf, &settings, nullptr, cpu.get(), nullptr };
        hle::lib_manager manager{ &kern, &io, nullptr };
    };

    loader::e32img make_image(const std::vector<std::string> &dependencies) {
        loader::e32img image{};
        image.epoc_ver = epocver::epoc94;
        image.header.uid1 = loader::e32_img_type::dll;
        image.header.code_base = 0x8000;
        image.header.code_size = 4;
        image.ed.syms = { 0x8000 };
        image.header.text_size = 4;
        image.data.resize(4);
        image.data[0] = 1;
        for (const auto &name : dependencies) {
            loader::e32img_import_block dependency{};
            dependency.dll_name = name;
            dependency.ordinals = { 0 };
            image.import_section.imports.push_back(dependency);
        }
        return image;
    }

    void write_image(const char *path, const std::vector<std::string> &dependencies) {
        auto image = make_image(dependencies);
        auto &header = image.header;
        header.sig = 0x434F5045;
        const std::uint32_t uids[] = { static_cast<std::uint32_t>(header.uid1), 0, 0 };
        header.check = crypt::calculate_checked_uid_checksum(uids);
        header.code_offset = sizeof(header);
        header.export_dir_offset = header.code_offset + header.code_size;
        header.export_dir_count = 1;
        header.import_offset = header.export_dir_offset + 4;
        header.dll_ref_table_count = static_cast<std::uint32_t>(dependencies.size());
        std::vector<char> imports(4 + dependencies.size() * 12);
        for (std::size_t i = 0; i < dependencies.size(); i++) {
            const std::uint32_t block[] = { static_cast<std::uint32_t>(imports.size()), 1, 0 };
            std::memcpy(imports.data() + 4 + i * 12, block, sizeof(block));
            imports.insert(imports.end(), dependencies[i].begin(), dependencies[i].end());
            imports.push_back(0);
        }
        const auto size = static_cast<std::uint32_t>(imports.size());
        std::memcpy(imports.data(), &size, sizeof(size));
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(&header), sizeof(header));
        out.write(image.data.data(), image.data.size());
        out.write(reinterpret_cast<const char *>(image.ed.syms.data()), 4);
        out.write(imports.data(), imports.size());
        REQUIRE(out.good());
    }

    constexpr std::array<std::uint8_t, 8> ARM_CODE = {
        0x00, 0x00, 0xA0, 0xE3,
        0x1E, 0xFF, 0x2F, 0xE1
    };

    void append16(std::vector<std::uint8_t> &buffer, const std::uint16_t value) {
        buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>(value >> 8));
    }

    void append32(std::vector<std::uint8_t> &buffer, const std::uint32_t value) {
        for (int i = 0; i < 4; i++) {
            buffer.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }

    void append_header(std::vector<std::uint8_t> &buffer, const eka2l1::loader::e32img_header &header, const bool eka1) {
        append32(buffer, static_cast<std::uint32_t>(header.uid1));
        append32(buffer, header.uid2);
        append32(buffer, header.uid3);
        append32(buffer, header.check);
        append32(buffer, header.sig);
        append32(buffer, eka1 ? 0x2000 : header.header_crc);
        append32(buffer, static_cast<std::uint32_t>(header.major) | (static_cast<std::uint32_t>(header.minor) << 16));
        append32(buffer, header.compression_type);
        append32(buffer, static_cast<std::uint32_t>(header.petran_major) | (static_cast<std::uint32_t>(header.petran_minor) << 8) | (static_cast<std::uint32_t>(header.petran_build) << 16));
        append32(buffer, header.msb);
        append32(buffer, header.lsb);
        append32(buffer, header.flags);
        append32(buffer, header.code_size);
        append32(buffer, header.data_size);
        append32(buffer, header.heap_size_min);
        append32(buffer, header.heap_size_max);
        append32(buffer, header.stack_size);
        append32(buffer, header.bss_size);
        append32(buffer, header.entry_point);
        append32(buffer, header.code_base);
        append32(buffer, header.data_base);
        append32(buffer, header.dll_ref_table_count);
        append32(buffer, header.export_dir_offset);
        append32(buffer, header.export_dir_count);
        append32(buffer, header.text_size);
        append32(buffer, header.code_offset);
        append32(buffer, header.data_offset);
        append32(buffer, header.import_offset);
        append32(buffer, header.code_reloc_offset);
        append32(buffer, header.data_reloc_offset);
        append32(buffer, static_cast<std::uint32_t>(header.priority) | (static_cast<std::uint32_t>(header.cpu) << 16));
    }

    void append_bytepair_page(std::vector<std::uint8_t> &buffer, const std::uint32_t decompressed_size,
        const std::vector<std::uint8_t> &page) {
        append32(buffer, static_cast<std::uint32_t>(page.size()));
        append32(buffer, decompressed_size);
        append16(buffer, 1);
        append16(buffer, static_cast<std::uint16_t>(page.size()));
        buffer.insert(buffer.end(), page.begin(), page.end());
    }

    std::vector<std::uint8_t> make_bytepair_image(const bool eka1) {
        eka2l1::loader::e32img_header header{};
        header.uid1 = eka2l1::loader::e32_img_type::exe;
        header.uid2 = 0x100039CE;
        header.uid3 = 0xA000017F;
        header.sig = 0x434F5045;
        header.major = 1;
        header.compression_type = 0x102822AA;
        header.flags = eka1 ? 0x01000000 : 0x01000028;
        header.code_size = static_cast<std::uint32_t>(ARM_CODE.size());
        header.code_base = 0x8000;
        header.data_base = 0x400000;
        header.text_size = header.code_size;
        header.code_offset = eka1 ? 124 : 128;
        header.data_offset = header.code_offset + header.code_size;
        header.import_offset = header.data_offset;
        header.cpu = eka1 ? eka2l1::loader::e32_cpu::armv4 : eka2l1::loader::e32_cpu::armv5;

        const std::uint32_t uids[] = { static_cast<std::uint32_t>(header.uid1), header.uid2, header.uid3 };
        header.check = eka2l1::crypt::calculate_checked_uid_checksum(uids);

        const std::vector<std::uint8_t> compressed_code = {
            0x01, 0xFE, 0x01, 0x00, 0x00,
            0x01, 0xA0, 0xE3, 0x1E, 0xFF, 0x2F, 0xE1
        };
        const std::vector<std::uint8_t> empty_import_section = { 0x00, 0x00, 0x00, 0x00, 0x00 };

        std::vector<std::uint8_t> buffer;
        append_header(buffer, header, eka1);
        append32(buffer, header.code_size + 4);
        append_bytepair_page(buffer, header.code_size, compressed_code);
        append_bytepair_page(buffer, 4, empty_import_section);
        return buffer;
    }
}

TEST_CASE("E32 images with missing dependencies fail without caching a partial codeseg", "[loader]") {
    loader_fixture fixture;
    auto &manager = fixture.manager;
    auto &kern = fixture.kern;

    for (const auto version : { epocver::epoc81a, epocver::epoc94 }) {
        loader::e32img image{};
        image.epoc_ver = version;
        image.header.uid1 = loader::e32_img_type::dll;
        image.header.code_size = 64;
        image.data.resize(64);
        loader::e32img_import_block dependency{};
        dependency.dll_name = "missing_dependency.dll";
        dependency.ordinals = { 1 };
        image.import_section.imports.push_back(dependency);

        for (int attempt = 0; attempt < 2; attempt++) {
            REQUIRE(manager.load_as_e32img(image, u"C:\\sys\\bin\\optional.dll") == nullptr);
            REQUIRE(kern.get_by_name<kernel::codeseg>("optional.dll") == nullptr);
        }

        image.import_section.imports.clear();
        auto loaded = manager.load_as_e32img(image, u"C:\\sys\\bin\\optional.dll");
        REQUIRE(loaded != nullptr);
        REQUIRE(kern.destroy(loaded));
    }
}

TEST_CASE("Failed cyclic DLL loads roll back without destroying their loading parent", "[loader]") {
    loader_fixture fixture;
    auto &manager = fixture.manager;
    auto &kern = fixture.kern;
    const char *root = "loader_missing_dependency/";
    common::create_directories(root);
    auto filesystem = create_physical_filesystem(epocver::epoc94, "");
    fixture.io.add_filesystem(filesystem);
    REQUIRE(fixture.io.mount_physical_path(drive_c, drive_media::physical, io_attrib_internal,
        u"loader_missing_dependency/"));
    manager.search_paths = { u"\\" };

    SECTION("A missing import after a completed cycle invalidates the entire cycle") {
        write_image("loader_missing_dependency/child.dll", { "parent.dll" });
        write_image("loader_missing_dependency/parent.dll", { "child.dll", "missing.dll" });
        auto parent = make_image({ "child.dll", "missing.dll" });
        REQUIRE(manager.load_as_e32img(parent, u"C:\\parent.dll") == nullptr);
        REQUIRE(kern.get_by_name<kernel::codeseg>("parent.dll") == nullptr);
        REQUIRE(kern.get_by_name<kernel::codeseg>("child.dll") == nullptr);
    }

    SECTION("A failing child can release a back reference to its loading parent") {
        write_image("loader_missing_dependency/child.dll", { "parent.dll", "missing.dll" });
        write_image("loader_missing_dependency/parent.dll", { "child.dll" });
        auto parent = make_image({ "child.dll" });
        REQUIRE(manager.load_as_e32img(parent, u"C:\\parent.dll") == nullptr);
        REQUIRE(kern.get_by_name<kernel::codeseg>("parent.dll") == nullptr);
        REQUIRE(kern.get_by_name<kernel::codeseg>("child.dll") == nullptr);
    }

    SECTION("A complete cycle remains loadable") {
        write_image("loader_missing_dependency/child.dll", { "parent.dll" });
        write_image("loader_missing_dependency/parent.dll", { "child.dll" });
        auto parent = make_image({ "child.dll" });
        REQUIRE(manager.load_as_e32img(parent, u"C:\\parent.dll") != nullptr);
        REQUIRE(kern.get_by_name<kernel::codeseg>("child.dll") != nullptr);
    }
    common::delete_folder(root);
}

TEST_CASE("E32Image byte-pair decompression preserves the code section", "[e32img]") {
    const bool formats[] = { false, true };

    for (const bool eka1 : formats) {
        INFO((eka1 ? "EKA1" : "EKA2"));
        std::vector<std::uint8_t> data = make_bytepair_image(eka1);
        eka2l1::common::ro_buf_stream stream(data.data(), data.size());
        auto image = eka2l1::loader::parse_e32img(&stream, false);

        REQUIRE(image);
        REQUIRE(image->epoc_ver == (eka1 ? epocver::epoc6 : epocver::epoc94));
        REQUIRE(image->header.code_base == 0x8000);
        REQUIRE(image->header.code_size == ARM_CODE.size());
        REQUIRE(image->data.size() == image->header.code_offset + image->header.code_size + 4);
        const auto *code = reinterpret_cast<const std::uint8_t *>(image->data.data() + image->header.code_offset);
        REQUIRE(std::equal(ARM_CODE.begin(), ARM_CODE.end(), code));
    }
}

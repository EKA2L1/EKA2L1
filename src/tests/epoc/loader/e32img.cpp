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
#include <common/crypt.h>
#include <common/fileutils.h>
#include <config/app_settings.h>
#include <config/config.h>
#include <cpu/arm_factory.h>
#include <kernel/codeseg.h>
#include <kernel/kernel.h>
#include <kernel/timing.h>

#include <cstring>
#include <fstream>

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

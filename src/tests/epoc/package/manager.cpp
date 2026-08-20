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

#include <catch2/catch.hpp>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/path.h>

#include <config/config.h>
#include <loader/e32img.h>
#include <loader/sis_fields.h>
#include <package/manager.h>
#include <vfs/vfs.h>

#include <cstdio>
#include <fstream>

using namespace eka2l1;

namespace {
    // assets/ifblock.pkg builds this SIS with the SDK's makesis. It installs
    // C:\eka2l1test\base.txt and the executable C:\eka2l1test\sample.dll from the
    // controller's own install block, and C:\eka2l1test\cond.txt from a conditional
    // whose condition holds (the else branch would install other.txt instead).
    static constexpr const char *IF_BLOCK_SIS_PATH = "packageassets//ifblock.sis";
    static constexpr manager::uid IF_BLOCK_PACKAGE_UID = 0xE1234567;

    // assets/ifblock_v2.pkg: same package UID, version 2, and only base.txt survives
    // from the first version's file list (new.txt joins it).
    static constexpr const char *IF_BLOCK_V2_SIS_PATH = "packageassets//ifblock_v2.sis";

    // assets/embedder.pkg installs C:\eka2l1test\host.txt and embeds embedded.pkg,
    // which installs C:\eka2l1test\guest.txt under its own UID.
    static constexpr const char *EMBEDDER_SIS_PATH = "packageassets//embedder.sis";
    static constexpr manager::uid EMBEDDER_PACKAGE_UID = 0xE1234570;
    static constexpr manager::uid EMBEDDED_PACKAGE_UID = 0xE1234571;

    // A package manager backed by a throwaway host folder mounted as drive C, with
    // a second one mounted read-only as drive Z to stand in for the ROM.
    struct package_test_env {
        std::string root;
        std::string rom_root;
        io_system io;
        config::state conf;
        std::unique_ptr<manager::packages> packages;

        explicit package_test_env(const std::string &name)
            // The uppercase letters are deliberate: on a case-sensitive host they
            // catch anything that lowercases a resolved host path rather than the
            // guest-relative half of it.
            : root(add_path("PackageTestEnv", name + eka2l1::get_separator()))
            , rom_root(add_path("PackageTestEnv", name + "_rom" + eka2l1::get_separator())) {
            common::delete_folder(root);
            common::delete_folder(rom_root);
            common::create_directories(root);
            common::create_directories(rom_root);

            file_system_inst physical_fs = create_physical_filesystem(epocver::epoc94, "");
            io.add_filesystem(physical_fs);
            io.mount_physical_path(drive_number::drive_c, drive_media::physical, io_attrib_internal,
                common::utf8_to_ucs2(root));
            io.mount_physical_path(drive_number::drive_z, drive_media::physical,
                io_attrib_internal | io_attrib_write_protected, common::utf8_to_ucs2(rom_root));

            conf.storage = root;
            packages = std::make_unique<manager::packages>(&io, &conf, drive_number::drive_c);
        }

        ~package_test_env() {
            packages.reset();
            common::delete_folder(root);
            common::delete_folder(rom_root);
        }

        bool install_sis(const char *path) {
            return packages->install_package(common::utf8_to_ucs2(path), drive_number::drive_c, nullptr, nullptr, true)
                == package::installation_result_success;
        }

        bool install_if_block_sis() {
            return install_sis(IF_BLOCK_SIS_PATH);
        }

        // Put a file where a package's guest data would live, to watch it go.
        void write_file(const std::u16string &path) {
            io.create_directories(eka2l1::file_directory(path));

            symfile file = io.open_file(path, WRITE_MODE | BIN_MODE);
            REQUIRE(file);

            const char content[] = "data";
            file->write_file(content, sizeof(content), 1);
            file->close();
        }

        // Drive Z takes no writes through the io system, so its content is laid
        // down on the host the way a ROM dump already carries it.
        void write_rom_file(const std::string &relative) {
            const std::string path = add_path(rom_root, relative);
            common::create_directories(eka2l1::file_directory(path));

            std::ofstream file(path, std::ios::binary);
            file << "data";
        }

        bool owns_file(package::object &pkg, const std::u16string &target) const {
            for (const package::file_description &desc : pkg.file_descriptions) {
                if (common::compare_ignore_case(desc.target, target) == 0) {
                    return true;
                }
            }

            return false;
        }

        // Operation the package recorded for a file it owns, -1 when it owns none.
        int file_operation(package::object &pkg, const std::u16string &target) const {
            for (const package::file_description &desc : pkg.file_descriptions) {
                if (common::compare_ignore_case(desc.target, target) == 0) {
                    return static_cast<int>(desc.operation);
                }
            }

            return -1;
        }

        // Secure ID the package recorded for a file it owns.
        epoc::uid file_secure_id(package::object &pkg, const std::u16string &target) const {
            for (const package::file_description &desc : pkg.file_descriptions) {
                if (common::compare_ignore_case(desc.target, target) == 0) {
                    return desc.sid;
                }
            }

            return 0;
        }

        // Secure ID as the E32 image itself states it, the value the installer
        // should have recorded.
        epoc::uid read_secure_id(const std::u16string &target) {
            symfile file = io.open_file(target, READ_MODE | BIN_MODE);
            if (!file) {
                return 0;
            }

            eka2l1::ro_file_stream stream(file.get());

            loader::e32img_header header;
            loader::e32img_header_extended extended_header;
            std::uint32_t uncompressed_size = 0;
            epocver version_used = epocver::eka1;

            extended_header.info.secure_id = 0;

            if (loader::parse_e32img_header(reinterpret_cast<common::ro_stream *>(&stream), header, extended_header,
                    uncompressed_size, version_used)
                != 0) {
                return 0;
            }

            return extended_header.info.secure_id;
        }
    };
}

TEST_CASE("conditional_block_files_are_owned_by_the_package", "package_manager") {
    package_test_env env("conditional_block_files");
    REQUIRE(env.install_if_block_sis());

    package::object *pkg = env.packages->package(IF_BLOCK_PACKAGE_UID, 0);
    REQUIRE(pkg != nullptr);

    // The file in the controller's own install block was always recorded...
    REQUIRE(env.owns_file(*pkg, u"C:\\eka2l1test\\base.txt"));
    // ...the one behind the conditional is the regression: without it, uninstall
    // leaves the file (an app's _reg.rsc, in the case that surfaced this) behind.
    REQUIRE(env.owns_file(*pkg, u"C:\\eka2l1test\\cond.txt"));
    // The branch that did not run installs nothing, so it owns nothing.
    REQUIRE_FALSE(env.owns_file(*pkg, u"C:\\eka2l1test\\other.txt"));
}

TEST_CASE("uninstall_deletes_conditional_block_files", "package_manager") {
    package_test_env env("uninstall_conditional_block");
    REQUIRE(env.install_if_block_sis());

    REQUIRE(env.io.exist(u"C:\\eka2l1test\\base.txt"));
    REQUIRE(env.io.exist(u"C:\\eka2l1test\\cond.txt"));

    package::object *pkg = env.packages->package(IF_BLOCK_PACKAGE_UID, 0);
    REQUIRE(pkg != nullptr);
    REQUIRE(env.packages->uninstall_package(*pkg));

    REQUIRE_FALSE(env.io.exist(u"C:\\eka2l1test\\base.txt"));
    REQUIRE_FALSE(env.io.exist(u"C:\\eka2l1test\\cond.txt"));
}

TEST_CASE("uninstall_deletes_files_with_an_undefined_operation", "package_manager") {
    package_test_env env("uninstall_undefined_operation");
    REQUIRE(env.install_if_block_sis());

    package::object *pkg = env.packages->package(IF_BLOCK_PACKAGE_UID, 0);
    REQUIRE(pkg != nullptr);

    // A language-dependent file carries no explicit operation, yet the installer
    // writes it like any other. Uninstall used to skip exactly these, leaving
    // Opera Mobile's UI resources behind.
    REQUIRE(env.file_operation(*pkg, u"C:\\eka2l1test\\lang.txt")
        == static_cast<int>(loader::ss_op::undefined));
    REQUIRE(env.io.exist(u"C:\\eka2l1test\\lang.txt"));

    REQUIRE(env.packages->uninstall_package(*pkg));
    REQUIRE_FALSE(env.io.exist(u"C:\\eka2l1test\\lang.txt"));
}

TEST_CASE("uninstall_removes_the_registry_entry", "package_manager") {
    package_test_env env("uninstall_registry_entry");
    REQUIRE(env.install_if_block_sis());
    REQUIRE(env.io.exist(u"C:\\sys\\install\\sisregistry\\e1234567\\00000000.reg"));

    package::object *pkg = env.packages->package(IF_BLOCK_PACKAGE_UID, 0);
    REQUIRE(pkg != nullptr);
    REQUIRE(env.packages->uninstall_package(*pkg));

    // A registry left on disk comes back as an installed package on the next
    // load_registries(), for files that are no longer there.
    REQUIRE_FALSE(env.io.exist(u"C:\\sys\\install\\sisregistry\\e1234567\\00000000.reg"));
    REQUIRE_FALSE(env.io.exist(u"C:\\sys\\install\\sisregistry\\e1234567"));
    REQUIRE(env.packages->package(IF_BLOCK_PACKAGE_UID, 0) == nullptr);
}

TEST_CASE("uninstall_keeps_the_registries_of_other_indices", "package_manager") {
    package_test_env env("uninstall_registry_sibling");
    REQUIRE(env.install_if_block_sis());

    // An augmentation keeps the UID's registry folder alive, so removing the base
    // install has to delete that install's own registry file on its own.
    package::object augmentation;
    augmentation.uid = IF_BLOCK_PACKAGE_UID;
    augmentation.install_type = package::install_type_augmentations;
    augmentation.package_name = u"EKA2L1 IfBlock Test Extra";
    augmentation.vendor_name = u"EKA2L1";

    REQUIRE(env.packages->add_package(augmentation, nullptr));

    const std::u16string base_registry = u"C:\\sys\\install\\sisregistry\\e1234567\\00000000.reg";
    const std::u16string augmentation_registry = u"C:\\sys\\install\\sisregistry\\e1234567\\00000001.reg";
    REQUIRE(env.io.exist(base_registry));
    REQUIRE(env.io.exist(augmentation_registry));

    package::object *base = env.packages->package(IF_BLOCK_PACKAGE_UID, 0);
    REQUIRE(base != nullptr);
    REQUIRE(env.packages->uninstall_package(*base));

    REQUIRE_FALSE(env.io.exist(base_registry));
    REQUIRE(env.io.exist(augmentation_registry));
}

TEST_CASE("installing_resolves_executable_secure_ids", "package_manager") {
    package_test_env env("executable_secure_ids");
    REQUIRE(env.install_if_block_sis());

    // The SID has to come from the extracted file: at registration time, when the
    // installer used to read it, the file is not on the drive yet.
    const epoc::uid sid = env.read_secure_id(u"C:\\eka2l1test\\sample.dll");
    REQUIRE(sid != 0);

    package::object *pkg = env.packages->package(IF_BLOCK_PACKAGE_UID, 0);
    REQUIRE(pkg != nullptr);
    REQUIRE(env.file_secure_id(*pkg, u"C:\\eka2l1test\\sample.dll") == sid);

    // Which is what makes a package findable by the app UID it installs.
    package::object *owner = env.packages->package_owning_executable(sid);
    REQUIRE(owner != nullptr);
    REQUIRE(owner->uid == IF_BLOCK_PACKAGE_UID);

    REQUIRE(env.packages->package_owning_executable(sid + 1) == nullptr);
    REQUIRE(env.packages->package_owning_executable(0) == nullptr);
    // A plain data file carries no SID to confuse the lookup with.
    REQUIRE(env.file_secure_id(*pkg, u"C:\\eka2l1test\\base.txt") == 0);
}

TEST_CASE("package_owning_file_looks_past_the_uid", "package_manager") {
    package_test_env env("package_owning_file");
    REQUIRE(env.install_if_block_sis());

    // What a frontend holding only an app UID would do. Symbian does not tie a
    // package's UID to the UID3 of the app it installs, so this legitimately
    // misses (Opera Mobile registers app 0x2002AA96 from package 0x2002AA97).
    REQUIRE(env.packages->package(IF_BLOCK_PACKAGE_UID - 1, 0) == nullptr);

    // Looking the file up instead finds the package, case-insensitively.
    package::object *owner = env.packages->package_owning_file(u"c:\\eka2l1test\\CoNd.txt");
    REQUIRE(owner != nullptr);
    REQUIRE(owner->uid == IF_BLOCK_PACKAGE_UID);

    REQUIRE(env.packages->package_owning_file(u"C:\\eka2l1test\\other.txt") == nullptr);
    REQUIRE(env.packages->package_owning_file(u"") == nullptr);
}

TEST_CASE("package_owning_file_accepts_another_directory", "package_manager") {
    package_test_env env("package_owning_file_directory");
    REQUIRE(env.install_if_block_sis());

    // The directory a caller knows a binary by is not the installed one: applist
    // rebuilds an app path as <drive>:\system\programs\<name>.exe (or
    // <drive>:\<name>.exe) no matter where the package put the file.
    for (const std::u16string &path : { std::u16string(u"C:\\base.txt"),
             std::u16string(u"C:\\system\\programs\\base.txt") }) {
        package::object *owner = env.packages->package_owning_file(path);
        REQUIRE(owner != nullptr);
        REQUIRE(owner->uid == IF_BLOCK_PACKAGE_UID);
    }

    // Only on the drive the package installed to, and only for a name it owns.
    REQUIRE(env.packages->package_owning_file(u"E:\\base.txt") == nullptr);
    REQUIRE(env.packages->package_owning_file(u"C:\\other.txt") == nullptr);
}

TEST_CASE("package_owning_file_refuses_an_ambiguous_name", "package_manager") {
    package_test_env env("package_owning_file_ambiguous");
    REQUIRE(env.install_if_block_sis());

    // A second package installing a file of the same name makes the name alone
    // meaningless, so the fallback has to decline rather than guess.
    package::object other;
    other.uid = IF_BLOCK_PACKAGE_UID + 1;
    other.install_type = package::install_type_normal_install;
    other.package_name = u"Namesake";
    other.vendor_name = u"EKA2L1";

    package::file_description desc;
    desc.target = u"C:\\somewhere\\else\\base.txt";
    other.file_descriptions.push_back(desc);

    REQUIRE(env.packages->add_package(other, nullptr));

    REQUIRE(env.packages->package_owning_file(u"C:\\system\\programs\\base.txt") == nullptr);
    // The exact path still resolves: it names one package on its own.
    package::object *owner = env.packages->package_owning_file(u"C:\\eka2l1test\\base.txt");
    REQUIRE(owner != nullptr);
    REQUIRE(owner->uid == IF_BLOCK_PACKAGE_UID);
}

TEST_CASE("uninstall_refuses_rom_and_non_removable_packages", "package_manager") {
    package_test_env env("uninstall_refuses");

    // What a stub SIS describing ROM content registers as. SWI's uninstall planner
    // leaves with KErrNotSupported on both, and it has to: the files are in ROM, and
    // the stub would register the package again on the next boot anyway.
    package::object in_rom;
    in_rom.uid = 0xE1234580;
    in_rom.install_type = package::install_type_normal_install;
    in_rom.package_name = u"EKA2L1 In ROM";
    in_rom.vendor_name = u"EKA2L1";
    in_rom.in_rom = 1;
    in_rom.is_removable = 0;
    REQUIRE(env.packages->add_package(in_rom, nullptr));

    package::object non_removable;
    non_removable.uid = 0xE1234581;
    non_removable.install_type = package::install_type_normal_install;
    non_removable.package_name = u"EKA2L1 Non Removable";
    non_removable.vendor_name = u"EKA2L1";
    non_removable.in_rom = 0;
    non_removable.is_removable = 0;
    REQUIRE(env.packages->add_package(non_removable, nullptr));

    package::object *rom_package = env.packages->package(0xE1234580, 0);
    REQUIRE(rom_package != nullptr);
    REQUIRE_FALSE(env.packages->uninstall_package(*rom_package));
    REQUIRE(env.packages->package(0xE1234580, 0) != nullptr);

    package::object *fixed_package = env.packages->package(0xE1234581, 0);
    REQUIRE(fixed_package != nullptr);
    REQUIRE_FALSE(env.packages->uninstall_package(*fixed_package));
    REQUIRE(env.packages->package(0xE1234581, 0) != nullptr);
}

TEST_CASE("a_registry_from_before_in_rom_was_set_still_uninstalls", "package_manager") {
    package_test_env env("legacy_registry_in_rom");
    REQUIRE(env.install_if_block_sis());

    package::object *pkg = env.packages->package(IF_BLOCK_PACKAGE_UID, 0);
    REQUIRE(pkg != nullptr);

    // The SIS v1 installer never set in_rom, so a registry written by an older
    // build carries whatever the stack held there. Anything but 1 has to read back
    // as "not in ROM", or a package installed by that build could never be removed
    // now that uninstall refuses ROM packages.
    pkg->in_rom = 0x2A;
    REQUIRE(env.packages->save_package(*pkg));

    manager::packages reloaded(&env.io, &env.conf, drive_number::drive_c);
    reloaded.load_registries();

    package::object *loaded = reloaded.package(IF_BLOCK_PACKAGE_UID, 0);
    REQUIRE(loaded != nullptr);
    REQUIRE(loaded->in_rom == 0);
    REQUIRE(reloaded.uninstall_package(*loaded));
}

TEST_CASE("uninstall_takes_embedded_packages_with_it", "package_manager") {
    package_test_env env("uninstall_embedded");
    REQUIRE(env.install_sis(EMBEDDER_SIS_PATH));

    REQUIRE(env.packages->package(EMBEDDER_PACKAGE_UID, 0) != nullptr);
    REQUIRE(env.packages->package(EMBEDDED_PACKAGE_UID, 0) != nullptr);
    REQUIRE(env.io.exist(u"C:\\eka2l1test\\host.txt"));
    REQUIRE(env.io.exist(u"C:\\eka2l1test\\guest.txt"));

    package::object *embedder = env.packages->package(EMBEDDER_PACKAGE_UID, 0);
    REQUIRE(env.packages->uninstall_package(*embedder));

    // The embedded package is not a package the user ever installed on its own, so
    // leaving it behind leaves an entry nothing can reach and files nothing owns.
    REQUIRE_FALSE(env.io.exist(u"C:\\eka2l1test\\host.txt"));
    REQUIRE_FALSE(env.io.exist(u"C:\\eka2l1test\\guest.txt"));
    REQUIRE(env.packages->package(EMBEDDER_PACKAGE_UID, 0) == nullptr);
    REQUIRE(env.packages->package(EMBEDDED_PACKAGE_UID, 0) == nullptr);
}

TEST_CASE("uninstall_removes_the_private_directory_of_an_executable", "package_manager") {
    package_test_env env("uninstall_private_dir");
    REQUIRE(env.install_if_block_sis());

    const epoc::uid sid = env.read_secure_id(u"C:\\eka2l1test\\sample.dll");
    REQUIRE(sid != 0);

    char private_path[64];
    std::snprintf(private_path, sizeof(private_path), "C:\\private\\%08x\\settings.dat", sid);
    const std::u16string private_file = common::utf8_to_ucs2(private_path);
    env.write_file(private_file);
    REQUIRE(env.io.exist(private_file));

    package::object *pkg = env.packages->package(IF_BLOCK_PACKAGE_UID, 0);
    REQUIRE(pkg != nullptr);
    REQUIRE(env.packages->uninstall_package(*pkg));

    REQUIRE_FALSE(env.io.exist(private_file));
}

TEST_CASE("upgrading_removes_files_the_new_version_dropped", "package_manager") {
    package_test_env env("upgrade_stale_files");
    REQUIRE(env.install_if_block_sis());
    REQUIRE(env.io.exist(u"C:\\eka2l1test\\base.txt"));
    REQUIRE(env.io.exist(u"C:\\eka2l1test\\cond.txt"));
    REQUIRE(env.io.exist(u"C:\\eka2l1test\\sample.dll"));

    REQUIRE(env.install_sis(IF_BLOCK_V2_SIS_PATH));

    package::object *pkg = env.packages->package(IF_BLOCK_PACKAGE_UID, 0);
    REQUIRE(pkg != nullptr);
    REQUIRE(pkg->version.major == 2);

    // Files the new version still installs stay; the ones it dropped go, instead of
    // lingering forever because only the registration was replaced.
    REQUIRE(env.io.exist(u"C:\\eka2l1test\\base.txt"));
    REQUIRE(env.io.exist(u"C:\\eka2l1test\\new.txt"));
    REQUIRE_FALSE(env.io.exist(u"C:\\eka2l1test\\cond.txt"));
    REQUIRE_FALSE(env.io.exist(u"C:\\eka2l1test\\sample.dll"));
    REQUIRE_FALSE(env.io.exist(u"C:\\eka2l1test\\lang.txt"));
}

TEST_CASE("invalid_target_paths_are_rejected", "package_manager") {
    REQUIRE(package::is_valid_target_path(u"C:\\eka2l1test\\base.txt"));
    REQUIRE(package::is_valid_target_path(u"Z:\\sys\\bin\\rom.exe"));

    REQUIRE_FALSE(package::is_valid_target_path(u""));
    REQUIRE_FALSE(package::is_valid_target_path(u"eka2l1test\\base.txt"));
    REQUIRE_FALSE(package::is_valid_target_path(u"C:base.txt"));
    REQUIRE_FALSE(package::is_valid_target_path(u"4:\\base.txt"));
    REQUIRE_FALSE(package::is_valid_target_path(u"C:\\eka2l1test\\..\\..\\sys\\bin\\victim.exe"));
    REQUIRE_FALSE(package::is_valid_target_path(u"C:\\\\eka2l1test\\base.txt"));
    // Executables are resolved by name system-wide; a non-ASCII one cannot be.
    REQUIRE_FALSE(package::is_valid_target_path(u"C:\\sys\\bin\\\u4e2d\u6587.exe"));
    REQUIRE(package::is_valid_target_path(u"C:\\resource\\apps\\\u4e2d\u6587.rsc"));
}

TEST_CASE("uninstall_leaves_rom_files_alone", "package_manager") {
    package_test_env env("uninstall_rom_file");
    REQUIRE(env.install_if_block_sis());

    env.write_rom_file("eka2l1test/rom.txt");
    REQUIRE(env.io.exist(u"Z:\\eka2l1test\\rom.txt"));

    package::object *pkg = env.packages->package(IF_BLOCK_PACKAGE_UID, 0);
    REQUIRE(pkg != nullptr);

    // A package that upgrades ROM content registers the ROM file as one of its
    // own. Removing the package does not remove that file: it is not the
    // package's, and the emulated Z drive maps straight onto the user's ROM dump.
    package::file_description rom_file;
    rom_file.target = u"Z:\\eka2l1test\\rom.txt";
    rom_file.operation = static_cast<std::uint32_t>(loader::ss_op::install);
    pkg->file_descriptions.push_back(rom_file);

    REQUIRE(env.packages->uninstall_package(*pkg));
    REQUIRE(env.io.exist(u"Z:\\eka2l1test\\rom.txt"));
}

TEST_CASE("uninstall_leaves_invalid_targets_alone", "package_manager") {
    package_test_env env("uninstall_invalid_target");
    REQUIRE(env.install_if_block_sis());

    // A registry entry naming a path outside what a package may own does not earn a
    // deletion just because it is written down.
    env.write_file(u"C:\\sys\\bin\\victim.exe");
    REQUIRE(env.io.exist(u"C:\\sys\\bin\\victim.exe"));

    package::object *pkg = env.packages->package(IF_BLOCK_PACKAGE_UID, 0);
    REQUIRE(pkg != nullptr);

    package::file_description escape;
    escape.target = u"C:\\eka2l1test\\..\\sys\\bin\\victim.exe";
    escape.operation = static_cast<std::uint32_t>(loader::ss_op::install);
    pkg->file_descriptions.push_back(escape);

    REQUIRE(env.packages->uninstall_package(*pkg));
    REQUIRE(env.io.exist(u"C:\\sys\\bin\\victim.exe"));
}

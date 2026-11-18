#include <catch2/catch.hpp>

#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <config/config.h>
#include <system/devices.h>
#include <system/installation/archive.h>
#include <system/installation/rpkg.h>
#include <vfs/vfs.h>

#include <fstream>

using namespace eka2l1;

namespace {
    struct firmware_fixture {
        const std::string root = "firmware_case_test/";
        config::state conf;

        firmware_fixture() {
            common::delete_folder(eka2l1::host_path{root});
            common::create_directories(eka2l1::host_path{root});
            conf.storage = eka2l1::host_path{root};
        }

        ~firmware_fixture() {
            common::delete_folder(eka2l1::host_path{root});
        }

        void check_installed(device_manager &devices) {
            REQUIRE(devices.total() == 1);
            REQUIRE(devices.get("CASE-1")->ver == epocver::epoc91);

            io_system io;
            auto fs = create_physical_filesystem(epocver::epoc91, "");
            io.add_filesystem(fs);
            REQUIRE(io.mount_physical_path(drive_a, drive_media::physical, io_attrib_internal,
                eka2l1::host_path{add_path(root, "drives/z/case-1/")}));
            auto dir = io.open_dir(eka2l1::guest_path{u"A:\\private\\12345678\\*.MID"}, {}, io_attrib_include_file);
            REQUIRE(dir);
            const auto entry = dir->get_next_entry();
            REQUIRE(entry);
            REQUIRE(entry->name.utf16() == u"Intro_Groove 01.mid");
            REQUIRE_FALSE(dir->get_next_entry());
            REQUIRE(io.open_file(eka2l1::guest_path{u"A:\\PRIVATE\\12345678\\intro_groove 01.MID"}, READ_MODE));
        }
    };
}

TEST_CASE("rpkg_installation_preserves_firmware_names", "[installation]") {
    firmware_fixture fixture;
    const std::string path = add_path(fixture.root, "case.rpkg");
    {
        std::ofstream file(path, std::ios::binary);
        auto write = [&](auto value) {
            file.write(reinterpret_cast<const char *>(&value), sizeof(value));
        };
        for (const std::uint32_t magic : { 'R', 'P', 'K', '2' }) {
            write(magic);
        }
        write(std::uint8_t(9));
        write(std::uint8_t(1));
        write(std::uint16_t(0));
        write(std::uint32_t(3));
        write(std::uint32_t(32));
        write(std::uint32_t(0));
        auto entry = [&](const std::u16string &name, const std::string &data) {
            write(std::uint64_t(0));
            write(std::uint64_t(0));
            write(std::uint64_t(name.size()));
            file.write(reinterpret_cast<const char *>(name.data()), name.size() * sizeof(char16_t));
            write(std::uint64_t(data.size()));
            file.write(data.data(), data.size());
        };
        entry(u"Z:\\Resource\\Versions\\Platform.txt", "SymbianOSMajorVersion=9\nSymbianOSMinorVersion=1\n");
        entry(u"Z:\\resource\\versions\\Product.txt", "Manufacturer=Test\nProduct=CASE-1\nModel=Case fixture\n");
        entry(u"Z:\\Private\\12345678\\Intro_Groove 01.mid", "sample");
    }
    device_manager devices(&fixture.conf);
    std::string code;
    REQUIRE(loader::install_rpkg(&devices, eka2l1::host_path{path}, eka2l1::host_path{add_path(fixture.root, "drives/z/")}, code,
        false, nullptr, nullptr) == device_installation_none);
    REQUIRE(code == "case-1");
    fixture.check_installed(devices);
}

TEST_CASE("archive_installation_preserves_firmware_names", "[installation]") {
    firmware_fixture fixture;
    device_manager devices(&fixture.conf);
    REQUIRE(loader::install_archive(&devices, eka2l1::host_path{"systemassets/case_device.zip"},
        eka2l1::host_path{add_path(fixture.root, "roms/")}, eka2l1::host_path{add_path(fixture.root, "drives/z/")}, false,
        nullptr, nullptr) == device_installation_none);
    fixture.check_installed(devices);
}

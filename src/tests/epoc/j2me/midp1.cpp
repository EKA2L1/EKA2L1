/*
 * Copyright (c) 2026 EKA2L1 Team.
 * This file is part of EKA2L1, licensed under GPL version 3 or later.
 */

#include <catch2/catch.hpp>
#include <common/archive.h>
#include <common/fileutils.h>
#include <j2me/midp1.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
    const std::string DISPLAYABLE = "javax/microedition/lcdui/Displayable";
    const std::string CANVAS = "javax/microedition/lcdui/Canvas";

    // Builds a class file constant pool by constant pool, remembering where each constant starts.
    class class_writer {
        std::vector<char> data_;
        std::vector<std::size_t> offsets_;

        void u8(const std::uint8_t value) {
            data_.push_back(static_cast<char>(value));
        }

        void u16(const std::uint16_t value) {
            u8(static_cast<std::uint8_t>(value >> 8));
            u8(static_cast<std::uint8_t>(value & 0xFF));
        }

        std::uint16_t begin(const std::uint8_t tag) {
            offsets_.push_back(data_.size());
            u8(tag);
            return static_cast<std::uint16_t>(offsets_.size() - 1);
        }

    public:
        class_writer() {
            const std::uint8_t header[] = { 0xCA, 0xFE, 0xBA, 0xBE, 0, 3, 0, 45, 0, 0 };
            data_.assign(header, header + sizeof(header));
            offsets_.push_back(0);
        }

        std::uint16_t utf8(const std::string &text) {
            const std::uint16_t index = begin(1);
            u16(static_cast<std::uint16_t>(text.size()));
            data_.insert(data_.end(), text.begin(), text.end());
            return index;
        }

        std::uint16_t class_of(const std::string &name) {
            const std::uint16_t name_index = utf8(name);
            const std::uint16_t index = begin(7);
            u16(name_index);
            return index;
        }

        std::uint16_t method(const std::uint16_t owner, const std::string &name, const std::string &descriptor) {
            const std::uint16_t name_index = utf8(name);
            const std::uint16_t descriptor_index = utf8(descriptor);
            const std::uint16_t name_and_type = begin(12);
            u16(name_index);
            u16(descriptor_index);

            const std::uint16_t index = begin(10);
            u16(owner);
            u16(name_and_type);
            return index;
        }

        void long_constant() {
            begin(5);
            for (int i = 0; i < 8; i++) {
                u8(0x11);
            }

            // A long takes two slots.
            offsets_.push_back(0);
        }

        std::size_t offset(const std::uint16_t index) const {
            return offsets_[index];
        }

        // Ends the pool with the rest of a class file, which only needs to come through unchanged.
        std::vector<char> finish() {
            std::vector<char> result = data_;
            const std::uint16_t count = static_cast<std::uint16_t>(offsets_.size());
            result[8] = static_cast<char>(count >> 8);
            result[9] = static_cast<char>(count & 0xFF);

            const char rest[] = { 0x00, 0x21, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
            result.insert(result.end(), rest, rest + sizeof(rest));
            return result;
        }
    };

    std::uint16_t read_u16(const std::vector<char> &data, const std::size_t pos) {
        return static_cast<std::uint16_t>((static_cast<std::uint8_t>(data[pos]) << 8)
            | static_cast<std::uint8_t>(data[pos + 1]));
    }

    std::vector<char> read_file(const std::string &path) {
        std::ifstream file(path, std::ios::binary);
        return std::vector<char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    struct jar_fixture {
        const std::string dir = "j2me_midp1_test/";

        jar_fixture() {
            eka2l1::common::delete_folder(dir);
            eka2l1::common::create_directories(dir);
        }

        ~jar_fixture() {
            eka2l1::common::delete_folder(dir);
        }
    };

    eka2l1::common::archive_file make_file(const std::string &path, const std::vector<char> &content) {
        eka2l1::common::archive_file file;
        file.info.path = path;
        file.info.size = content.size();
        file.info.is_directory = false;
        file.content = content;
        return file;
    }
}

TEST_CASE("MIDP 2.0 calls to Displayable.getWidth and getHeight go to Canvas", "[j2me]") {
    class_writer writer;
    const std::uint16_t displayable = writer.class_of(DISPLAYABLE);
    const std::uint16_t get_width = writer.method(displayable, "getWidth", "()I");
    writer.long_constant();
    const std::uint16_t get_height = writer.method(displayable, "getHeight", "()I");
    const std::uint16_t get_title = writer.method(displayable, "getTitle", "()Ljava/lang/String;");

    const std::vector<char> original = writer.finish();
    const std::uint16_t count = read_u16(original, 8);
    std::vector<char> patched = original;

    REQUIRE(eka2l1::j2me::redirect_midp2_size_calls(patched) == 2);

    // Canvas is added at the end of the pool, its name first.
    REQUIRE(read_u16(patched, 8) == count + 2);
    const std::uint16_t canvas = count + 1;

    REQUIRE(read_u16(patched, writer.offset(get_width) + 1) == canvas);
    REQUIRE(read_u16(patched, writer.offset(get_height) + 1) == canvas);
    REQUIRE(read_u16(patched, writer.offset(get_title) + 1) == displayable);

    const std::size_t pool_end = original.size() - 14;
    REQUIRE(patched.size() == original.size() + 3 + CANVAS.size() + 3);
    REQUIRE(patched[pool_end] == 1);
    REQUIRE(read_u16(patched, pool_end + 1) == CANVAS.size());
    REQUIRE(std::string(&patched[pool_end + 3], CANVAS.size()) == CANVAS);
    REQUIRE(patched[pool_end + 3 + CANVAS.size()] == 7);
    REQUIRE(read_u16(patched, pool_end + 4 + CANVAS.size()) == count);

    // What follows the pool is the same.
    REQUIRE(std::equal(original.begin() + pool_end, original.end(), patched.end() - 14));
    REQUIRE(std::equal(original.begin(), original.begin() + 8, patched.begin()));
}

TEST_CASE("A Canvas class already in the pool is reused", "[j2me]") {
    class_writer writer;
    const std::uint16_t canvas = writer.class_of(CANVAS);
    const std::uint16_t displayable = writer.class_of(DISPLAYABLE);
    const std::uint16_t get_height = writer.method(displayable, "getHeight", "()I");

    const std::vector<char> original = writer.finish();
    std::vector<char> patched = original;

    REQUIRE(eka2l1::j2me::redirect_midp2_size_calls(patched) == 1);
    REQUIRE(patched.size() == original.size());
    REQUIRE(read_u16(patched, 8) == read_u16(original, 8));
    REQUIRE(read_u16(patched, writer.offset(get_height) + 1) == canvas);
}

TEST_CASE("Classes that do not call the MIDP 2.0 methods are left alone", "[j2me]") {
    class_writer writer;
    const std::uint16_t canvas = writer.class_of(CANVAS);
    writer.method(canvas, "getWidth", "()I");
    const std::uint16_t displayable = writer.class_of(DISPLAYABLE);
    writer.method(displayable, "getWidth", "(I)I");

    const std::vector<char> original = writer.finish();
    std::vector<char> unchanged = original;
    REQUIRE(eka2l1::j2me::redirect_midp2_size_calls(unchanged) == 0);
    REQUIRE(unchanged == original);

    // Cut in the middle of the pool, or not a class at all.
    std::vector<char> truncated(original.begin(), original.begin() + 20);
    const std::vector<char> truncated_copy = truncated;
    REQUIRE(eka2l1::j2me::redirect_midp2_size_calls(truncated) == 0);
    REQUIRE(truncated == truncated_copy);

    std::vector<char> text = { 'n', 'o', 't', ' ', 'a', ' ', 'c', 'l', 'a', 's', 's', '!' };
    REQUIRE(eka2l1::j2me::redirect_midp2_size_calls(text) == 0);
}

TEST_CASE("Installing a JAR for MIDP 1.0 patches its classes and keeps the rest", "[j2me]") {
    jar_fixture fixture;

    class_writer writer;
    const std::uint16_t displayable = writer.class_of(DISPLAYABLE);
    const std::uint16_t get_width = writer.method(displayable, "getWidth", "()I");
    const std::vector<char> game_class = writer.finish();

    const std::string manifest_text = "Manifest-Version: 1.0\nMicroEdition-Profile: MIDP-1.0\n";
    const std::vector<char> manifest(manifest_text.begin(), manifest_text.end());
    const std::vector<char> image(300, 0x42);

    eka2l1::common::archive_file folder;
    folder.info.path = "META-INF/";
    folder.info.size = 0;
    folder.info.is_directory = true;

    const std::vector<eka2l1::common::archive_file> files = {
        folder,
        make_file("META-INF/MANIFEST.MF", manifest),
        make_file("i.class", game_class),
        make_file("splash.png", image)
    };

    const std::string source = fixture.dir + "game.jar";
    const std::string dest = fixture.dir + "installed.jar";
    REQUIRE(eka2l1::common::write_zip_archive(source, files));
    REQUIRE(eka2l1::j2me::copy_jar_for_midp1(source, dest));

    std::vector<eka2l1::common::archive_file> installed;
    REQUIRE(eka2l1::common::read_archive(dest, installed));
    REQUIRE(installed.size() == 4);

    REQUIRE(installed[0].info.is_directory);
    REQUIRE(installed[0].info.path == "META-INF/");
    REQUIRE(installed[1].info.path == "META-INF/MANIFEST.MF");
    REQUIRE(installed[1].content == manifest);
    REQUIRE(installed[3].info.path == "splash.png");
    REQUIRE(installed[3].content == image);

    REQUIRE(installed[2].info.path == "i.class");
    REQUIRE(installed[2].content.size() == game_class.size() + 3 + CANVAS.size() + 3);
    REQUIRE(read_u16(installed[2].content, writer.offset(get_width) + 1) == read_u16(game_class, 8) + 1);

    // The zip ends with its end of central directory record, without padding.
    const std::vector<char> raw = read_file(dest);
    REQUIRE(raw.size() > 22);
    REQUIRE(std::memcmp(&raw[raw.size() - 22], "PK\x05\x06", 4) == 0);
}

TEST_CASE("A JAR with nothing to patch is installed byte for byte", "[j2me]") {
    jar_fixture fixture;

    class_writer writer;
    const std::uint16_t canvas = writer.class_of(CANVAS);
    writer.method(canvas, "getWidth", "()I");

    const std::string source = fixture.dir + "game.jar";
    const std::string dest = fixture.dir + "installed.jar";
    REQUIRE(eka2l1::common::write_zip_archive(source, { make_file("a.class", writer.finish()) }));
    REQUIRE(eka2l1::j2me::copy_jar_for_midp1(source, dest));

    REQUIRE(read_file(dest) == read_file(source));
}

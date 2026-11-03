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

#include <j2me/midp1.h>

#include <common/archive.h>
#include <common/fileutils.h>
#include <common/log.h>

#include <cstdint>
#include <cstring>

namespace eka2l1::j2me {
    namespace {
        constexpr const char *DISPLAYABLE_CLASS = "javax/microedition/lcdui/Displayable";
        constexpr const char *CANVAS_CLASS = "javax/microedition/lcdui/Canvas";

        // JARs of the phones this VM ran on are a few hundred kilobytes; this only guards the allocation.
        constexpr std::uint64_t MAX_JAR_CONTENT_SIZE = 32 * 1024 * 1024;

        enum constant_tag : std::uint8_t {
            CONSTANT_UTF8 = 1,
            CONSTANT_INTEGER = 3,
            CONSTANT_FLOAT = 4,
            CONSTANT_LONG = 5,
            CONSTANT_DOUBLE = 6,
            CONSTANT_CLASS = 7,
            CONSTANT_STRING = 8,
            CONSTANT_FIELDREF = 9,
            CONSTANT_METHODREF = 10,
            CONSTANT_INTERFACE_METHODREF = 11,
            CONSTANT_NAME_AND_TYPE = 12
        };

        struct constant {
            std::uint8_t tag = 0;

            // Where the tag byte is in the class file.
            std::size_t offset = 0;

            // The two indices of a reference or a name and type, the one index of a class, or the
            // byte length of a UTF-8 string.
            std::uint16_t first = 0;
            std::uint16_t second = 0;
        };

        std::uint16_t read_u16(const std::vector<char> &data, const std::size_t pos) {
            return static_cast<std::uint16_t>((static_cast<std::uint8_t>(data[pos]) << 8)
                | static_cast<std::uint8_t>(data[pos + 1]));
        }

        void write_u16(std::vector<char> &data, const std::size_t pos, const std::uint16_t value) {
            data[pos] = static_cast<char>(value >> 8);
            data[pos + 1] = static_cast<char>(value & 0xFF);
        }

        void append_u16(std::vector<char> &data, const std::uint16_t value) {
            data.push_back(static_cast<char>(value >> 8));
            data.push_back(static_cast<char>(value & 0xFF));
        }

        bool ends_with_class(const std::string &path) {
            static constexpr const char EXTENSION[] = ".class";
            static constexpr std::size_t EXTENSION_LENGTH = sizeof(EXTENSION) - 1;

            return (path.size() > EXTENSION_LENGTH)
                && (path.compare(path.size() - EXTENSION_LENGTH, EXTENSION_LENGTH, EXTENSION) == 0);
        }
    }

    std::size_t redirect_midp2_size_calls(std::vector<char> &class_file) {
        // magic, minor and major version, then the constant pool count.
        if ((class_file.size() < 10) || (read_u16(class_file, 0) != 0xCAFE) || (read_u16(class_file, 2) != 0xBABE)) {
            return 0;
        }

        const std::uint16_t count = read_u16(class_file, 8);
        std::vector<constant> pool(count);
        std::size_t pos = 10;

        for (std::uint32_t i = 1; i < count; i++) {
            if (pos >= class_file.size()) {
                return 0;
            }

            constant &entry = pool[i];
            entry.tag = static_cast<std::uint8_t>(class_file[pos]);
            entry.offset = pos;

            std::size_t length = 0;

            switch (entry.tag) {
            case CONSTANT_UTF8:
                if (pos + 3 > class_file.size()) {
                    return 0;
                }

                length = 3 + read_u16(class_file, pos + 1);
                break;

            case CONSTANT_CLASS:
            case CONSTANT_STRING:
                length = 3;
                break;

            case CONSTANT_INTEGER:
            case CONSTANT_FLOAT:
            case CONSTANT_FIELDREF:
            case CONSTANT_METHODREF:
            case CONSTANT_INTERFACE_METHODREF:
            case CONSTANT_NAME_AND_TYPE:
                length = 5;
                break;

            case CONSTANT_LONG:
            case CONSTANT_DOUBLE:
                // These take two slots of the pool.
                length = 9;
                i++;
                break;

            default:
                // Not a constant MIDP 1.0 class files have.
                return 0;
            }

            if (pos + length > class_file.size()) {
                return 0;
            }

            if (length >= 3) {
                entry.first = read_u16(class_file, pos + 1);
            }

            if (length >= 5) {
                entry.second = read_u16(class_file, pos + 3);
            }

            pos += length;
        }

        const std::size_t pool_end = pos;

        auto is_utf8 = [&](const std::uint16_t index, const char *text) {
            if ((index == 0) || (index >= count) || (pool[index].tag != CONSTANT_UTF8)) {
                return false;
            }

            const std::size_t length = std::strlen(text);
            return (pool[index].first == length)
                && (std::memcmp(&class_file[pool[index].offset + 3], text, length) == 0);
        };

        auto is_class = [&](const std::uint16_t index, const char *name) {
            return (index != 0) && (index < count) && (pool[index].tag == CONSTANT_CLASS)
                && is_utf8(pool[index].first, name);
        };

        std::vector<std::uint16_t> calls;
        std::uint16_t canvas = 0;

        for (std::uint32_t i = 1; i < count; i++) {
            const constant &entry = pool[i];

            if (is_class(static_cast<std::uint16_t>(i), CANVAS_CLASS)) {
                canvas = static_cast<std::uint16_t>(i);
                continue;
            }

            if ((entry.tag != CONSTANT_METHODREF) || !is_class(entry.first, DISPLAYABLE_CLASS)
                || (entry.second == 0) || (entry.second >= count) || (pool[entry.second].tag != CONSTANT_NAME_AND_TYPE)) {
                continue;
            }

            const constant &name_and_type = pool[entry.second];

            if ((is_utf8(name_and_type.first, "getWidth") || is_utf8(name_and_type.first, "getHeight"))
                && is_utf8(name_and_type.second, "()I")) {
                calls.push_back(static_cast<std::uint16_t>(i));
            }
        }

        if (calls.empty()) {
            return 0;
        }

        if (canvas == 0) {
            // No Canvas class constant to point at yet: add its name and the class at the end of the pool.
            if (count > 0xFFFF - 2) {
                return 0;
            }

            const std::size_t name_length = std::strlen(CANVAS_CLASS);
            std::vector<char> added;

            added.push_back(static_cast<char>(CONSTANT_UTF8));
            append_u16(added, static_cast<std::uint16_t>(name_length));
            added.insert(added.end(), CANVAS_CLASS, CANVAS_CLASS + name_length);
            added.push_back(static_cast<char>(CONSTANT_CLASS));
            append_u16(added, count);

            class_file.insert(class_file.begin() + pool_end, added.begin(), added.end());
            write_u16(class_file, 8, static_cast<std::uint16_t>(count + 2));

            canvas = static_cast<std::uint16_t>(count + 1);
        }

        // The method references sit before the end of the pool, so they have not moved.
        for (const std::uint16_t call : calls) {
            write_u16(class_file, pool[call].offset + 1, canvas);
        }

        return calls.size();
    }

    bool copy_jar_for_midp1(const std::string &source, const std::string &dest) {
        std::vector<common::archive_file> files;
        std::size_t redirected = 0;

        if (common::read_archive(source, files, MAX_JAR_CONTENT_SIZE)) {
            for (common::archive_file &file : files) {
                if (!file.info.is_directory && ends_with_class(file.info.path)) {
                    redirected += redirect_midp2_size_calls(file.content);
                }
            }
        }

        if (redirected == 0) {
            return common::copy_file(source, dest, true);
        }

        LOG_INFO(J2ME, "{} calls to Displayable.getWidth/getHeight, which MIDP 1.0 only has on Canvas, now go to Canvas",
            redirected);

        return common::write_zip_archive(dest, files);
    }
}

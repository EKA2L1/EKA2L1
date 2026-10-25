/*
 * Copyright (c) 2026 EKA2L1 Team.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch.hpp>
#include <services/fs/fs.h>
#include <utils/err.h>

TEST_CASE("RFs name validation accepts filenames and complete paths", "[fs_name]") {
    // RFs::IsValidName cases from kerneltest/f32test/server/t_misc.cpp.
    for (const auto name : { u"abc", u".ext", u"as(){}@~#;!\u00a3$%^&()df.blarg", u"'",
             u"C:\\asdf.blarg\\asdf.blarg", u"E:\\Private\\2002A08C\\library\\bg\\loading.png",
             u"\\images\\loading.png", u"C:\\Hello World\\group\\release.txt", u"image.png ", u"\u0085" }) {
        CAPTURE(name);
        char16_t bad_character = u'?';
        REQUIRE(eka2l1::validate_file_name(name, u"C:\\data\\", bad_character) == eka2l1::epoc::error_none);
        REQUIRE(bad_character == 0);
    }
}

TEST_CASE("RFs name validation reports bad names through the completion status", "[fs_name]") {
    for (const auto name : { u"*", u"?", u">", u"<", u":", u"\"", u"/", u"|", u"\\", u"",
             u"C:\\asdf.blarg\\", u"C:\\..\\group\\release.txt", u"C:\\.\\group\\release.txt",
             u"C:\\\\group\\release.txt", u"C:\\ \\group\\release.txt", u"C:\\v123456..",
             u"E:\\My Videos\\Downloads\\1\\1\\Name.3gp?", u"xx:yy", u"xx/yy", u"images\\file.png", u"C: images\\file.png",
             u"\t", u"\u00a0", u"\u3000" }) {
        CAPTURE(name);
        char16_t bad_character = 0;
        REQUIRE(eka2l1::validate_file_name(name, u"C:\\data\\", bad_character) == eka2l1::epoc::error_bad_name);
    }

    char16_t bad_character = 0;
    REQUIRE(eka2l1::validate_file_name(u"C:\\bad?name.png", u"C:\\", bad_character) == eka2l1::epoc::error_bad_name);
    REQUIRE(bad_character == u'?');
    REQUIRE(eka2l1::validate_file_name(u"C:\\..\\image.png", u"C:\\", bad_character) == eka2l1::epoc::error_bad_name);
    REQUIRE(bad_character == u'.');
}

TEST_CASE("RFs name validation rejects binary image data and overlong paths", "[fs_name]") {
    char16_t bad_character = 0;
    const std::u16string png_data = { u'I', u'H', u'D', u'R', 0, 0, 0, 128 };
    REQUIRE(eka2l1::validate_file_name(png_data, u"C:\\", bad_character) == eka2l1::epoc::error_bad_name);
    REQUIRE(bad_character == 0);
    REQUIRE(eka2l1::validate_file_name(std::u16string(257, u'a'), u"C:\\", bad_character) == eka2l1::epoc::error_bad_name);
    REQUIRE(eka2l1::validate_file_name(std::u16string(253, u'a'), u"C:\\", bad_character) == eka2l1::epoc::error_none);
    REQUIRE(eka2l1::validate_file_name(std::u16string(254, u'a'), u"C:\\", bad_character) == eka2l1::epoc::error_bad_name);
}

TEST_CASE("RFs detailed name validation returns SDK error codes and positions", "[fs_name]") {
    using eka2l1::file_name_error;
    struct invalid_name {
        const char16_t *name;
        file_name_error error;
        std::uint32_t position;
    };
    // RFs::TNameValidParam cases from kerneltest/f32test/server/t_misc.cpp.
    for (const auto use_session : { 0, 1 }) {
        for (const auto &entry : {
                 invalid_name{ u"C:\\?\\group\\release.txt", file_name_error::bad_character, 4 },
                 invalid_name{ u"C:\\..\\group\\release.txt", file_name_error::bad_character, 4 },
                 invalid_name{ u"C::\\group\\release.txt", file_name_error::bad_character, 3 },
                 invalid_name{ u"xx/yy", file_name_error::bad_character, 3 },
                 invalid_name{ u">\\group\\release.txt", file_name_error::bad_character, 1 },
                 invalid_name{ u"C:\\>>\\group\\release.txt", file_name_error::bad_character, 5 },
                 invalid_name{ u"C:\\\\\\group\\release.txt", file_name_error::bad_character, 11 },
                 invalid_name{ u"abc>def|ghi?jkl:mno<pqr*stu", file_name_error::bad_character, 24 },
                 invalid_name{ u"abc>def|ghi<jkl:mno", file_name_error::bad_character, 16 },
                 invalid_name{ u"C:\\v123456..", file_name_error::bad_character, 11 },
                 invalid_name{ u"C:\\ \\group\\release.txt", file_name_error::bad_name, 0 },
                 invalid_name{ u"file1.txt\\\\", file_name_error::bad_name, 0 },
                 invalid_name{ u"", file_name_error::bad_name, 0 } }) {
            CAPTURE(entry.name, use_session);
            eka2l1::file_name_validation_params params{ file_name_error::none, use_session, 99 };
            char16_t bad_character = 0;
            REQUIRE(eka2l1::validate_file_name(entry.name, u"C:\\data\\", bad_character, &params) == eka2l1::epoc::error_bad_name);
            REQUIRE(params.error_ == entry.error);
            REQUIRE(params.invalid_char_pos_ == entry.position);
            REQUIRE(params.use_session_path_ == use_session);
        }
        for (const auto name : { u"\\", u"C:\\asdf.blarg\\", u"C:\\asdf..blarg\\", u".ext", u"C:\\file.png" }) {
            eka2l1::file_name_validation_params params{ file_name_error::bad_name, use_session, 99 };
            char16_t bad_character = u'?';
            REQUIRE(eka2l1::validate_file_name(name, u"C:\\data\\", bad_character, &params) == eka2l1::epoc::error_none);
            REQUIRE(params.error_ == file_name_error::none);
            REQUIRE(params.invalid_char_pos_ == 0);
            REQUIRE(bad_character == 0);
        }
    }
}

TEST_CASE("RFs detailed name validation applies the requested session path policy", "[fs_name]") {
    using eka2l1::file_name_error;
    char16_t bad_character = 0;
    eka2l1::file_name_validation_params params{ file_name_error::none, 0, 0 };
    const std::u16string long_name(250, u'a');
    REQUIRE(eka2l1::validate_file_name(long_name, u"C:\\data\\", bad_character, &params) == eka2l1::epoc::error_none);
    params.use_session_path_ = 1;
    REQUIRE(eka2l1::validate_file_name(long_name, u"C:\\data\\", bad_character, &params) == eka2l1::epoc::error_bad_name);
    REQUIRE(params.error_ == file_name_error::too_long);
    REQUIRE(params.invalid_char_pos_ == 0);
}

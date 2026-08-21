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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>

#include <common/buffer.h>
#include <services/applist/applist.h>

#include <string>
#include <vector>

using namespace eka2l1;

namespace {
    data_recog_result recognize(std::vector<std::uint8_t> data, const std::u16string &name) {
        common::ro_buf_stream stream(data.empty() ? nullptr : data.data(), data.size());
        return applist_server::recognize_data_impl(stream, name);
    }

    std::string type_of(data_recog_result result) {
        return result.type_.type_name_.to_std_string(nullptr);
    }

    std::vector<std::uint8_t> bytes(const std::string &content) {
        return std::vector<std::uint8_t>(content.begin(), content.end());
    }
}

TEST_CASE("recognizer_reports_apparc_confidence_values", "applist") {
    // TRecognitionConfidence (apmrec.h). A client compares these numerically, and
    // EPossible is zero rather than the middle of the scale, so the fallback below
    // has to sit above it or it reads as "nothing recognised this".
    REQUIRE(data_recognition_confidence_certain == 0x7FFFFFFF);
    REQUIRE(data_recognition_confidence_probable == 100);
    REQUIRE(data_recognition_confidence_possible == 0);
    REQUIRE(data_recognition_confidence_unlikely == -100);

    data_recog_result unknown = recognize(bytes("nothing in particular"), u"c:\\data\\thing.bin");
    REQUIRE(type_of(unknown) == "application/octet-stream");
    REQUIRE(unknown.confidence_rating_ > data_recognition_confidence_possible);
}

TEST_CASE("recognizer_reads_the_name_for_web_content", "applist") {
    // S60's web recognizer reports XHTML as text/html; it has no separate
    // application/xhtml+xml type. The Store's local front page is an .xhtml file
    // whose bytes say nothing, so the name is the only thing to go on.
    const std::vector<std::uint8_t> page = bytes("<!DOCTYPE html><html></html>");

    for (const std::u16string &name : { std::u16string(u"e:\\showroom\\front.xhtml"),
             std::u16string(u"e:\\showroom\\front.html"), std::u16string(u"e:\\a.HTM"),
             std::u16string(u"e:\\a.shtml") }) {
        data_recog_result result = recognize(page, name);
        REQUIRE(type_of(result) == "text/html");
        REQUIRE(result.confidence_rating_ == data_recognition_confidence_probable);
    }

    data_recog_result xml = recognize(page, u"e:\\feed.xml");
    REQUIRE(type_of(xml) == "text/xml");
}

TEST_CASE("recognizer_reads_the_magic_for_media", "applist") {
    // RIFF, a size, then the WAVE form type at bytes 8 to 11.
    std::vector<std::uint8_t> wave = { 'R', 'I', 'F', 'F', 0x24, 0x08, 0, 0, 'W', 'A', 'V', 'E' };
    data_recog_result wav = recognize(wave, u"c:\\sound.dat");
    REQUIRE(type_of(wav) == "audio/wav");
    REQUIRE(wav.confidence_rating_ == data_recognition_confidence_certain);

    // An ID3 tag, and a bare MPEG frame header.
    REQUIRE(type_of(recognize({ 'I', 'D', '3', 0x03, 0, 0, 0, 0, 0, 0, 0, 0 }, u"c:\\a.dat")) == "audio/mpeg");
    REQUIRE(type_of(recognize({ 0xFF, 0xFB, 0x90, 0x00, 0, 0, 0, 0, 0, 0, 0, 0 }, u"c:\\a.dat")) == "audio/mpeg");

    // The three Flash signatures: uncompressed, zlib and LZMA.
    for (const char first : { 'F', 'C', 'Z' }) {
        std::vector<std::uint8_t> flash = { static_cast<std::uint8_t>(first), 'W', 'S', 0x0A, 0, 0, 0, 0, 0, 0, 0, 0 };
        REQUIRE(type_of(recognize(flash, u"c:\\movie.dat")) == "application/x-shockwave-flash");
    }

    // ftyp sits at byte 4, so the brand runs from 4 to 11.
    REQUIRE(type_of(recognize({ 0, 0, 0, 0x18, 'f', 't', 'y', 'p', 'm', 'p', '4', '2' }, u"c:\\clip.dat")) == "video/mp4");
}

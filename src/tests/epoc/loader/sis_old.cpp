/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <catch2/catch.hpp>
#include <loader/sis_old.h>
#include <cstdio>
#include <fstream>

using namespace eka2l1::loader;

namespace {
    using record = std::vector<std::uint32_t>;
    record condition(std::uint32_t op, std::uint32_t value) {
        return {op, 12, sis_old_file_expression_type_number, value, 0};
    }
    record file(std::uint32_t tag) {
        return {file_record_type_simple_file, 0, tag, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    }
    std::optional<sis_old> parse_records(const std::vector<record> &records) {
        const char *path = "sis-old-conditional-test.sis";
        struct cleanup { const char *path; ~cleanup() { std::remove(path); } } remove{path};
        sis_old_header header{};
        header.num_files = static_cast<std::uint16_t>(records.size());
        header.file_ptr = sizeof(header);
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(&header), sizeof(header));
        // makesis stores its linked list in reverse package order.
        for (auto it = records.rbegin(); it != records.rend(); ++it) {
            out.write(reinterpret_cast<const char *>(it->data()), it->size() * sizeof(std::uint32_t));
        }
        out.close();
        return parse_sis_old(path);
    }
}

TEST_CASE("SIS old ELSEIF belongs to its IF and ENDIF resumes the parent", "[sis]") {
    auto parsed = parse_records({condition(file_record_type_if, 1), file(10),
        condition(file_record_type_elseif, 2), file(20), {file_record_type_else}, file(30),
        {file_record_type_endif}, file(40)});
    REQUIRE(parsed);
    auto &root = parsed->root_block;
    REQUIRE(root.true_commands.size() == 2);
    auto &first = *static_cast<sis_old_block *>(root.true_commands[0].get());
    REQUIRE(first.true_commands.size() == 1);
    REQUIRE(static_cast<sis_old_file *>(first.true_commands[0].get())->file_details == 10);
    auto &second = *static_cast<sis_old_block *>(first.false_commands[0].get());
    REQUIRE(static_cast<sis_old_file *>(second.true_commands[0].get())->file_details == 20);
    REQUIRE(static_cast<sis_old_file *>(second.false_commands[0].get())->file_details == 30);
    REQUIRE(static_cast<sis_old_file *>(root.true_commands[1].get())->file_details == 40);
}

TEST_CASE("SIS old rejects unmatched conditional records", "[sis]") {
    REQUIRE_FALSE(parse_records({{file_record_type_endif}}));
    REQUIRE_FALSE(parse_records({{file_record_type_else}}));
    REQUIRE_FALSE(parse_records({condition(file_record_type_elseif, 1)}));
    REQUIRE_FALSE(parse_records({condition(file_record_type_if, 1)}));
    REQUIRE_FALSE(parse_records({condition(file_record_type_if, 1), {file_record_type_else},
        {file_record_type_else}, {file_record_type_endif}}));
}

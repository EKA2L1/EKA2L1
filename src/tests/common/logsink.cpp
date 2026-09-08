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

#include <common/log.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace eka2l1;

namespace {
    std::vector<std::string> read_lines(const std::string &path) {
        std::vector<std::string> lines;
        std::ifstream stream(path, std::ios::binary);
        std::string line;

        while (std::getline(stream, line)) {
            // The file is read as binary, so a Windows line ending keeps its carriage return.
            if (!line.empty() && (line.back() == '\r')) {
                line.pop_back();
            }

            lines.push_back(line);
        }

        return lines;
    }

    std::shared_ptr<spdlog::logger> make_logger(const std::string &path, const std::size_t max_lines) {
        auto logger = std::make_shared<spdlog::logger>(path, log::make_capped_file_sink(path, max_lines));
        logger->set_pattern("%v");
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::trace);

        return logger;
    }
}

TEST_CASE("capped_log_sink_keeps_the_newest_lines_within_its_budget", "log_sink") {
    const std::string path = "capped_log_budget.log";
    const std::size_t max_lines = 100;

    {
        auto logger = make_logger(path, max_lines);

        for (int i = 0; i < 1000; i++) {
            logger->info("line {}", i);
        }
    }

    const std::vector<std::string> lines = read_lines(path);
    std::remove(path.c_str());

    REQUIRE(!lines.empty());
    REQUIRE(lines.size() <= max_lines);

    // The tail is the part worth keeping, and it must survive intact.
    REQUIRE(lines.back() == "line 999");

    for (std::size_t i = 1; i + 1 < lines.size(); i++) {
        const int previous = std::stoi(lines[i].substr(5));
        const int current = std::stoi(lines[i + 1].substr(5));

        REQUIRE(current == previous + 1);
    }
}

TEST_CASE("capped_log_sink_reports_how_many_lines_it_dropped", "log_sink") {
    const std::string path = "capped_log_notice.log";
    const std::size_t max_lines = 100;
    const int written = 1000;

    {
        auto logger = make_logger(path, max_lines);

        for (int i = 0; i < written; i++) {
            logger->info("line {}", i);
        }
    }

    const std::vector<std::string> lines = read_lines(path);
    std::remove(path.c_str());

    REQUIRE(lines.size() >= 2);
    REQUIRE(lines.front().rfind("--- log trimmed: ", 0) == 0);

    const std::size_t dropped = std::stoul(lines.front().substr(std::string("--- log trimmed: ").size()));
    const std::size_t first_surviving = static_cast<std::size_t>(std::stoi(lines[1].substr(5)));

    // Every log line missing from the head is accounted for. The count also covers the
    // notice each earlier trim left behind, which the next trim drops in turn.
    const std::size_t trims_at_most = (static_cast<std::size_t>(written) / (max_lines / 2)) + 1;

    REQUIRE(dropped >= first_surviving);
    REQUIRE(dropped <= first_surviving + trims_at_most);
}

TEST_CASE("capped_log_sink_survives_a_budget_of_one_line", "log_sink") {
    const std::string path = "capped_log_minimum.log";

    {
        auto logger = make_logger(path, 1);

        for (int i = 0; i < 20; i++) {
            logger->info("line {}", i);
        }
    }

    const std::vector<std::string> lines = read_lines(path);
    std::remove(path.c_str());

    REQUIRE(!lines.empty());
    REQUIRE(lines.size() <= 3);
    REQUIRE(lines.front().rfind("--- log trimmed: ", 0) == 0);
}

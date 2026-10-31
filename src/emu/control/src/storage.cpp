// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "storage.h"
#include <algorithm>
#include <atomic>
#if defined(_WIN32)
#include <windows.h>
#endif
#include <charconv>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

namespace eka2l1::control {
    namespace fs = std::filesystem;
    static void require(bool condition, const std::string &code, const std::string &message, int status = 422) {
        if (!condition)
            throw api_error(status, code, message);
    }
    static std::string read_file(const fs::path &path) {
        std::ifstream file(path, std::ios::binary);
        require(bool(file), "not_found", "cannot read " + path.string(), 404);
        return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }
    response host_files(const request &req, const std::string &resolved) {
        const auto raw = req.param("path");
        require(!raw.empty(), "bad_request", "path query parameter required", 400);
        auto path = fs::u8path(resolved);
        if (req.method == "GET") {
            require(fs::exists(path), "not_found", "file does not exist", 404);
            if (fs::is_directory(path)) {
                json::array list;
                for (const auto &entry : fs::directory_iterator(path)) {
                    json value{ { "name", entry.path().filename().string() }, { "directory", entry.is_directory() } };
                    if (!entry.is_directory())
                        value["size"] = static_cast<std::uint64_t>(entry.file_size());
                    const auto modified = entry.last_write_time();
                    const auto time = std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        modified - fs::file_time_type::clock::now() + std::chrono::system_clock::now()));
                    std::tm utc{};
#if defined(_WIN32)
                    gmtime_s(&utc, &time);
#else
                    gmtime_r(&time, &utc);
#endif
                    std::ostringstream date;
                    date << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
                    value["modified"] = date.str();
                    list.push_back(std::move(value));
                }
                std::sort(list.begin(), list.end(), [](const auto &a, const auto &b) { return a["name"].string() < b["name"].string(); });
                return response::from_json({ { "path", raw }, { "hostPath", path.string() }, { "entries", list } });
            }
            return { 200, "application/octet-stream", read_file(path), { { "X-Host-Path", path.string() } } };
        }
        if (req.method == "PUT") {
            require(!fs::is_directory(path), "is_directory", "path is a directory", 409);
            fs::create_directories(path.parent_path());
            auto temp = path;
            static std::atomic<std::uint64_t> sequence{ 0 };
            temp += ".eka2l1-upload-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(sequence++);
            struct cleanup {
                fs::path path;
                ~cleanup() {
                    std::error_code ec;
                    fs::remove(path, ec);
                }
            } remove_temp{ temp };
            {
                std::ofstream file(temp, std::ios::binary);
                file.write(req.body.data(), req.body.size());
                require(bool(file), "write_failed", "cannot write file", 500);
            }
#if defined(_WIN32)
            require(MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),
                "write_failed", "cannot replace file", 500);
#else
            fs::rename(temp, path);
#endif
            return response::from_json({ { "path", raw }, { "hostPath", path.string() }, { "size", std::uint64_t(req.body.size()) } });
        }
        if (req.method == "DELETE") {
            require(fs::exists(path), "not_found", "file does not exist", 404);
            require(req.param("recursive") == "1" || !fs::is_directory(path) || fs::is_empty(path), "not_empty", "pass recursive=1 to delete a nonempty directory", 409);
            fs::remove_all(path);
            return response::from_json({ { "deleted", raw }, { "hostPath", path.string() } });
        }
        throw api_error(404, "not_found", "unknown files operation");
    }
    response host_log(const request &req, const std::string &path) {
        std::ifstream file(fs::u8path(path), std::ios::binary);
        std::uint64_t end = 0;
        if (file) {
            file.seekg(0, std::ios::end);
            end = file.tellg();
        }
        std::uint64_t start = end > 4 * 1024 * 1024 ? end - 4 * 1024 * 1024 : 0;
        if (req.query.count("since")) {
            const auto value = req.param("since");
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), start);
            require(parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size(), "bad_request", "invalid since offset", 400);
            if (start > end)
                start = 0;
        }
        if (end - start > 16 * 1024 * 1024)
            start = end - 16 * 1024 * 1024;
        std::string text(end - start, 0);
        if (file) {
            file.seekg(start);
            file.read(text.data(), text.size());
        }
        auto pattern = req.param("grep");
        const bool filtered = !pattern.empty() || !req.query.count("since") || req.query.count("tail");
        if (filtered) {
            std::regex regex;
            try {
                regex = std::regex(pattern);
            } catch (...) {
                throw api_error(400, "bad_request", "invalid grep regex");
            }
            std::istringstream stream(text);
            std::string line;
            std::vector<std::string> lines;
            while (std::getline(stream, line))
                if (pattern.empty() || std::regex_search(line, regex))
                    lines.push_back(line);
            int tail = 200;
            try {
                if (req.query.count("tail"))
                    tail = std::stoi(req.param("tail"));
            } catch (...) {
                throw api_error(400, "bad_request", "invalid tail");
            }
            std::size_t first = tail > 0 && lines.size() > std::size_t(tail) ? lines.size() - tail : 0;
            text.clear();
            for (auto i = first; i < lines.size(); ++i)
                text += lines[i] + "\n";
        }
        return { 200, "text/plain; charset=utf-8", std::move(text), { { "X-Log-Offset", std::to_string(end) } } };
    }
}

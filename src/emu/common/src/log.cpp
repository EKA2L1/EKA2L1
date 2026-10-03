/*
 * Copyright (c) 2018 EKA2L1 Team.
 * Copyright 2018 Citra Emulator Project.
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

#include <common/algorithm.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <common/platform.h>
#include <common/pystr.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#if EKA2L1_PLATFORM(WIN32)
#include <common/cvt.h>
#include <filesystem>
#endif

#ifdef _MSC_VER
#include <spdlog/sinks/msvc_sink.h>
#elif EKA2L1_PLATFORM_ANDROID
#include "spdlog/sinks/android_sink.h"
#endif

#include <spdlog/details/os.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/dist_sink.h>

#if EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#include <wincon.h>
#endif

namespace eka2l1 {
    bool already_setup = false;

    const char *log_class_to_string(const log_class cls) {
        if (cls >= LOG_CLASS_COUNT) {
            return nullptr;
        }

#define LOGCLASS(name, short_nice_name, nice_name) short_nice_name,
        static const char *LOG_CLASS_NAME_ARRAYS[LOG_CLASS_COUNT] = {
#include <common/logclass.def>
#undef LOGCLASS
        };

        return LOG_CLASS_NAME_ARRAYS[static_cast<int>(cls)];
    }

    bool string_to_log_class(const char *str, log_class &result) {
#define LOGCLASS(name, short_nice_name, nice_name) if (eka2l1::common::compare_ignore_case(short_nice_name, str) == 0) { result = name; return true; }
#include <common/logclass.def>
#undef LOGCLASS

        return false;
    }

    bool string_to_log_level(const char *str, spdlog::level::level_enum &level) {
        if (common::compare_ignore_case(str, "Debug") == 0) {
            level = spdlog::level::debug;
            return true;
        }

        if ((common::compare_ignore_case(str, "Error") == 0) || (common::compare_ignore_case(str, "Err") == 0)) {
            level = spdlog::level::err;
            return true;
        }

        if (common::compare_ignore_case(str, "Trace") == 0) {
            level = spdlog::level::trace;
            return true;
        }

        if ((common::compare_ignore_case(str, "Warn") == 0) || (common::compare_ignore_case(str, "Warning") == 0)) {
            level = spdlog::level::warn;
            return true;
        }

        if (common::compare_ignore_case(str, "Critical") == 0) {
            level = spdlog::level::critical;
            return true;
        }

        if (common::compare_ignore_case(str, "Info") == 0) {
            level = spdlog::level::info;
            return true;
        }

        if (common::compare_ignore_case(str, "Off") == 0) {
            level = spdlog::level::off;
            return true;
        }

        return false;
    }

    log_filterings::log_filterings() {
        reset_all(spdlog::level::trace);
    }

    bool log_filterings::set_minimum_level(const log_class cls, const spdlog::level::level_enum level) {
        if (cls >= LOG_CLASS_COUNT) {
            return false;
        }

        levels_[static_cast<int>(cls)] = level;
        return true;
    }

    bool log_filterings::is_passed(const log_class cls, const spdlog::level::level_enum level) {
        return (cls < LOG_CLASS_COUNT) && (level >= levels_[static_cast<int>(cls)]);
    }

    void log_filterings::reset_all(const spdlog::level::level_enum level) {
        std::fill(levels_, levels_ + LOG_CLASS_COUNT, level);
    }

    void log_filterings::parse_filter_string(const std::string &filtering_str) {
        // Format is taken from citra!
        common::pystr filter_pstr(filtering_str);
        std::vector<common::pystr> rules = filter_pstr.split(' ');

        for (const common::pystr &rule: rules) {
            std::vector<common::pystr> comp = rule.split(':');
            if (comp.size() != 2) {
                LOG_ERROR(COMMON, "Rule {} is invalid (valid format: <class>:<level>)!", filtering_str);
            } else {
                spdlog::level::level_enum level_in_rule;
                if (!string_to_log_level(comp[1].cstr(), level_in_rule)) {
                    LOG_ERROR(COMMON, "Unrecognised level {} in rule {}", comp[1].std_str(), rule.std_str());
                    continue;
                }

                log_class class_in_rule;
                if (comp[0] == "*") {
                    reset_all(level_in_rule);
                } else {
                    if (!string_to_log_class(comp[0].cstr(), class_in_rule)) {
                        LOG_ERROR(COMMON, "Unrecognized class {} in rule {}", comp[0].std_str(), rule.std_str());
                    } else {
                        set_minimum_level(class_in_rule, level_in_rule);
                    }
                }
            }
        }
    }

    namespace log {
        std::shared_ptr<spdlog::logger> spd_logger;
        std::unique_ptr<log_filterings> filterings;
        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> stdout_color_sink;
        std::shared_ptr<spdlog::sinks::dist_sink_mt> color_dist_sink;

        bool console_shown = false;

#if EKA2L1_PLATFORM(WIN32)
        // Windows reads a narrow file name in the ANSI code page, so the standard
        // streams are given the UTF-8 name as a wide one.
        static std::filesystem::path stream_path(const std::string &path) {
            return std::filesystem::path(common::utf8_to_wstr(path));
        }
#else
        static const std::string &stream_path(const std::string &path) {
            return path;
        }
#endif

        // The file sink runs under its own lock, and every log line ends up in it, so
        // it works on its file through the C runtime alone: common's file helpers take
        // a lock of their own and may log, which would come back into the sink.
        namespace log_file {
#if EKA2L1_PLATFORM(WIN32)
            // Windows reads a narrow name in the ANSI code page: hand it the wide one.
            static std::FILE *open(const std::string &path, const char *mode) {
                return _wfopen(common::utf8_to_wstr(path).c_str(), common::utf8_to_wstr(mode).c_str());
            }

            static void create_folder(const std::string &folder) {
                std::error_code ignored;
                std::filesystem::create_directories(stream_path(folder), ignored);
            }

            static void remove(const std::string &path) {
                _wremove(common::utf8_to_wstr(path).c_str());
            }

            static void rename(const std::string &path, const std::string &new_path) {
                _wrename(common::utf8_to_wstr(path).c_str(), common::utf8_to_wstr(new_path).c_str());
            }
#else
            static std::FILE *open(const std::string &path, const char *mode) {
                return std::fopen(path.c_str(), mode);
            }

            static void create_folder(const std::string &folder) {
                spdlog::details::os::create_dir(folder);
            }

            static void remove(const std::string &path) {
                std::remove(path.c_str());
            }

            static void rename(const std::string &path, const std::string &new_path) {
                std::rename(path.c_str(), new_path.c_str());
            }
#endif
        }

        // A file sink that stays within a line budget. Once the budget is reached the
        // oldest half of the file is dropped: the newest lines are the ones worth
        // keeping, and halving makes the rewrite rare enough not to matter.
        //
        // The sink opens its file itself rather than through spdlog's file helper,
        // which takes a narrow name: on Windows a log in a folder whose name is not
        // ASCII could not be opened.
        class capped_file_sink final : public spdlog::sinks::base_sink<std::mutex> {
        public:
            capped_file_sink(const std::string &filename, const std::size_t max_lines)
                : filename_(filename)
                , max_lines_(max_lines) {
                open_file(true);
            }

            ~capped_file_sink() override {
                close_file();
            }

        protected:
            void sink_it_(const spdlog::details::log_msg &msg) override {
                spdlog::memory_buf_t formatted;
                base_sink<std::mutex>::formatter_->format(msg, formatted);

                write_file(formatted.data(), formatted.size());
                lines_ += static_cast<std::size_t>(std::count(formatted.begin(), formatted.end(), '\n'));

                if (lines_ >= max_lines_) {
                    drop_oldest_lines();
                }
            }

            void flush_() override {
                if (file_ && (std::fflush(file_) != 0)) {
                    spdlog::throw_spdlog_ex("Failed flush to file " + filename_, errno);
                }
            }

        private:
            static constexpr int OPEN_TRIES = 5;
            static constexpr unsigned int OPEN_RETRY_INTERVAL_MS = 10;

            // Opens the file the way spdlog's file helper does: the folder is created,
            // a truncation is done separately and the file is written in append mode, and
            // a file that is briefly held elsewhere gets a few more tries.
            void open_file(const bool truncate) {
                close_file();

                const std::string folder = eka2l1::file_directory(filename_);
                if (!folder.empty()) {
                    log_file::create_folder(folder);
                }

                for (int tries = 0; tries < OPEN_TRIES; tries++) {
                    std::FILE *truncated = truncate ? log_file::open(filename_, "wb") : nullptr;
                    if (truncated) {
                        std::fclose(truncated);
                    }

                    if (!truncate || truncated) {
                        file_ = log_file::open(filename_, "ab");
                        if (file_) {
                            return;
                        }
                    }

                    spdlog::details::os::sleep_for_millis(OPEN_RETRY_INTERVAL_MS);
                }

                spdlog::throw_spdlog_ex("Failed opening file " + filename_ + " for writing", errno);
            }

            void close_file() {
                if (file_) {
                    std::fclose(file_);
                    file_ = nullptr;
                }
            }

            void write_file(const char *data, const std::size_t size) {
                if (file_ && (std::fwrite(data, 1, size, file_) != size)) {
                    spdlog::throw_spdlog_ex("Failed writing to file " + filename_, errno);
                }
            }

            std::string trim_notice(const std::size_t dropped_total) const {
                // The rest of the file ends its lines the way spdlog does, so this one has to too.
                return "--- log trimmed: " + std::to_string(dropped_total) + " oldest lines dropped so far ---"
                    + spdlog::details::os::default_eol;
            }

            void drop_oldest_lines() {
                const std::string trimmed_path = filename_ + ".trim";

                std::size_t dropped = lines_;
                std::size_t kept_lines = 0;
                bool rewritten = false;

                close_file();

                {
                    std::ifstream source(stream_path(filename_), std::ios::binary);

                    if (source) {
                        // Counting the dropped lines off rather than seeking to the middle
                        // of the file keeps the budget exact whatever the lines measure.
                        const std::size_t to_drop = (lines_ / 2) + 1;

                        std::size_t skipped = 0;
                        std::string line;

                        while ((skipped < to_drop) && std::getline(source, line)) {
                            skipped++;
                        }

                        std::ofstream kept(stream_path(trimmed_path), std::ios::binary | std::ios::trunc);

                        if (kept) {
                            const std::string notice = trim_notice(dropped_ + skipped);
                            kept.write(notice.data(), static_cast<std::streamsize>(notice.size()));
                            kept_lines = 1;

                            std::vector<char> buffer(64 * 1024);

                            while (source) {
                                source.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

                                const std::streamsize read_size = source.gcount();
                                if (read_size <= 0) {
                                    break;
                                }

                                kept.write(buffer.data(), read_size);
                                kept_lines += static_cast<std::size_t>(std::count(buffer.data(), buffer.data() + read_size, '\n'));
                            }

                            rewritten = kept.good();
                            dropped = skipped;
                        }
                    }
                }

                dropped_ += dropped;

                if (rewritten) {
                    log_file::remove(filename_);
                    log_file::rename(trimmed_path, filename_);

                    open_file(false);
                    lines_ = kept_lines;

                    return;
                }

                // The rewrite failed, but the budget still has to hold: start the file
                // over rather than let it grow.
                log_file::remove(trimmed_path);
                open_file(true);

                const std::string notice = trim_notice(dropped_);
                write_file(notice.data(), notice.size());
                lines_ = 1;
            }

            std::FILE *file_ = nullptr;
            std::string filename_;
            std::size_t max_lines_;
            std::size_t lines_ = 0;
            std::size_t dropped_ = 0;
        };

        std::shared_ptr<spdlog::sinks::sink> make_capped_file_sink(const std::string &filename, const std::size_t max_lines) {
            return std::make_shared<capped_file_sink>(filename, max_lines);
        }

        struct imgui_logger_sink : public spdlog::sinks::base_sink<std::mutex> {
            explicit imgui_logger_sink(std::shared_ptr<base_logger> _logger)
                : logger(_logger.get()) {}

            void set_force_clear(bool clear) {
                force_clear = clear;
            }

        private:
            base_logger *logger;
            bool force_clear = false;

        protected:
            void sink_it_(const spdlog::details::log_msg &msg) override {
                spdlog::memory_buf_t formatted;
                base_sink<std::mutex>::formatter_->format(msg, formatted);

                const std::string real_msg = fmt::to_string(formatted);
                logger->log(real_msg.c_str());

                if (force_clear) {
                    flush_();
                }
            }

            void flush_() override {
                //logger->clear();
            }
        };

        void setup_log(std::shared_ptr<base_logger> gui_logger) {
            const char *log_file_name = "EKA2L1.log";
            const char *log_file_name_prev = "EKA2L1_TakeThis.log";

            if (common::exists(log_file_name)) {
                common::move_file(log_file_name, log_file_name_prev);
            }

            std::vector<spdlog::sink_ptr> sinks;

            common::remove(log_file_name);

            color_dist_sink = std::make_shared<spdlog::sinks::dist_sink_mt>();

            sinks.push_back(color_dist_sink);
            sinks.push_back(make_capped_file_sink(log_file_name, LOG_FILE_MAX_LINES));

#ifdef _MSC_VER
            sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_st>());
#elif EKA2L1_PLATFORM(ANDROID)
            sinks.push_back(std::make_shared<spdlog::sinks::android_sink_mt>());
#endif
            if (gui_logger) {
                sinks.push_back(std::make_shared<imgui_logger_sink>(gui_logger));
            }

            spd_logger = std::make_unique<spdlog::logger>("EKA2L1 Logger", begin(sinks), end(sinks));
            spdlog::set_default_logger(spd_logger);

            spdlog::set_error_handler([](const std::string &msg) {
                std::cerr << "spdlog error: " << msg << std::endl;
            });

            spdlog::set_pattern("%L %^%v%$");
            spdlog::set_level(spdlog::level::trace);

            spd_logger->flush_on(spdlog::level::debug);

            // Setup the filterings
            filterings = std::make_unique<log_filterings>();
            already_setup = true;
        }
        
        // See https://github.com/citra-emu/citra/blob/master/src/citra_qt/debugger/console.cpp
        void toggle_console() {
            if (!already_setup) {
                return;
            }

#ifdef _WIN32
            FILE *temp = nullptr;
#endif

            if (!console_shown) {
#ifdef _WIN32
                if (AllocConsole()) {
#endif

                stdout_color_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                stdout_color_sink->set_pattern("%L %^%v%$");
                stdout_color_sink->set_level(spdlog::level::trace);

                color_dist_sink->add_sink(stdout_color_sink);
                console_shown = true;

#ifdef _WIN32
                }
#endif
            } else {
#ifdef _WIN32
                if (FreeConsole()) {
#endif
                console_shown = false;
                color_dist_sink->remove_sink(stdout_color_sink);
                stdout_color_sink.reset();

#ifdef _WIN32
                }
#endif
            }
        }
        
        bool is_console_enabled() {
            return console_shown;
        }
    }
}

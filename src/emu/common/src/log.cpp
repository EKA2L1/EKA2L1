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
#include <common/platform.h>
#include <common/pystr.h>

#define SPDLOG_FMT_EXTERNAL
#include <spdlog/spdlog.h>

#include <iostream>
#include <memory>

#ifdef _MSC_VER
#include <spdlog/sinks/msvc_sink.h>
#elif EKA2L1_PLATFORM_ANDROID
#include "spdlog/sinks/android_sink.h"
#endif

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
                LOG_ERROR(COMMON, "Rule {} is invalid (valid format: <class>:<level>)!");
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

#if EKA2L1_PLATFORM(WIN32)
            DeleteFileA(log_file_name);
#else
            remove(log_file_name);
#endif

            color_dist_sink = std::make_shared<spdlog::sinks::dist_sink_mt>();

            sinks.push_back(color_dist_sink);
            sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_name));

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

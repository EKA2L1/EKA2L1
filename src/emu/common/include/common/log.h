/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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
#pragma once

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

namespace eka2l1 {
    extern bool already_setup;

    class base_logger {
    public:
        virtual void log(const char *fmt, ...) = 0;
        virtual void clear() = 0;
    };

    namespace log {
        extern std::shared_ptr<spdlog::logger> spd_logger;

        void setup_log(std::shared_ptr<base_logger> gui_logger);
    }
}

#define LOG_TRACE(fmt, ...) eka2l1::log::spd_logger->trace("{:s}: " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) eka2l1::log::spd_logger->debug("{:s}: " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) eka2l1::log::spd_logger->info("{:s}: " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) eka2l1::log::spd_logger->warn("{:s}: " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) eka2l1::log::spd_logger->error("{:s}: " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) eka2l1::log::spd_logger->critical("{:s}: " fmt, __FUNCTION__, ##__VA_ARGS__)


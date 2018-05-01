#pragma once

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

namespace eka2l1 {
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

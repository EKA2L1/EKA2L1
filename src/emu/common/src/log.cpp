#include <common/log.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <iostream>

#ifdef _MSC_VER
#include <spdlog/sinks/msvc_sink.h>
#endif

namespace eka2l1 {
    namespace log {
        std::shared_ptr<spdlog::logger> spd_logger;

        struct imgui_logger_sink: public spdlog::sinks::base_sink<std::mutex> {
            explicit imgui_logger_sink(std::shared_ptr<base_logger> _logger)
                : logger(_logger.get()) {}

            void set_force_clear(bool clear) {
                force_clear = clear;
            }

        private:
            base_logger* logger;
            bool force_clear = false;

        protected:
            void _sink_it(const spdlog::details::log_msg &msg) override
            {
                logger->log(msg.formatted.str().c_str());

                if (force_clear)
                {
                    _flush();
                }
            }

            void _flush() override
            {
                //logger->clear();
            }
        };

        void setup_log(std::shared_ptr<base_logger> gui_logger) {
            const char* log_file_name = "EKA2L1.log";

            std::vector<spdlog::sink_ptr> sinks;

            sinks.push_back(std::make_shared<imgui_logger_sink>(gui_logger));

#ifdef WIN32
            sinks.push_back(std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>());
#else
            sinks.push_back(std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>());
#endif

            sinks.push_back(std::make_shared<spdlog::sinks::simple_file_sink_mt>(log_file_name));

#ifdef _MSC_VER
            sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_st>());
#endif

           spd_logger = std::make_shared<spdlog::logger>("EKA2L1 Logger", begin(sinks), end(sinks));
           spdlog::register_logger(spd_logger);

           spdlog::set_error_handler([](const std::string& msg)
           {
               std::cerr << "spdlog error: " << msg << std::endl;
           });

           spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");
           spdlog::set_level(spdlog::level::trace);

           spd_logger->flush_on(spdlog::level::debug);
        }
    }
}

#include "Logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

std::shared_ptr<spdlog::logger> Logger::s_logger;

void Logger::init()
{
    // create console sink (how it should be displayed in the console)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    //console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %^%v%$\n");
    
    
    // Create file sink (how it should be displayed in the file)
    // Create logs directory if it doesn't exist
    std::filesystem::path logsPath = std::filesystem::current_path() / "logs";
    std::filesystem::create_directories(logsPath);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((logsPath / "log.txt").string(), true);
    //file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v\n");

    // create logger with multiple sinks
    s_logger = std::make_shared<spdlog::logger>("logger", 
        sinks_init_list{console_sink, file_sink});

    // set global pattern
    //set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] %v%$", pattern_time_type::local);
    auto f = std::make_unique<spdlog::pattern_formatter>("[%Y-%m-%d %H:%M:%S.%e] %^%v%$", spdlog::pattern_time_type::local, std::string("\n"));  // disable eol
    s_logger->set_formatter( std::move(f) );
    
    // set global log level
    s_logger->set_level(spdlog::level::trace);

    // flush on error
    s_logger->flush_on(spdlog::level::info);

}

void Logger::printToConsole(std::string message, level::level_enum level)
{
    switch (level)
    {
        case level::trace:
            s_logger->trace(message);
            break;
        case level::debug:
            s_logger->debug(message);
            break;
        case level::info:
            s_logger->info(message);
             break;
        case level::warn:
            s_logger->warn(message);
            break;
        case level::err:
            s_logger->error(message);
            break;
        case level::critical:
            s_logger->critical(message);
            break;
        default:
            s_logger->info(message);
            break;
    }
}
#pragma once
#include <spdlog/spdlog.h>
#include <memory>
#include <filesystem>

using namespace spdlog;

class Logger {
public:
    static void init();
    static void printToConsole(std::string message, level::level_enum level = level::info);
    
private:
    static std::shared_ptr<spdlog::logger> s_logger;
};
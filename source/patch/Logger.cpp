#include "patch/Logger.hpp"
#include <fstream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iostream>

namespace love {

std::string Logger::logPath;

static std::mutex logMutex;

void Logger::init(const std::string& basePath)
{
    logPath = basePath;
    if (!logPath.empty() && logPath.back() != '/' && logPath.back() != '\\')
        logPath += "/";
    logPath += "lovely_log.txt";
}

static std::string timestamp()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return std::string(buf);
}

void Logger::info(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(logMutex);
    if (!logPath.empty())
    {
        std::ofstream ofs(logPath, std::ios::app);
        if (ofs)
            ofs << timestamp() << " [INFO] " << msg << "\n";
    }
    else
    {
        std::cerr << timestamp() << " [INFO] " << msg << "\n";
    }
}

void Logger::error(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(logMutex);
    if (!logPath.empty())
    {
        std::ofstream ofs(logPath, std::ios::app);
        if (ofs)
            ofs << timestamp() << " [ERROR] " << msg << "\n";
    }
    else
    {
        std::cerr << timestamp() << " [ERROR] " << msg << "\n";
    }
}

} // namespace love

#pragma once

#include <string>

namespace love {

class Logger {
public:
    static void init(const std::string& basePath);
    static void info(const std::string& msg);
    static void error(const std::string& msg);
private:
    static std::string logPath;
};

} // namespace love

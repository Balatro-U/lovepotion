#include "DebugLogger.hpp"
#include <cstring>
#include <ctime>

namespace love
{
    FILE* DebugLogger::logFile = nullptr;
    bool DebugLogger::initialized = false;

    void DebugLogger::init()
    {
        if (initialized)
            return;

        // Try multiple paths for log file
        const char* logPaths[] = {
            "/vol/external01/wiiu/apps/balatro/balatro_debug.log",
            "/vol/external01/balatro_debug.log",
            "balatro_debug.log"  // fallback to current directory
        };
        
        const char* usedPath = nullptr;
        for (int i = 0; i < 3; i++)
        {
            logFile = fopen(logPaths[i], "w");
            if (logFile != nullptr)
            {
                usedPath = logPaths[i];
                break;
            }
        }
        
        if (logFile != nullptr)
        {
            initialized = true;
            
            // Log header information
            fprintf(logFile, "=== LOVE Potion Debug Log Started ===\n");
            fprintf(logFile, "Build Type: DEBUG\n");
            fprintf(logFile, "Platform: Wii U (Cafe)\n");
            
            time_t rawtime;
            struct tm* timeinfo;
            char buffer[80];
            
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
            fprintf(logFile, "Started at: %s\n", buffer);
            fprintf(logFile, "Log file: %s\n", usedPath);
            fprintf(logFile, "=====================================\n");
            
            fflush(logFile);
        }
        else
        {
            // If all paths failed, at least mark as initialized to avoid repeated attempts
            initialized = true;
        }
    }

    void DebugLogger::log(const char* format, ...)
    {
        // If not initialized, try to auto-initialize
        if (!initialized)
            init();
            
        // If still no log file, try to fallback to simple_debug.log
        if (logFile == nullptr)
        {
            // Try to write to simple_debug.log as fallback
            FILE* fallbackFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                
            if (fallbackFile != nullptr)
            {
                va_list args;
                va_start(args, format);
                
                time_t rawtime;
                struct tm* timeinfo;
                char timeBuffer[20];
                
                time(&rawtime);
                timeinfo = localtime(&rawtime);
                strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", timeinfo);
                
                fprintf(fallbackFile, "[%s] DEBUG_FALLBACK: ", timeBuffer);
                vfprintf(fallbackFile, format, args);
                fprintf(fallbackFile, "\n");
                fflush(fallbackFile);
                fclose(fallbackFile);
                
                va_end(args);
            }
            return;
        }

        va_list args;
        va_start(args, format);
        
        // Add timestamp
        time_t rawtime;
        struct tm* timeinfo;
        char timeBuffer[20];
        
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", timeinfo);
        
        fprintf(logFile, "[%s] ", timeBuffer);
        vfprintf(logFile, format, args);
        fprintf(logFile, "\n");
        fflush(logFile);
        
        va_end(args);
    }

    void DebugLogger::logLuaError(const char* error)
    {
        log("LUA ERROR: %s", error);
    }

    void DebugLogger::close()
    {
        if (initialized && logFile != nullptr)
        {
            log("=== LOVE Potion Debug Log Ended ===");
            fclose(logFile);
            logFile = nullptr;
            initialized = false;
        }
    }
}

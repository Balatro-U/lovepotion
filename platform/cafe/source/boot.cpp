#include "boot.hpp"
#include "common/Console.hpp"
#include "common/service.hpp"
#include "DebugLogger.hpp"

#include "driver/EventQueue.hpp"

#include <coreinit/bsp.h>
#include <coreinit/core.h>
#include <coreinit/dynload.h>
#include <coreinit/exit.h>
#include <coreinit/filesystem.h>
#include <coreinit/foreground.h>
#include <coreinit/screen.h>
#include <coreinit/time.h>

#include <nn/ac/ac_c.h>

#include <proc_ui/procui.h>

#include <padscore/kpad.h>
#include <vpad/input.h>

#include <sysapp/launch.h>

#include <vector>

namespace love
{
    // clang-format off
    // CEMU COMPATIBILITY: Reduced services for better compatibility
    static constexpr std::array<const Service, 2> services =
    {{
        { "procUI", BIND(ProcUIInit, OSSavesDone_ReadyToRelease), &ProcUIShutdown },
        // { "vpad",   BIND(VPADInit),                               &VPADShutdown   }, // CEMU: VPADInit causes problems
        // { "kpad",   BIND(KPADInit),                               &KPADShutdown   }, // CEMU: Disable for testing
        // { "ac",     BIND(ACInitialize),                           &ACFinalize     }, // CEMU: Network may cause hang
        { "fs",     BIND(FSInit),                                 &FSShutdown     }
        // { "bsp",    BIND(bspInitializeShimInterface),             []() { }        }
    }};
    // clang-format on

    uint32_t Console::mainCoreId = 0;
    bool Console::mainCoreIdSet  = false;

    static constexpr const char* DEFAULT_PATH = "fs:/vol/external01/balatro.wuhb";

    std::string getApplicationPath(const std::string& argv0)
    {
        if (argv0 == "embedded boot.lua")
            return DEFAULT_PATH;

        OSDynLoad_Module module;
        const auto type  = OS_DYNLOAD_EXPORT_FUNC;
        const char* name = "RL_GetPathOfRunningExecutable";

        if (OSDynLoad_Acquire("homebrew_rpx_loader", &module) == OS_DYNLOAD_OK)
        {
            char path[256];

            bool (*RL_GetPathOfRunningExecutable)(char*, uint32_t);
            auto** function = reinterpret_cast<void**>(&RL_GetPathOfRunningExecutable);

            if (OSDynLoad_FindExport(module, type, name, function) == OS_DYNLOAD_OK)
            {
                if (RL_GetPathOfRunningExecutable(path, sizeof(path)) == 0)
                    return path;
            }
        }

        return DEFAULT_PATH;
    }

    int preInit()
    {
        // Initialize debug logger (safe to call multiple times)
        DebugLogger::init();
        DebugLogger::log("=== PREINIT STARTED ===");
        DebugLogger::log("preInit() called - beginning service initialization");

        /* we aren't running Aroma */
        // if (getApplicationPath().empty())
        //     return -1;

        // Initialize services first without loading screen
        DebugLogger::log("About to initialize %zu services", services.size());
        
        int serviceCount = 0;
        for (const auto& service : services)
        {
            DebugLogger::log("=== SERVICE INIT: %s ===", service.name);
            DebugLogger::log("About to initialize service: %s", service.name);
            
#ifdef __WIIU__
            // Also log to simple file for Cemu debugging
            FILE* serviceLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (serviceLog) {
                fprintf(serviceLog, "=== INITIALIZING SERVICE: %s ===\n", service.name);
                fflush(serviceLog);
                fclose(serviceLog);
            }
#endif
            
            auto result = service.init();
            if (!result.success())
            {
                DebugLogger::log("FAILED to initialize service: %s", service.name);
#ifdef __WIIU__
                FILE* serviceErrorLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (serviceErrorLog) {
                    fprintf(serviceErrorLog, "FAILED TO INITIALIZE SERVICE: %s\n", service.name);
                    fflush(serviceErrorLog);
                    fclose(serviceErrorLog);
                }
#endif
                return -1;
            }
            DebugLogger::log("SUCCESS: Service %s initialized successfully", service.name);
            serviceCount++;
            
#ifdef __WIIU__
            FILE* serviceOkLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (serviceOkLog) {
                fprintf(serviceOkLog, "SUCCESS: Service %s initialized (%d/%zu)\n", service.name, serviceCount, services.size());
                fflush(serviceOkLog);
                fclose(serviceOkLog);
            }
#endif
        }

        DebugLogger::log("All %d services initialized successfully", serviceCount);

        // CEMU COMPATIBILITY: These functions cause problems in Cemu emulator
        // Commenting them out for better compatibility
        DebugLogger::log("Skipping WPAD functions for Cemu compatibility");
        // WPADEnableWiiRemote(true);
        // WPADEnableURCC(true);
        DebugLogger::log("WPAD functions skipped for Cemu compatibility");

        DebugLogger::log("About to set main core ID");
        Console::setMainCoreId(OSGetCoreId());
        DebugLogger::log("Main core ID set to: %u", OSGetCoreId());

        DebugLogger::log("=== PREINIT COMPLETED SUCCESSFULLY ===");
        
#ifdef __WIIU__
        FILE* completedLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (completedLog) {
            fprintf(completedLog, "=== PREINIT COMPLETED SUCCESSFULLY ===\n");
            fflush(completedLog);
            fclose(completedLog);
        }
#endif
        
        return 0;
    }

    // Enhanced crash screen: shows error type, message, function info, and traceback
    void drawCrashScreen(const char* error, const char* errorType, const char* functionInfo, const char* tb1, const char* tb2, const char* tb3) {
#ifdef __WIIU__
        static bool screenInited = false;
        if (!screenInited) {
            OSScreenInit();
            screenInited = true;
        }
        OSScreenID screens[2] = { SCREEN_TV, SCREEN_DRC };
        for (int i = 0; i < 2; ++i) {
            OSScreenID screen = screens[i];
            OSScreenClearBufferEx(screen, 0); // Black background
            int width = (screen == SCREEN_TV) ? 80 : 52;
            int currentLine = 5;
            
            // Title
            const char* title = "=== LUA CRASH DETECTED ===";
            int x = (width - (int)strlen(title)) / 2;
            OSScreenPutFontEx(screen, x, currentLine++, title);
            currentLine++; // blank line
            
            // Error type
            if (errorType && strlen(errorType) > 0) {
                char typeDisplay[100];
                snprintf(typeDisplay, sizeof(typeDisplay), "Type: %s", errorType);
                x = (width - (int)strlen(typeDisplay)) / 2;
                OSScreenPutFontEx(screen, x, currentLine++, typeDisplay);
            }
            
            // Error message
            if (error && strlen(error) > 0) {
                char truncatedError[width - 10];
                strncpy(truncatedError, error, sizeof(truncatedError) - 1);
                truncatedError[sizeof(truncatedError) - 1] = '\0';
                x = (width - (int)strlen(truncatedError)) / 2;
                OSScreenPutFontEx(screen, x, currentLine++, truncatedError);
            }
            currentLine++; // blank line
            
            // Function info
            if (functionInfo && strlen(functionInfo) > 0) {
                char truncatedFunc[width - 10];
                strncpy(truncatedFunc, functionInfo, sizeof(truncatedFunc) - 1);
                truncatedFunc[sizeof(truncatedFunc) - 1] = '\0';
                x = (width - (int)strlen(truncatedFunc)) / 2;
                OSScreenPutFontEx(screen, x, currentLine++, truncatedFunc);
                currentLine++; // blank line
            }
            
            // Traceback header
            const char* tbHeader = "Traceback:";
            x = (width - (int)strlen(tbHeader)) / 2;
            OSScreenPutFontEx(screen, x, currentLine++, tbHeader);
            
            // Traceback lines
            if (tb1 && strlen(tb1) > 0) {
                char truncatedTb1[width - 5];
                strncpy(truncatedTb1, tb1, sizeof(truncatedTb1) - 1);
                truncatedTb1[sizeof(truncatedTb1) - 1] = '\0';
                OSScreenPutFontEx(screen, 2, currentLine++, truncatedTb1);
            }
            if (tb2 && strlen(tb2) > 0) {
                char truncatedTb2[width - 5];
                strncpy(truncatedTb2, tb2, sizeof(truncatedTb2) - 1);
                truncatedTb2[sizeof(truncatedTb2) - 1] = '\0';
                OSScreenPutFontEx(screen, 2, currentLine++, truncatedTb2);
            }
            if (tb3 && strlen(tb3) > 0) {
                char truncatedTb3[width - 5];
                strncpy(truncatedTb3, tb3, sizeof(truncatedTb3) - 1);
                truncatedTb3[sizeof(truncatedTb3) - 1] = '\0';
                OSScreenPutFontEx(screen, 2, currentLine++, truncatedTb3);
            }
            
            currentLine += 2; // space before prompt
            
            // Instructions
            const char* prompt1 = "Check simple_debug.log for full details";
            const char* prompt2 = "Press B to exit...";
            x = (width - (int)strlen(prompt1)) / 2;
            OSScreenPutFontEx(screen, x, currentLine++, prompt1);
            x = (width - (int)strlen(prompt2)) / 2;
            OSScreenPutFontEx(screen, x, currentLine, prompt2);
            
            OSScreenFlipBuffersEx(screen);
        }
#endif
    }

    static bool isRunning()
    {
#ifdef __WIIU__
        FILE* logFile_start = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile_start) {
            fprintf(logFile_start, "isRunning(): Function entered\n");
            fflush(logFile_start);
            fclose(logFile_start);
        }
        
        FILE* logFile_main = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile_main) {
            fprintf(logFile_main, "isRunning(): About to call OSGetMainCoreId()\n");
            fflush(logFile_main);
            fclose(logFile_main);
        }
#endif

        uint32_t mainCoreId = OSGetMainCoreId();
        
#ifdef __WIIU__
        FILE* logFile_core = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile_core) {
            fprintf(logFile_core, "isRunning(): OSGetMainCoreId() returned %u\n", mainCoreId);
            fflush(logFile_core);
            fclose(logFile_core);
        }
        
        FILE* logFile_check = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile_check) {
            fprintf(logFile_check, "isRunning(): About to call Console::isMainCoreId()\n");
            fflush(logFile_check);
            fclose(logFile_check);
        }
#endif

        if (!Console::isMainCoreId(mainCoreId))
        {
#ifdef __WIIU__
            FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile) {
                fprintf(logFile, "isRunning(): Not main core, calling ProcUISubProcessMessages(false)\n");
                fflush(logFile);
                fclose(logFile);
            }
#endif
            ProcUISubProcessMessages(false);
            return true;
        }

        const auto status = ProcUIProcessMessages(false);

#ifdef __WIIU__
        FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile2) {
            fprintf(logFile2, "isRunning(): ProcUIProcessMessages(false) returned status: %d\n", status);
            fflush(logFile2);
            fclose(logFile2);
        }
#endif

        switch (status)
        {
            case PROCUI_STATUS_IN_FOREGROUND:
#ifdef __WIIU__
                {
                    FILE* logFile3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                    if (logFile3) {
                        fprintf(logFile3, "isRunning(): PROCUI_STATUS_IN_FOREGROUND - sending focus(true)\n");
                        fflush(logFile3);
                        fclose(logFile3);
                    }
                }
#endif
                EventQueue::getInstance().sendFocus(true);
                break;
            case PROCUI_STATUS_RELEASE_FOREGROUND:
#ifdef __WIIU__
                {
                    FILE* logFile4 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                    if (logFile4) {
                        fprintf(logFile4, "isRunning(): PROCUI_STATUS_RELEASE_FOREGROUND - sending focus(false)\n");
                        fflush(logFile4);
                        fclose(logFile4);
                    }
                }
#endif
                EventQueue::getInstance().sendFocus(false);
                ProcUIDrawDoneRelease();
                break;
            case PROCUI_STATUS_EXITING:
#ifdef __WIIU__
                {
                    FILE* logFile5 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                    if (logFile5) {
                        fprintf(logFile5, "isRunning(): PROCUI_STATUS_EXITING - returning false!\n");
                        fflush(logFile5);
                        fclose(logFile5);
                    }
                }
#endif
                EventQueue::getInstance().sendQuit();
                return false;
            default:
#ifdef __WIIU__
                {
                    FILE* logFile6 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                    if (logFile6) {
                        fprintf(logFile6, "isRunning(): Unknown ProcUI status: %d (falling through to return true)\n", status);
                        fflush(logFile6);
                        fclose(logFile6);
                    }
                }
#endif
                break;
        }

#ifdef __WIIU__
        FILE* logFile7 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile7) {
            fprintf(logFile7, "isRunning(): returning true\n");
            fflush(logFile7);
            fclose(logFile7);
        }
#endif

        return true;
    }

    static bool s_Shutdown = false;

    // Helper to check for user-defined love.run and log which is used
    void logWhichLoveRun(lua_State* L) {
        lua_getglobal(L, "love");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "run");
            if (lua_isfunction(L, -1)) {
                DebugLogger::log("User-defined love.run detected: using user main loop");
#ifdef __WIIU__
                FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (logFile) {
                    fprintf(logFile, "User-defined love.run detected: using user main loop\n");
                    fflush(logFile);
                    fclose(logFile);
                }
#endif
            } else {
                DebugLogger::log("No user-defined love.run: using default main loop");
#ifdef __WIIU__
                FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (logFile) {
                    fprintf(logFile, "No user-defined love.run: using default main loop\n");
                    fflush(logFile);
                    fclose(logFile);
                }
#endif
            }
            lua_pop(L, 1); // pop love.run
        } else {
            DebugLogger::log("No global 'love' table found in Lua state");
        }
        lua_pop(L, 1); // pop love
    }

    bool mainLoop(lua_State* L, int argc, int* nres)
    {
        static bool checkedLoveRun = false;
        if (!checkedLoveRun) {
            logWhichLoveRun(L);
            checkedLoveRun = true;
        }
        DebugLogger::log("mainLoop() called with argc=%d, shutdown=%s", argc, s_Shutdown ? "true" : "false");
        
#ifdef __WIIU__
        // Additional simple logging for debugging freeze
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "mainLoop() entered, about to check shutdown\n");
            fflush(logFile);
            fclose(logFile);
        }
#endif
        
        // Check if running first before anything else
#ifdef __WIIU__
        FILE* logFile_pre = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile_pre) {
            fprintf(logFile_pre, "About to call isRunning()...\n");
            fflush(logFile_pre);
            fclose(logFile_pre);
        }
#endif
        
        bool running = true; // BYPASS isRunning() - force true
        
#ifdef __WIIU__
        FILE* logFile0 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile0) {
            fprintf(logFile0, "BYPASSED isRunning() - forced to: %s\n", running ? "true" : "false");
            fflush(logFile0);
            fclose(logFile0);
        }
        
        // Add detailed state tracking
        static int mainLoopCallCount = 0;
        mainLoopCallCount++;
        FILE* stateLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (stateLog) {
            fprintf(stateLog, "mainLoop() call #%d - L=%p, Lua stack top: %d\n", 
                    mainLoopCallCount, (void*)L, lua_gettop(L));
            fflush(stateLog);
            fclose(stateLog);
        }
#endif
        
        if (!running)
        {
#ifdef __WIIU__
            FILE* logFileExit = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFileExit) {
                fprintf(logFileExit, "isRunning() is false, returning false from mainLoop()\n");
                fflush(logFileExit);
                fclose(logFileExit);
            }
#endif
            return false;
        }
        
        if (!s_Shutdown)
        {
#ifdef __WIIU__
            FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile2) {
                fprintf(logFile2, "Not shutdown, about to call luax_resume()\n");
                fflush(logFile2);
                fclose(logFile2);
            }
#endif
            DebugLogger::log("About to call luax_resume()...");
            
            // Pre-execution stack analysis
            int preStackTop = lua_gettop(L);
            DebugLogger::log("Pre-execution stack analysis: %d items", preStackTop);
            
#ifdef __WIIU__
            FILE* preLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (preLog) {
                fprintf(preLog, "=== PRE-EXECUTION STACK ANALYSIS ===\n");
                fprintf(preLog, "Stack has %d items before luax_resume()\n", preStackTop);
                
                for (int i = 1; i <= preStackTop; i++) {
                    int type = lua_type(L, i);
                    const char* typeName = lua_typename(L, type);
                    fprintf(preLog, "Pre-Stack[%d]: %s", i, typeName);
                    
                    if (type == LUA_TSTRING) {
                        const char* str = lua_tostring(L, i);
                        if (str) fprintf(preLog, " = \"%s\"", str);
                    } else if (type == LUA_TNUMBER) {
                        double num = lua_tonumber(L, i);
                        fprintf(preLog, " = %g", num);
                    } else if (type == LUA_TBOOLEAN) {
                        int b = lua_toboolean(L, i);
                        fprintf(preLog, " = %s", b ? "true" : "false");
                    } else if (type == LUA_TFUNCTION) {
                        lua_Debug ar;
                        lua_pushvalue(L, i); // Copy function to stack top
                        if (lua_getinfo(L, ">n", &ar)) {
                            fprintf(preLog, " (function: %s)", ar.name ? ar.name : "anonymous");
                        }
                    }
                    fprintf(preLog, "\n");
                }
                
                // Check what's at the top of the stack (what we're trying to resume)
                if (preStackTop > 0) {
                    int topType = lua_type(L, preStackTop);
                    fprintf(preLog, "Top of stack (resume target): %s\n", lua_typename(L, topType));
                    
                    // CRITICAL: Check if we're trying to resume a non-thread
                    if (topType != LUA_TTHREAD) {
                        fprintf(preLog, "ERROR: Attempting to resume a %s instead of a thread!\n", lua_typename(L, topType));
                        fprintf(preLog, "This will cause 'attempt to call a boolean value' error.\n");
                        
                        // Look for the actual thread in the stack
                        for (int k = 1; k <= preStackTop; k++) {
                            if (lua_type(L, k) == LUA_TTHREAD) {
                                fprintf(preLog, "Found thread at stack position %d\n", k);
                                lua_State* foundThread = lua_tothread(L, k);
                                if (foundThread) {
                                    int threadStatus = lua_status(foundThread);
                                    fprintf(preLog, "Thread status: %d\n", threadStatus);
                                }
                                break;
                            }
                        }
                    } else {
                        // It's a thread, analyze it
                        lua_State* thread = lua_tothread(L, preStackTop);
                        if (thread) {
                            int status = lua_status(thread);
                            switch (status) {
                                case 0: fprintf(preLog, "OK/SUSPENDED"); break;
                                case 1: fprintf(preLog, "YIELDED"); break; // LUA_YIELD
                                case LUA_ERRRUN: fprintf(preLog, "RUNTIME_ERROR"); break;
                                case LUA_ERRSYNTAX: fprintf(preLog, "SYNTAX_ERROR"); break;
                                case LUA_ERRMEM: fprintf(preLog, "MEMORY_ERROR"); break;
                                case LUA_ERRERR: fprintf(preLog, "ERROR_HANDLER_ERROR"); break;
                                default: fprintf(preLog, "UNKNOWN(%d)", status); break;
                            }
                            fprintf(preLog, "\n");
                            
                            // Check thread stack
                            int threadStackTop = lua_gettop(thread);
                            fprintf(preLog, "Thread stack has %d items\n", threadStackTop);
                            for (int j = 1; j <= threadStackTop && j <= 5; j++) {
                                int threadType = lua_type(thread, j);
                                fprintf(preLog, "  Thread[%d]: %s", j, lua_typename(thread, threadType));
                                if (threadType == LUA_TBOOLEAN) {
                                    int b = lua_toboolean(thread, j);
                                    fprintf(preLog, " = %s", b ? "true" : "false");
                                }
                                fprintf(preLog, "\n");
                            }
                        }
                    }
                }
                
                fprintf(preLog, "=== END PRE-EXECUTION ANALYSIS ===\n");
                fflush(preLog);
                fclose(preLog);
            }
#endif
            
            // SAFETY CHECK: Verify we're resuming a thread, not a boolean
            if (preStackTop > 0) {
                int topType = lua_type(L, preStackTop);
                if (topType != LUA_TTHREAD) {
                    DebugLogger::log("CRITICAL ERROR: Attempting to resume %s instead of thread!", lua_typename(L, topType));
                    
#ifdef __WIIU__
                    FILE* errorLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                    if (errorLog) {
                        fprintf(errorLog, "=== CRITICAL STACK ERROR DETECTED ===\n");
                        fprintf(errorLog, "Attempting to resume %s instead of thread!\n", lua_typename(L, topType));
                        fprintf(errorLog, "This would cause 'attempt to call a boolean value' error.\n");
                        
                        // Show the problematic stack
                        for (int i = 1; i <= preStackTop; i++) {
                            int type = lua_type(L, i);
                            fprintf(errorLog, "Stack[%d]: %s", i, lua_typename(L, type));
                            if (type == LUA_TBOOLEAN) {
                                int b = lua_toboolean(L, i);
                                fprintf(errorLog, " = %s", b ? "true" : "false");
                            }
                            fprintf(errorLog, "\n");
                        }
                        
                        fprintf(errorLog, "=== STACK ERROR PREVENTION ===\n");
                        fflush(errorLog);
                        fclose(errorLog);
                    }
#endif
                    
                    // Emergency exit instead of crashing
                    DebugLogger::log("Emergency exit to prevent Lua crash");
                    SYSLaunchMenu();
                    s_Shutdown = true;
                    return false;
                }
            }
            
            const auto resumeResult = luax_resume(L, argc, nres);
#ifdef __WIIU__
            FILE* logFile3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile3) {
                fprintf(logFile3, "luax_resume() returned, result=%d\n", resumeResult);
                fflush(logFile3);
                fclose(logFile3);
            }
#endif
            DebugLogger::log("luax_resume() returned %d", resumeResult);
            
            const auto yielding = (resumeResult == 1); // LUA_YIELD
            DebugLogger::log("yielding = %s", yielding ? "true" : "false");

            if (!yielding)
            {
                DebugLogger::log("=== CRITICAL: LUA THREAD COMPLETED/ERROR ===");
                DebugLogger::log("luax_resume returned %d (not yielding)", resumeResult);
                
#ifdef __WIIU__
                FILE* criticalLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (criticalLog) {
                    fprintf(criticalLog, "=== CRITICAL: LUA THREAD COMPLETED/ERROR ===\n");
                    fprintf(criticalLog, "luax_resume returned %d (not yielding)\n", resumeResult);
                    
                    if (resumeResult == 0) {
                        fprintf(criticalLog, "LUA_OK: The main loop function COMPLETED instead of yielding!\n");
                        fprintf(criticalLog, "This is why Lua execution stops after loading.\n");
                        fprintf(criticalLog, "The love.run() function should yield continuously, not return.\n");
                    } else {
                        fprintf(criticalLog, "Lua error occurred: %d\n", resumeResult);
                    }
                    
                    fflush(criticalLog);
                    fclose(criticalLog);
                }
#endif
                DebugLogger::log("=== THREAD COMPLETED ANALYSIS ===");
                DebugLogger::log("Resume result: %d (0=LUA_OK, 1=LUA_YIELD, 2=LUA_ERRRUN)", resumeResult);
                
                if (resumeResult == 0) { // LUA_OK
                    DebugLogger::log("DIAGNOSIS: Lua thread completed normally (returned)");
                    DebugLogger::log("This means the boot function finished executing and returned a value");
                    DebugLogger::log("Expected: Thread should yield indefinitely, not complete");
                    DebugLogger::log("CAUSE: The main loop in boot.lua exited instead of yielding");
                }
                
                // FALLBACK LOOP: When Lua stops, show a diagnostic black screen with text
                DebugLogger::log("=== ENTERING C++ FALLBACK LOOP ===");
                DebugLogger::log("Lua has stopped running. Displaying diagnostic screen.");
                
#ifdef __WIIU__
                FILE* fallbackLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (fallbackLog) {
                    fprintf(fallbackLog, "=== ENTERING C++ FALLBACK LOOP ===\n");
                    fprintf(fallbackLog, "Displaying black screen with diagnostic text\n");
                    fflush(fallbackLog);
                    fclose(fallbackLog);
                }
                
                // Initialize OSScreen for fallback display
                static bool fallbackScreenInited = false;
                if (!fallbackScreenInited) {
                    OSScreenInit();
                    fallbackScreenInited = true;
                }
                
                // Fallback loop variables
                VPADStatus vpad;
                KPADStatus kpad;
                bool fallbackRunning = true;
                int fallbackFrameCount = 0;
                
                while (fallbackRunning && isRunning()) {
                    // Handle input
                    VPADRead(VPAD_CHAN_0, &vpad, 1, NULL);
                    KPADRead(WPAD_CHAN_0, &kpad, 1);
                    
                    // Exit fallback loop if HOME button is pressed
                    if ((vpad.hold & VPAD_BUTTON_HOME) || (kpad.hold & WPAD_BUTTON_HOME)) {
                        fallbackRunning = false;
                        s_Shutdown = true;
                        break;
                    }
                    
                    // Draw diagnostic screen on both TV and GamePad
                    OSScreenID screens[2] = { SCREEN_TV, SCREEN_DRC };
                    for (int i = 0; i < 2; ++i) {
                        OSScreenID screen = screens[i];
                        OSScreenClearBufferEx(screen, 0); // Black background
                        
                        int width = (screen == SCREEN_TV) ? 80 : 52;
                        int currentLine = 5;
                        
                        // Title
                        const char* title = "=== LOVE POTION DIAGNOSTIC ===";
                        int x = (width - (int)strlen(title)) / 2;
                        OSScreenPutFontEx(screen, x, currentLine++, title);
                        currentLine++; // blank line
                        
                        // Status message
                        const char* status = "Lua main loop has stopped running";
                        x = (width - (int)strlen(status)) / 2;
                        OSScreenPutFontEx(screen, x, currentLine++, status);
                        currentLine++; // blank line
                        
                        // Reason for stopping
                        if (resumeResult == 0) {
                            const char* reason = "Reason: Lua thread completed (did not yield)";
                            x = (width - (int)strlen(reason)) / 2;
                            OSScreenPutFontEx(screen, x, currentLine++, reason);
                            
                            const char* solution = "Solution: Check love.run() implementation";
                            x = (width - (int)strlen(solution)) / 2;
                            OSScreenPutFontEx(screen, x, currentLine++, solution);
                        } else {
                            const char* reason = "Reason: Lua error occurred";
                            x = (width - (int)strlen(reason)) / 2;
                            OSScreenPutFontEx(screen, x, currentLine++, reason);
                            
                            char errorCode[50];
                            snprintf(errorCode, sizeof(errorCode), "Error code: %d", resumeResult);
                            x = (width - (int)strlen(errorCode)) / 2;
                            OSScreenPutFontEx(screen, x, currentLine++, errorCode);
                        }
                        
                        currentLine += 2; // space
                        
                        // Debug info
                        char frameInfo[50];
                        snprintf(frameInfo, sizeof(frameInfo), "Fallback frame: %d", fallbackFrameCount);
                        x = (width - (int)strlen(frameInfo)) / 2;
                        OSScreenPutFontEx(screen, x, currentLine++, frameInfo);
                        
                        currentLine += 2; // space
                        
                        // Instructions
                        const char* instruction1 = "Check simple_debug.log for details";
                        const char* instruction2 = "Press HOME to exit";
                        x = (width - (int)strlen(instruction1)) / 2;
                        OSScreenPutFontEx(screen, x, currentLine++, instruction1);
                        x = (width - (int)strlen(instruction2)) / 2;
                        OSScreenPutFontEx(screen, x, currentLine, instruction2);
                        
                        OSScreenFlipBuffersEx(screen);
                    }
                    
                    fallbackFrameCount++;
                    
                    // Log every 60 frames
                    if (fallbackFrameCount % 60 == 0) {
                        FILE* fallbackFrameLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                        if (fallbackFrameLog) {
                            fprintf(fallbackFrameLog, "Fallback loop frame %d\n", fallbackFrameCount);
                            fflush(fallbackFrameLog);
                            fclose(fallbackFrameLog);
                        }
                    }
                    
                    // Small delay to prevent excessive CPU usage
                    OSSleepTicks(OSMillisecondsToTicks(16)); // ~60 FPS
                }
                
                DebugLogger::log("=== EXITING C++ FALLBACK LOOP ===");
                
                FILE* exitLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (exitLog) {
                    fprintf(exitLog, "=== EXITING C++ FALLBACK LOOP ===\n");
                    fprintf(exitLog, "Fallback loop ran for %d frames\n", fallbackFrameCount);
                    fflush(exitLog);
                    fclose(exitLog);
                }
#endif
                // Check if there was a Lua error
                if (resumeResult != 0) // LUA_OK is 0, any non-zero value is an error
                {
                    const char* errorMsg = lua_tostring(L, -1);
                    std::string errorText = errorMsg ? errorMsg : "Unknown Lua error";
                    std::string traceback;
                    std::string fullStackDump;
                    
                    // Get detailed error type information
                    const char* errorTypeStr = "UNKNOWN";
                    switch (resumeResult) {
                        case LUA_ERRRUN: errorTypeStr = "RUNTIME_ERROR"; break;
                        case LUA_ERRMEM: errorTypeStr = "MEMORY_ERROR"; break;
                        case LUA_ERRERR: errorTypeStr = "ERROR_HANDLER_ERROR"; break;
                        case LUA_ERRSYNTAX: errorTypeStr = "SYNTAX_ERROR"; break;
                        case 1: errorTypeStr = "YIELD"; break; // LUA_YIELD
                        default: errorTypeStr = "OTHER_ERROR"; break;
                    }
                    
                    // Get full stack information
                    int stackTop = lua_gettop(L);
                    char stackInfo[512];
                    snprintf(stackInfo, sizeof(stackInfo), "Stack top: %d", stackTop);
                    fullStackDump = stackInfo;
                    
                    // Get enhanced traceback with more detail
                    lua_getglobal(L, "debug");
                    if (lua_istable(L, -1))
                    {
                        lua_getfield(L, -1, "traceback");
                        if (lua_isfunction(L, -1))
                        {
                            // Get detailed traceback starting from level 0
                            lua_pushstring(L, errorText.c_str());
                            lua_pushinteger(L, 0); // Start from level 0 to get more detail
                            lua_call(L, 2, 1);
                            
                            const char* tb = lua_tostring(L, -1);
                            if (tb) traceback = tb;
                            lua_pop(L, 1); // pop traceback result
                        }
                        else
                        {
                            lua_pop(L, 1); // pop non-function
                        }
                        lua_pop(L, 1); // pop debug table
                    }
                    else
                    {
                        lua_pop(L, 1); // pop non-table
                    }
                    
                    // Get additional stack frame information
                    std::string stackFrames;
                    for (int level = 0; level < 10; level++) {
                        lua_Debug ar;
                        if (lua_getstack(L, level, &ar)) {
                            if (lua_getinfo(L, "Slnt", &ar)) {
                                char frameInfo[512];
                                snprintf(frameInfo, sizeof(frameInfo), 
                                        "Frame[%d]: %s:%d in function '%s' (what=%s, namewhat=%s)\n",
                                        level,
                                        ar.source ? ar.source : "?",
                                        ar.currentline,
                                        ar.name ? ar.name : "?",
                                        ar.what ? ar.what : "?",
                                        ar.namewhat ? ar.namewhat : "?");
                                stackFrames += frameInfo;
                            }
                        } else {
                            break; // No more stack frames
                        }
                    }
                    
                    // Also check if we have a thread and analyze its stack
                    std::string threadAnalysis;
                    for (int i = 1; i <= stackTop; i++) {
                        if (lua_type(L, i) == LUA_TTHREAD) {
                            lua_State* thread = lua_tothread(L, i);
                            if (thread) {
                                char threadInfo[1024];
                                snprintf(threadInfo, sizeof(threadInfo), "Thread[%d] analysis:\n", i);
                                threadAnalysis += threadInfo;
                                
                                int threadStackTop = lua_gettop(thread);
                                snprintf(threadInfo, sizeof(threadInfo), "  Thread stack: %d items\n", threadStackTop);
                                threadAnalysis += threadInfo;
                                
                                // Check thread stack frames
                                for (int level = 0; level < 5; level++) {
                                    lua_Debug threadAr;
                                    if (lua_getstack(thread, level, &threadAr)) {
                                        if (lua_getinfo(thread, "Slnt", &threadAr)) {
                                            snprintf(threadInfo, sizeof(threadInfo),
                                                    "  ThreadFrame[%d]: %s:%d in '%s' (%s %s)\n",
                                                    level,
                                                    threadAr.source ? threadAr.source : "?",
                                                    threadAr.currentline,
                                                    threadAr.name ? threadAr.name : "?",
                                                    threadAr.what ? threadAr.what : "?",
                                                    threadAr.namewhat ? threadAr.namewhat : "?");
                                            threadAnalysis += threadInfo;
                                        }
                                    } else {
                                        break;
                                    }
                                }
                                
                                // Check what's on thread stack that might be the boolean
                                for (int j = 1; j <= threadStackTop && j <= 10; j++) {
                                    int threadType = lua_type(thread, j);
                                    snprintf(threadInfo, sizeof(threadInfo), "  ThreadStack[%d]: %s", j, lua_typename(thread, threadType));
                                    if (threadType == LUA_TBOOLEAN) {
                                        int b = lua_toboolean(thread, j);
                                        snprintf(threadInfo + strlen(threadInfo), sizeof(threadInfo) - strlen(threadInfo), " = %s", b ? "true" : "false");
                                    } else if (threadType == LUA_TSTRING) {
                                        const char* str = lua_tostring(thread, j);
                                        if (str && strlen(str) < 50) {
                                            snprintf(threadInfo + strlen(threadInfo), sizeof(threadInfo) - strlen(threadInfo), " = \"%s\"", str);
                                        }
                                    }
                                    threadAnalysis += threadInfo;
                                    threadAnalysis += "\n";
                                }
                            }
                        }
                    }
                    
                    // Get current function info with more detail
                    std::string currentFunction = "unknown";
                    lua_Debug ar;
                    if (lua_getstack(L, 1, &ar)) { // Try level 1 first for the actual error location
                        if (lua_getinfo(L, "Slnt", &ar)) {
                            char funcInfo[512];
                            snprintf(funcInfo, sizeof(funcInfo), "%s:%d in %s (%s %s)", 
                                    ar.source ? ar.source : "?", 
                                    ar.currentline,
                                    ar.name ? ar.name : "anonymous",
                                    ar.what ? ar.what : "?",
                                    ar.namewhat ? ar.namewhat : "");
                            currentFunction = funcInfo;
                        }
                    } else if (lua_getstack(L, 0, &ar)) { // Fallback to level 0
                        if (lua_getinfo(L, "Slnt", &ar)) {
                            char funcInfo[512];
                            snprintf(funcInfo, sizeof(funcInfo), "%s:%d in %s (%s %s)", 
                                    ar.source ? ar.source : "?", 
                                    ar.currentline,
                                    ar.name ? ar.name : "anonymous",
                                    ar.what ? ar.what : "?",
                                    ar.namewhat ? ar.namewhat : "");
                            currentFunction = funcInfo;
                        }
                    }
                    
                    DebugLogger::log("=== ENHANCED LUA ERROR REPORT ===");
                    DebugLogger::log("Error Type: %s (code: %d)", errorTypeStr, resumeResult);
                    DebugLogger::log("Error Message: %s", errorText.c_str());
                    DebugLogger::log("Current Function: %s", currentFunction.c_str());
                    DebugLogger::log("Stack Info: %s", fullStackDump.c_str());
                    
                    // ENHANCED: Write to console (stderr) and stdout for maximum visibility
                    fprintf(stderr, "\n=== LUA ERROR DETECTED ===\n");
                    fprintf(stderr, "Error Type: %s (code: %d)\n", errorTypeStr, resumeResult);
                    fprintf(stderr, "Error Message: %s\n", errorText.c_str());
                    fprintf(stderr, "Current Function: %s\n", currentFunction.c_str());
                    fprintf(stderr, "Stack Info: %s\n", fullStackDump.c_str());
                    fflush(stderr);
                    
                    // Print to stdout as well for console visibility
                    printf("\n=== LUA ERROR DETECTED ===\n");
                    printf("Error Type: %s (code: %d)\n", errorTypeStr, resumeResult);
                    printf("Error Message: %s\n", errorText.c_str());
                    printf("Current Function: %s\n", currentFunction.c_str());
                    printf("Stack Info: %s\n", fullStackDump.c_str());
                    fflush(stdout);
                    
                    // Also write to a simple text file for easy reading
                    FILE* errorFile = fopen("/vol/external01/wiiu/apps/balatro/lua_error.txt", "w");
                    if (errorFile) {
                        fprintf(errorFile, "=== LUA ERROR REPORT ===\n");
                        fprintf(errorFile, "Error Type: %s (code: %d)\n", errorTypeStr, resumeResult);
                        fprintf(errorFile, "Error Message: %s\n", errorText.c_str());
                        fprintf(errorFile, "Current Function: %s\n", currentFunction.c_str());
                        fprintf(errorFile, "Stack Info: %s\n", fullStackDump.c_str());
                        fclose(errorFile);
                    }
                    
                    // Special analysis for "attempt to call a boolean value" error
                    if (strstr(errorText.c_str(), "attempt to call a boolean value")) {
                        DebugLogger::log("DIAGNOSIS: This error typically occurs when:");
                        DebugLogger::log("1. A boolean value is being called as a function");
                        DebugLogger::log("2. Wrong stack item is being resumed (boolean instead of thread)");
                        DebugLogger::log("3. Function was overwritten with boolean value");
                        DebugLogger::log("4. Module loading issue where function becomes boolean");
                        
                        printf("DIAGNOSIS: This error typically occurs when:\n");
                        printf("1. A boolean value is being called as a function\n");
                        printf("2. Wrong stack item is being resumed (boolean instead of thread)\n");
                        printf("3. Function was overwritten with boolean value\n");
                        printf("4. Module loading issue where function becomes boolean\n");
                    }
                    
                    // Special analysis for "attempt to index field" error
                    if (strstr(errorText.c_str(), "attempt to index field")) {
                        DebugLogger::log("DIAGNOSIS: This error typically occurs when:");
                        DebugLogger::log("1. Trying to access a field on a nil value");
                        DebugLogger::log("2. Missing table or object initialization");
                        DebugLogger::log("3. Shader or resource not loaded properly");
                        
                        printf("DIAGNOSIS: This error typically occurs when:\n");
                        printf("1. Trying to access a field on a nil value\n");
                        printf("2. Missing table or object initialization\n");
                        printf("3. Shader or resource not loaded properly\n");
                    }
                    
                    if (!stackFrames.empty()) {
                        DebugLogger::log("Stack Frames:\n%s", stackFrames.c_str());
                        printf("Stack Frames:\n%s\n", stackFrames.c_str());
                    }
                    if (!threadAnalysis.empty()) {
                        DebugLogger::log("Thread Analysis:\n%s", threadAnalysis.c_str());
                        printf("Thread Analysis:\n%s\n", threadAnalysis.c_str());
                    }
                    if (!traceback.empty()) {
                        DebugLogger::log("Full Traceback: %s", traceback.c_str());
                        printf("Full Traceback: %s\n", traceback.c_str());
                    }
                    
#ifdef __WIIU__
                    // Log to simple debug file with enhanced information
                    FILE* crashLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                    if (crashLog) {
                        fprintf(crashLog, "=== ENHANCED LUA CRASH REPORT ===\n");
                        fprintf(crashLog, "Error Type: %s (code: %d)\n", errorTypeStr, resumeResult);
                        fprintf(crashLog, "Error Message: %s\n", errorText.c_str());
                        fprintf(crashLog, "Current Function: %s\n", currentFunction.c_str());
                        fprintf(crashLog, "Stack Info: %s\n", fullStackDump.c_str());
                        
                        // Special diagnosis for boolean call error
                        if (strstr(errorText.c_str(), "attempt to call a boolean value")) {
                            fprintf(crashLog, "=== BOOLEAN CALL ERROR DIAGNOSIS ===\n");
                            fprintf(crashLog, "This error occurs when:\n");
                            fprintf(crashLog, "1. A boolean value is called as function: someBoolean()\n");
                            fprintf(crashLog, "2. Wrong stack item resumed (boolean instead of thread)\n");
                            fprintf(crashLog, "3. Function overwritten with boolean value\n");
                            fprintf(crashLog, "4. Module loading issue (function -> boolean)\n");
                            fprintf(crashLog, "5. Incorrect stack manipulation in C code\n");
                            fprintf(crashLog, "=== END DIAGNOSIS ===\n");
                        }
                        
                        // Log detailed stack contents
                        fprintf(crashLog, "=== STACK CONTENTS ===\n");
                        for (int i = 1; i <= stackTop; i++) {
                            int type = lua_type(L, i);
                            const char* typeName = lua_typename(L, type);
                            fprintf(crashLog, "Stack[%d]: %s", i, typeName);
                            
                            if (type == LUA_TSTRING) {
                                const char* str = lua_tostring(L, i);
                                if (str) fprintf(crashLog, " = \"%s\"", str);
                            } else if (type == LUA_TNUMBER) {
                                double num = lua_tonumber(L, i);
                                fprintf(crashLog, " = %g", num);
                            } else if (type == LUA_TBOOLEAN) {
                                int b = lua_toboolean(L, i);
                                fprintf(crashLog, " = %s", b ? "true" : "false");
                            }
                            fprintf(crashLog, "\n");
                        }
                        
                        // Log detailed stack frames
                        if (!stackFrames.empty()) {
                            fprintf(crashLog, "=== STACK FRAMES ===\n");
                            fprintf(crashLog, "%s", stackFrames.c_str());
                        }
                        
                        // Log thread analysis
                        if (!threadAnalysis.empty()) {
                            fprintf(crashLog, "=== THREAD ANALYSIS ===\n");
                            fprintf(crashLog, "%s", threadAnalysis.c_str());
                        }
                        
                        if (!traceback.empty()) {
                            fprintf(crashLog, "=== FULL TRACEBACK ===\n");
                            fprintf(crashLog, "%s\n", traceback.c_str());
                        }
                        
                        // Additional debug information
                        fprintf(crashLog, "=== ENVIRONMENT INFO ===\n");
                        fprintf(crashLog, "Lua Version: %s\n", LUA_VERSION);
                        fprintf(crashLog, "Lua Stack Top: %d items\n", stackTop);
                        fprintf(crashLog, "Memory Usage: %d KB\n", lua_gc(L, LUA_GCCOUNT, 0));
                        
                        fprintf(crashLog, "=== END ENHANCED CRASH REPORT ===\n\n");
                        fflush(crashLog);
                        fclose(crashLog);
                    }
                    
                    // Show enhanced crash screen and wait for B button
                    bool waiting = true;
                    VPADStatus vpad;
                    KPADStatus kpad;
                    
                    // Parse traceback into multiple lines for display
                    std::string tb1, tb2, tb3;
                    if (!traceback.empty()) {
                        std::vector<std::string> lines;
                        size_t start = 0;
                        size_t pos = 0;
                        while ((pos = traceback.find('\n', start)) != std::string::npos) {
                            std::string line = traceback.substr(start, pos - start);
                            if (!line.empty() && line != "\t") {
                                lines.push_back(line);
                            }
                            start = pos + 1;
                        }
                        if (start < traceback.length()) {
                            std::string line = traceback.substr(start);
                            if (!line.empty() && line != "\t") {
                                lines.push_back(line);
                            }
                        }
                        
                        if (lines.size() > 0) tb1 = lines[0];
                        if (lines.size() > 1) tb2 = lines[1];
                        if (lines.size() > 2) tb3 = lines[2];
                    }
                    
                    while (waiting) {
                        drawCrashScreen(errorText.c_str(), errorTypeStr, currentFunction.c_str(), 
                                      tb1.c_str(), tb2.c_str(), tb3.c_str());
                        VPADRead(VPAD_CHAN_0, &vpad, 1, NULL);
                        KPADRead(WPAD_CHAN_0, &kpad, 1);
                        if ((vpad.hold & VPAD_BUTTON_B) || (kpad.hold & WPAD_BUTTON_B)) {
                            waiting = false;
                        }
                        OSSleepTicks(OSMillisecondsToTicks(50));
                    }
#endif
                }
                
                // IMPORTANT: After a Lua error, let the error handler take complete control
                // Set shutdown flag to stop the C++ main loop from interfering
                DebugLogger::log("Lua error detected - stopping C++ main loop to let love.errhand take over");
                DebugLogger::log("Lua error result: %d", resumeResult);
                DebugLogger::log("Current Lua stack size: %d", lua_gettop(L));
                
                // The error handler (love.errhand) will now handle the error display and user interaction
                // We need to stop the C++ main loop to prevent interference with error handler rendering
                s_Shutdown = true;
                
                // Log the transition
                DebugLogger::log("C++ main loop stopped - love.errhand should now have control");
            }
        }

        DebugLogger::log("mainLoop() returning %s", running ? "true" : "false");
        return running;
    }

    void onExit()
    {
        DebugLogger::log("onExit() called - shutting down services");
        
        for (auto it = services.rbegin(); it != services.rend(); ++it)
        {
            DebugLogger::log("Shutting down service: %s", it->name);
            it->exit();
        }
        
        DebugLogger::log("All services shut down");
        DebugLogger::close();
    }
} // namespace love

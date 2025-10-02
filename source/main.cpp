#include "common/luax.hpp"
#include "common/version.hpp"

#include "boot.hpp"
#include "modules/love/love.hpp"

#ifdef __WIIU__
#include "DebugLogger.hpp"
#include "driver/EventQueue.hpp"
#include "common/LoadingScreen.hpp"
#include <coreinit/time.h>
#include <coreinit/thread.h>
#ifdef USE_PPC_DEBUGGER
#include "common/PPCDebugger.hpp"
#endif
#include <cstdio>
#include <ctime>
#endif

#include <filesystem>
#include <vector>

extern "C"
{
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "common/Exception.hpp"

#ifdef __WIIU__
// Simple logging functions using basic C file I/O
void simpleLog(const char* message) {
    FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    
    if (logFile != nullptr) {
        time_t rawtime;
        struct tm* timeinfo;
        char timeStr[80];
        
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", timeinfo);
        
        fprintf(logFile, "[%s] %s\n", timeStr, message);
        fflush(logFile);
        fclose(logFile);
    }
    
    // Also log to DebugLogger (it has its own fallbacks)
    // love::DebugLogger::log("SIMPLE: %s", message);  // Temporarily disabled
}

void initSimpleLog() {
    FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "w");
    
    if (logFile != nullptr) {
        fprintf(logFile, "=== LOVE Potion Simple Debug Log ===\n");
        fprintf(logFile, "Log started\n");
        fprintf(logFile, "=====================================\n");
        fflush(logFile);
        fclose(logFile);
    }
}
#endif

enum DoneAction
{
    DONE_QUIT,
    DONE_RESTART
};

static DoneAction runLove(char** argv, int argc, int& result, love::Variant& restartValue)
{
#ifdef __WIIU__
    char buffer[256];
    sprintf(buffer, "runLove() called with argc=%d", argc);
    simpleLog(buffer);
    
    // Initialize loading screen first
    simpleLog("Initializing loading screen...");
    initLoadingScreen();
    updateLoadingScreen(0.1f, "Starting LOVE Potion...");
    
    simpleLog("Creating Lua state...");
#endif

    lua_State* L = luaL_newstate();
#ifdef __WIIU__
    updateLoadingScreen(0.2f, "Lua state created, opening libraries...");
    simpleLog("Lua state created, opening libs...");
#endif
    luaL_openlibs(L);
    luaopen_bit(L);

#ifdef __WIIU__
    updateLoadingScreen(0.3f, "Libraries loaded, preloading LOVE...");
    simpleLog("Libs opened, preloading love...");
#endif
    love::luax_preload(L, love_initialize, "love");

#ifdef __WIIU__
    updateLoadingScreen(0.4f, "LOVE preloaded, setting up arguments...");
    simpleLog("Love preloaded, setting up args table...");
#endif

    {
        lua_newtable(L);

        if (argc > 0)
        {
            lua_pushstring(L, argv[0]);
            lua_rawseti(L, -2, -2);
        }

        // on 3DS and Switch, argv[0] is the binary path
        // on Wii U, argv[0] is "safe.rpx"
        // args[-1] is "embedded boot.lua"
        // args[1] is "game" for the game folder
        // this should allow "game" to be used in love.filesystem.setSource
        std::vector<const char*> args(argv, argv + argc);

        /* Skip filesystem checks on Wii U to avoid hanging */
#ifdef __WIIU__
        simpleLog("Wii U detected - skipping filesystem checks, assuming fused game");
        // For Wii U, we always use fused mode since the game is embedded in the binary
#else
        /* if the game directory or game.love exists, add the arg */
        std::filesystem::path filepath = love::getApplicationPath(argv[0]);
        std::filesystem::path gameDir = filepath.parent_path().append("game");
        std::filesystem::path gameLove = filepath.parent_path().append("game.love");
        
        if (std::filesystem::exists(gameDir))
        {
            args.push_back("game");
        }
        else if (std::filesystem::exists(gameLove))
        {
            args.push_back("game.love");
        }
        else
        {
            // No external game found, assume fused
        }
#endif

        lua_pushstring(L, "embedded boot.lua");
        lua_rawseti(L, -2, -1);

        for (int index = 1; index < (int)args.size(); index++)
        {
            lua_pushstring(L, args[index]);
            lua_rawseti(L, -2, index);
        }

        lua_setglobal(L, "arg");
    }

#ifdef __WIIU__
    updateLoadingScreen(0.5f, "Arguments configured, requiring LOVE module...");
    simpleLog("Args table set, requiring love module...");
#endif

    lua_getglobal(L, "require");
    lua_pushstring(L, "love");
    lua_call(L, 1, 1); // leave the returned table on the stack.

#ifdef __WIIU__
    updateLoadingScreen(0.6f, "LOVE module loaded, setting up execution...");
    simpleLog("Love module required, setting _exe flag...");
#endif

    {
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "_exe");
    }

    love::luax_pushvariant(L, restartValue);
    lua_setfield(L, -2, "restart");
    restartValue = love::Variant();

    // Inject diagnostics (Lua) BEFORE popping love table
#ifdef __WIIU__
    simpleLog("Injecting Lua diagnostics into love table...");
#endif
    lua_getfield(L, -1, "_debugLog"); // try to see if exists (optional)
    lua_pop(L, 1);
    {
        const char *diagLua = R"DIAG_LUA(
            local function lowrite(msg)
                local f = io.open('/vol/external01/wiiu/apps/balatro/simple_debug.log','a') or io.open('/vol/external01/simple_debug.log','a') or io.open('simple_debug.log','a')
                if f then f:write('[LUA-DIAG] '..msg..'\n'); f:flush(); f:close() end
            end
            local clock = os.clock
            lowrite('DIAG injection start')
            if not love._diagInstalled then
                love._diagInstalled = true
                local _origRequire = require
                local rcount = 0
                function require(m)
                    rcount = rcount + 1
                    local memBefore = collectgarbage('count')
                    local t0 = clock()
                    if rcount < 200 or (rcount % 25) == 0 then
                        lowrite(string.format('require #%d BEGIN %s (mem=%.1fKB)', rcount, tostring(m), memBefore))
                    end
                    local ok, res = pcall(_origRequire, m)
                    local dt = (clock() - t0) * 1000.0
                    local memAfter = collectgarbage('count')
                    local dmem = memAfter - memBefore
                    if ok then
                        if rcount < 200 or (rcount % 25) == 0 then
                            lowrite(string.format('require #%d END   %s (%.2f ms, memΔ=%.1fKB total=%.1fKB)', rcount, tostring(m), dt, dmem, memAfter))
                        end
                        return res
                    else
                        lowrite(string.format('require #%d ERROR %s : %s (%.2f ms)', rcount, tostring(m), tostring(res), dt))
                        error(res)
                    end
                end
                if love.boot and type(love.boot)=='function' then
                    local _origBoot = love.boot
                    love.boot = function(...)
                        lowrite('love.boot enter')
                        local r = {_origBoot(...)}
                        lowrite('love.boot returned '..#r..' values')
                        for i,v in ipairs(r) do lowrite('boot ret['..i..']='..type(v)) end
                        return table.unpack(r)
                    end
                else
                    lowrite('love.boot missing or not function at injection')
                end
            else
                lowrite('DIAG already installed')
            end
            lowrite('DIAG injection end')
        )DIAG_LUA";
        if (luaL_dostring(L, diagLua) != 0) {
#ifdef __WIIU__
            simpleLog("Lua diagnostic injection failed");
#endif
            lua_pop(L,1);
        }
#ifdef __WIIU__
        simpleLog("Lua diagnostics injected (or attempted)");
#endif
    }

    lua_pop(L, 1); // pop love table

    lua_pop(L, 1);

#ifdef __WIIU__
    updateLoadingScreen(0.7f, "Setting up boot loader...");
    simpleLog("Setting up love.boot...");
#endif

    lua_getglobal(L, "require");
    lua_pushstring(L, "love.boot");
    lua_call(L, 1, 1);

#ifdef __WIIU__
    updateLoadingScreen(0.8f, "Boot loader ready, initializing engine...");
    simpleLog("love.boot loaded, checking what was returned...");
    
    // Debug what love.boot returned
    FILE* bootLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (bootLog) {
        int stackTop = lua_gettop(L);
        fprintf(bootLog, "=== LOVE.BOOT RETURN VALUE ANALYSIS ===\n");
        fprintf(bootLog, "Stack has %d items after love.boot call\n", stackTop);
        
        if (stackTop > 0) {
            int topType = lua_type(L, stackTop);
            const char* typeName = lua_typename(L, topType);
            fprintf(bootLog, "love.boot returned: %s\n", typeName);
            
            if (topType == LUA_TBOOLEAN) {
                int b = lua_toboolean(L, stackTop);
                fprintf(bootLog, "Boolean value: %s\n", b ? "true" : "false");
                fprintf(bootLog, "ERROR: love.boot returned boolean instead of function!\n");
                fprintf(bootLog, "This will cause the 'attempt to call boolean value' error!\n");
            } else if (topType == LUA_TFUNCTION) {
                fprintf(bootLog, "Function returned correctly\n");
            } else {
                fprintf(bootLog, "WARNING: Unexpected type returned by love.boot\n");
            }
        }
        fprintf(bootLog, "=== END LOVE.BOOT ANALYSIS ===\n");
        fflush(bootLog);
        fclose(bootLog);
    }
    
    simpleLog("Creating new thread and setting up stack...");
#endif

    // For LOVE Potion, we need to handle the boot function differently
    // The require("love.boot") returns a function that contains the main loop
    // We need to create a thread and transfer the function to it
    
    lua_State* thread = lua_newthread(L);  // Create thread, pushes it on stack
    
    // Now we need to transfer the boot function to the thread
    // Stack now: [args...] [boot_func] [thread]
    // We want: [args...] [thread] [boot_func] (where boot_func is in thread's stack)
    
    lua_pushvalue(L, -2);  // Copy boot function: [args...] [boot_func] [thread] [boot_func]
    lua_xmove(L, thread, 1);  // Move boot function to thread: [args...] [boot_func] [thread]
    
    // Now clean up the main stack - remove the original boot function
    lua_remove(L, -2);  // Remove original boot function: [args...] [thread]

#ifdef __WIIU__
    // Debug the corrected thread setup
    FILE* threadLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (threadLog) {
        int stackTop = lua_gettop(L);
        fprintf(threadLog, "=== CORRECTED THREAD SETUP ===\n");
        fprintf(threadLog, "Stack has %d items after thread setup\n", stackTop);
        
        // Check main stack
        for (int i = 1; i <= stackTop; i++) {
            int type = lua_type(L, i);
            const char* typeName = lua_typename(L, type);
            fprintf(threadLog, "MainStack[%d]: %s", i, typeName);
            
            if (type == LUA_TBOOLEAN) {
                int b = lua_toboolean(L, i);
                fprintf(threadLog, " = %s", b ? "true" : "false");
            } else if (type == LUA_TSTRING) {
                const char* str = lua_tostring(L, i);
                if (str) fprintf(threadLog, " = \"%s\"", str);
            } else if (type == LUA_TTHREAD) {
                lua_State* thread = lua_tothread(L, i);
                if (thread) {
                    int status = lua_status(thread);
                    fprintf(threadLog, " (status: %d)", status);
                    
                    // Check thread stack
                    int threadStackTop = lua_gettop(thread);
                    fprintf(threadLog, " ThreadStack: %d items", threadStackTop);
                    for (int j = 1; j <= threadStackTop && j <= 3; j++) {
                        int threadType = lua_type(thread, j);
                        fprintf(threadLog, " [%d]:%s", j, lua_typename(thread, threadType));
                    }
                }
            } else if (type == LUA_TFUNCTION) {
                fprintf(threadLog, " (function)");
            }
            fprintf(threadLog, "\n");
        }
        
        // Check what's at the top (what will be resumed)
        if (stackTop > 0) {
            int topType = lua_type(L, stackTop);
            fprintf(threadLog, "Top of stack (resume target): %s\n", lua_typename(L, topType));
            
            if (topType == LUA_TTHREAD) {
                fprintf(threadLog, "GOOD: Thread is at top of stack for lua_resume\n");
                lua_State* thread = lua_tothread(L, stackTop);
                if (thread) {
                    int threadStackTop = lua_gettop(thread);
                    fprintf(threadLog, "Thread has %d items, should have boot function\n", threadStackTop);
                    if (threadStackTop > 0) {
                        int funcType = lua_type(thread, threadStackTop);
                        fprintf(threadLog, "Thread top: %s\n", lua_typename(thread, funcType));
                    }
                }
            } else {
                fprintf(threadLog, "ERROR: Expected thread at top of stack!\n");
            }
        }
        
        fprintf(threadLog, "=== END THREAD SETUP ANALYSIS ===\n");
        fflush(threadLog);
        fclose(threadLog);
    }
#endif

    int position = lua_gettop(L);
    int results  = 0;

#ifdef __WIIU__
    updateLoadingScreen(0.9f, "Engine initialized, starting main loop...");
    simpleLog("Starting main loop...");
    love::DebugLogger::log("About to enter main loop with position=%d", position);
    simpleLog("About to call first love::mainLoop()...");
#endif

    int loopCount = 0;
    bool mainLoopResult = false;
    
#ifdef __WIIU__
    // Complete loading screen before starting main loop
    updateLoadingScreen(1.0f, "Ready! Starting game...");
    
    // Small delay to show completion - use thread yield instead of sleep
    OSYieldThread();
    
    // Shutdown loading screen once game starts
    shutdownLoadingScreen();
    
    // Use simple counter instead of time() which seems broken on Wii U
    int iterationCounter = 0;
    int lastLogIteration = 0;
#endif
    
    while (true)
    {
        try
        {
#ifdef __WIIU__
            // Log every 60 iterations or first few iterations
            if (loopCount < 3 || (loopCount % 60 == 0))
            {
                char counterBuffer[256];
                sprintf(counterBuffer, "Loop iteration %d", loopCount);
                simpleLog(counterBuffer);
            }
            
            // Log before every call for first few iterations
            if (loopCount < 3)
            {
                simpleLog("About to call love::mainLoop()...");
            }
            
            // Emergency break after 1000 iterations to prevent infinite loop
            if (loopCount > 1000)
            {
                simpleLog("ERROR: Too many iterations, breaking out of loop");
                break;
            }
#endif
            
            mainLoopResult = love::mainLoop(L, 0, &results);
            
#ifdef __WIIU__
            if (loopCount < 3)
            {
                char resultBuffer[256];
                sprintf(resultBuffer, "love::mainLoop() returned %s", mainLoopResult ? "true" : "false");
                simpleLog(resultBuffer);
            }
            
            // Send resize event after first successful mainLoop call to ensure canvas initialization
            if (loopCount == 0 && mainLoopResult)
            {
                simpleLog("Sending resize event to ensure canvas initialization...");
                try {
                    auto& eventQueue = love::EventQueue::getInstance();
                    eventQueue.sendResize(800, 600);
                    simpleLog("Resize event sent successfully");
                } catch (...) {
                    simpleLog("ERROR: Failed to send resize event");
                }
            }
#endif
            
#ifdef __WIIU__
            if (loopCount < 5) // Log first few iterations in detail
            {
                char buffer[256];
                sprintf(buffer, "mainLoop() returned %s, results=%d", 
                        mainLoopResult ? "true" : "false", results);
                simpleLog(buffer);
            }
#endif
            
            if (!mainLoopResult)
            {
#ifdef __WIIU__
                simpleLog("mainLoop() returned false, exiting...");
#endif
                break;
            }
            
#if LUA_VERSION_NUM >= 504
            lua_pop(L, results)
#else
            lua_pop(L, lua_gettop(L) - position);
#endif
            
            loopCount++;
        }
        catch (const love::Exception& e)
        {
#ifdef __WIIU__
            love::DebugLogger::log("LOVE Exception in main loop iteration: %s", e.what());
            simpleLog("LOVE Exception in main loop iteration");
#endif
            break; // Exit the main loop
        }
        catch (const std::exception& e)
        {
#ifdef __WIIU__
            love::DebugLogger::log("C++ Exception in main loop iteration: %s", e.what());
            simpleLog("C++ Exception in main loop iteration");
#endif
            break; // Exit the main loop
        }
        catch (...)
        {
#ifdef __WIIU__
            love::DebugLogger::log("Unknown exception in main loop iteration");
            simpleLog("Unknown exception in main loop iteration");
#endif
            break; // Exit the main loop
        }
    }

#ifdef __WIIU__
    simpleLog("Main loop finished, processing results...");
    love::DebugLogger::log("Main loop exited normally");
#endif

    result            = 0;
    DoneAction action = DONE_QUIT;

    const auto index = position;
    if (!lua_isnoneornil(L, index))
    {
        if (lua_type(L, index) == LUA_TSTRING && strcmp(lua_tostring(L, index), "restart") == 0)
            action = DONE_RESTART;

        if (lua_isnumber(L, index))
            result = lua_tonumber(L, index);

        if (index < lua_gettop(L))
            restartValue = love::luax_checkvariant(L, index + 1, false);
    }

#ifdef __WIIU__
    // Check if there are any Lua errors left on the stack
    if (lua_gettop(L) > 0 && lua_type(L, -1) == LUA_TSTRING)
    {
        const char* errorMsg = lua_tostring(L, -1);
        if (errorMsg)
        {
            love::DebugLogger::log("Final Lua error on stack: %s", errorMsg);
        }
    }
#endif

    lua_close(L);

#ifdef __WIIU__
    simpleLog("runLove() finished, returning action");
#endif

    return action;
}

int main(int argc, char** argv)
{
#ifdef __WIIU__
    // Initialize simple logging first
    initSimpleLog();

#ifdef USE_PPC_DEBUGGER
    // Initialize PPC debugger IMMEDIATELY for maximum coverage
    love::PPCDebugger::Initialize();
    love::PPCDebugger::DebugPoint("MAIN_START", "Application entry point reached");
    love::PPCDebugger::StartPerformanceTimer("main_execution");
#endif
    simpleLog("=== MAIN() STARTED ===");
    
    // Initialize DebugLogger too
    love::DebugLogger::init();
    love::DebugLogger::log("=== MAIN() STARTED ===");
    
    char buffer[256];
    sprintf(buffer, "main() called with argc=%d", argc);
    simpleLog(buffer);
    
    for (int i = 0; i < argc; i++) {
        sprintf(buffer, "argv[%d] = %s", i, argv[i] ? argv[i] : "(null)");
        simpleLog(buffer);
    }

#ifdef USE_PPC_DEBUGGER    
    love::PPCDebugger::LogMemoryDump(0x10000000, 512, "MAIN_INIT_MEMORY");
    love::PPCDebugger::DebugPoint("PREINIT_START", "About to call preInit()");
#endif
    simpleLog("About to call preInit()");
    
    // Also use DebugLogger
    love::DebugLogger::log("main() starting with argc=%d", argc);
    for (int i = 0; i < argc; i++)
        love::DebugLogger::log("argv[%d] = %s", i, argv[i]);
#endif

    try
    {
#ifdef __WIIU__
        simpleLog("About to call love::preInit()");
#endif
        int preInitResult = love::preInit();
        if (preInitResult != 0)
        {
#ifdef __WIIU__
            simpleLog("ERROR: preInit() failed!");
            love::DebugLogger::log("ERROR: preInit() failed with code %d", preInitResult);
#ifdef USE_PPC_DEBUGGER
            love::PPCDebugger::CriticalError("preInit() failed", true);
#endif
#endif
            love::onExit();
            return 0;
        }
#ifdef __WIIU__
#ifdef USE_PPC_DEBUGGER
        love::PPCDebugger::DebugPoint("PREINIT_SUCCESS", "preInit() completed successfully");
#endif
        simpleLog("preInit() completed successfully");
#endif
    }
    catch (const love::Exception& e)
    {
#ifdef __WIIU__
        love::DebugLogger::log("LOVE Exception in preInit(): %s", e.what());
#ifdef USE_PPC_DEBUGGER
        love::PPCDebugger::CriticalError("LOVE Exception in preInit()", true);
#endif
#endif
        love::onExit();
        return 1;
    }
    catch (const std::exception& e)
    {
#ifdef __WIIU__
        love::DebugLogger::log("C++ Exception in preInit(): %s", e.what());
#endif
        love::onExit();
        return 1;
    }
    catch (...)
    {
#ifdef __WIIU__
        love::DebugLogger::log("Unknown exception in preInit()");
#endif
        love::onExit();
        return 1;
    }

    try
    {
#ifdef __WIIU__
        simpleLog("preInit() completed successfully");
#endif

        int result        = 0;
        DoneAction action = DONE_QUIT;
        love::Variant restartValue;

#ifdef __WIIU__
        simpleLog("Starting main game loop");
        love::DebugLogger::log("Starting main loop");
#endif

        do
        {
            try
            {
#ifdef __WIIU__
                love::DebugLogger::log("Calling runLove() iteration...");
#endif
                action = runLove(argv, argc, result, restartValue);
#ifdef __WIIU__
                love::DebugLogger::log("runLove completed with action=%d, result=%d", (int)action, result);
#endif
            }
            catch (const love::Exception& e)
            {
#ifdef __WIIU__
                simpleLog("LOVE Exception caught in main loop");
                love::DebugLogger::log("LOVE Exception caught in main loop: %s", e.what());
#endif
                result = 1;
                action = DONE_QUIT;
            }
            catch (const std::exception& e)
            {
#ifdef __WIIU__
                simpleLog("C++ Exception caught in main loop");
                love::DebugLogger::log("C++ Exception caught in main loop: %s", e.what());
#endif
                result = 1;
                action = DONE_QUIT;
            }
            catch (...)
            {
#ifdef __WIIU__
                simpleLog("Unknown exception caught in main loop");
                love::DebugLogger::log("Unknown exception caught in main loop");
#endif
                result = 1;
                action = DONE_QUIT;
            }
        } while (action != DONE_QUIT);

#ifdef __WIIU__
        simpleLog("Main loop finished, calling onExit()");
        love::DebugLogger::log("Main loop finished, calling onExit()");
#endif
        
        try
        {
            love::onExit();
        }
        catch (const love::Exception& e)
        {
#ifdef __WIIU__
            love::DebugLogger::log("LOVE Exception in onExit(): %s", e.what());
#endif
        }
        catch (const std::exception& e)
        {
#ifdef __WIIU__
            love::DebugLogger::log("C++ Exception in onExit(): %s", e.what());
#endif
        }
        catch (...)
        {
#ifdef __WIIU__
            love::DebugLogger::log("Unknown exception in onExit()");
#endif
        }

#ifdef __WIIU__
        simpleLog("=== MAIN() COMPLETED SUCCESSFULLY ===");
        love::DebugLogger::close();
#endif
        return result;
    }
    catch (const love::Exception& e)
    {
#ifdef __WIIU__
        love::DebugLogger::log("FATAL LOVE Exception in main(): %s", e.what());
        love::DebugLogger::close();
#endif
        love::onExit();
        return 1;
    }
    catch (const std::exception& e)
    {
#ifdef __WIIU__
        love::DebugLogger::log("FATAL C++ Exception in main(): %s", e.what());
        love::DebugLogger::close();
#endif
        love::onExit();
        return 1;
    }
    catch (...)
    {
#ifdef __WIIU__
        love::DebugLogger::log("FATAL Unknown exception in main()");
        love::DebugLogger::close();
#endif
        love::onExit();
        return 1;
    }
}

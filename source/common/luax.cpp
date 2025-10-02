#include "common/luax.hpp"
#include "common/config.hpp"

#include "common/Module.hpp"
#include "common/Object.hpp"
#include "common/Reference.hpp"

#include <cmath>
#include <cstring>
#include <cstdio>

#include <algorithm>

#ifdef __WIIU__
#include <coreinit/time.h>
#endif

namespace love
{
    // #region Startup

    int luax_preload(lua_State* L, lua_CFunction function, const char* name)
    {
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "preload");
        lua_pushcfunction(L, function);
        lua_setfield(L, -2, name);
        lua_pop(L, 2);

        return 0;
    }

    int luax_insistglobal(lua_State* L, const char* name)
    {
        lua_getglobal(L, name);

        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setglobal(L, name);
        }

        return 1;
    }

    lua_State* luax_insistpinnedthread(lua_State* L)
    {
        lua_getfield(L, LUA_REGISTRYINDEX, MAIN_THREAD_KEY);

        if (lua_isnoneornil(L, -1))
        {
            lua_pop(L, 1);
            lua_pushthread(L);
            lua_pushvalue(L, -1);
            lua_setfield(L, LUA_REGISTRYINDEX, MAIN_THREAD_KEY);
        }

        lua_State* thread = lua_tothread(L, -1);
        lua_pop(L, 1);

        return thread;
    }

    lua_State* luax_getpinnedthread(lua_State* L)
    {
        lua_getfield(L, LUA_REGISTRYINDEX, MAIN_THREAD_KEY);
        lua_State* thread = lua_tothread(L, -1);
        lua_pop(L, 1);

        return thread;
    }

    int luax_insist(lua_State* L, int index, const char* name)
    {
        if (index < 0 && index > LUA_REGISTRYINDEX)
            index += lua_gettop(L) + 1;

        lua_getfield(L, index, name);

        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setfield(L, index, name);
        }

        return 1;
    }

    int luax_insistlove(lua_State* L, const char* name)
    {
        lua_getglobal(L, "love");
        luax_insist(L, -1, name);
        lua_replace(L, -2);

        return 1;
    }

    int luax_getlove(lua_State* L, const char* name)
    {
        lua_getglobal(L, "love");

        if (!lua_isnil(L, -1))
        {
            lua_getfield(L, -1, name);
            lua_replace(L, -2);
        }

        return 1;
    }

    int luax_insistregistry(lua_State* L, Registry registry)
    {
        switch (registry)
        {
            case REGISTRY_MODULES:
                return luax_insistlove(L, MODULES_REGISTRY_KEY);
            case REGISTRY_OBJECTS:
                return luax_insist(L, LUA_REGISTRYINDEX, OBJECTS_REGISTRY_KEY);
            default:
                return luaL_error(L, "Attempted to use invalid registry.");
        }
    }

    int luax_getregistry(lua_State* L, Registry registry)
    {
        switch (registry)
        {
            case REGISTRY_MODULES:
                return luax_getlove(L, MODULES_REGISTRY_KEY);
            case REGISTRY_OBJECTS:
                lua_getfield(L, LUA_REGISTRYINDEX, OBJECTS_REGISTRY_KEY);
                return 1;
            default:
                return luaL_error(L, "Attempted to use invalid registry.");
        }
    }

    int luax_resume(lua_State* L, int argc, int* nres)
    {
#ifdef __WIIU__
        static int resumeCallCount = 0;
        static uint64_t firstResumeTime = 0;
        resumeCallCount++;
        
        // Get current time for timing calculations
        uint64_t currentTime = OSGetSystemTime();
        if (firstResumeTime == 0) {
            firstResumeTime = currentTime;
        }
        uint64_t elapsed = OSTicksToMilliseconds(currentTime - firstResumeTime);
        
        printf("[LUAX_RESUME] Starting lua_resume() call #%d with argc=%d\n", resumeCallCount, argc);
        fflush(stdout);
        
        // Also log to file for debugging
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "luax_resume() call #%d with argc=%d\n", resumeCallCount, argc);
            fflush(logFile);
            fclose(logFile);
        }
        
        // Log timestamp to detect how long lua_resume takes
        FILE* timeLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (timeLog) {
            fprintf(timeLog, "lua_resume() call #%d starting at system time, elapsed: %lld ms\n", resumeCallCount, elapsed);
            fflush(timeLog);
            fclose(timeLog);
        }
        
        // Log current Lua stack state
        FILE* logFile3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile3) {
            fprintf(logFile3, "Lua stack top: %d\n", lua_gettop(L));
            fflush(logFile3);
            fclose(logFile3);
        }
        
        printf("[LUAX_RESUME] About to call lua_resume...\n");
        fflush(stdout);
        
        // Add file logging too  
        FILE* logFileA = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFileA) {
            fprintf(logFileA, "About to call lua_resume()...\n");
            
            // Log current Lua stack for debugging
            int top = lua_gettop(L);
            fprintf(logFileA, "Lua stack has %d items\n", top);
            for (int i = 1; i <= top; i++) {
                int type = lua_type(L, i);
                const char* typeName = lua_typename(L, type);
                fprintf(logFileA, "  Stack[%d]: %s", i, typeName);
                if (type == LUA_TSTRING) {
                    fprintf(logFileA, " = \"%s\"", lua_tostring(L, i));
                } else if (type == LUA_TNUMBER) {
                    fprintf(logFileA, " = %f", lua_tonumber(L, i));
                } else if (type == LUA_TBOOLEAN) {
                    fprintf(logFileA, " = %s", lua_toboolean(L, i) ? "true" : "false");
                } else if (type == LUA_TFUNCTION) {
                    fprintf(logFileA, " = function");
                }
                fprintf(logFileA, "\n");
            }
            fflush(logFileA);
            fclose(logFileA);
        }
#endif

        // CRITICAL FIX: We need to resume the thread, not the main state
        // Get the thread from the top of the stack
        lua_State* thread = nullptr;
        int stackTop = lua_gettop(L);
        
        if (stackTop > 0 && lua_type(L, stackTop) == LUA_TTHREAD) {
            thread = lua_tothread(L, stackTop);
        }
        
        if (!thread) {
            // Emergency fallback - this should not happen
#ifdef __WIIU__
            FILE* errorLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (errorLog) {
                fprintf(errorLog, "CRITICAL ERROR: No thread found at top of stack in luax_resume!\n");
                fprintf(errorLog, "Stack has %d items, top type: %s\n", stackTop, 
                       stackTop > 0 ? lua_typename(L, lua_type(L, stackTop)) : "empty");
                fflush(errorLog);
                fclose(errorLog);
            }
#endif
            return 2; // LUA_ERRRUN - Return error
        }

#ifdef __WIIU__
        FILE* threadLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (threadLog) {
            fprintf(threadLog, "About to resume thread (not main state) with argc=%d\n", argc);
            int threadStackTop = lua_gettop(thread);
            fprintf(threadLog, "Thread stack has %d items\n", threadStackTop);
            
            // Check thread status
            int status = lua_status(thread);
            fprintf(threadLog, "Thread status: %d", status);
            if (status == 0) { // 0 = LUA_OK in older Lua versions
                fprintf(threadLog, " (LUA_OK - can be resumed)\n");
            } else if (status == 1) { // 1 = LUA_YIELD in Lua 5.1
                fprintf(threadLog, " (LUA_YIELD - yielded)\n");
            } else {
                fprintf(threadLog, " (ERROR or unexpected status)\n");
                // If thread is in error state, try to get error message
                if (threadStackTop > 0 && lua_type(thread, -1) == LUA_TSTRING) {
                    const char* error = lua_tostring(thread, -1);
                    fprintf(threadLog, "Thread error: %s\n", error ? error : "unknown");
                }
            }
            
            fflush(threadLog);
            fclose(threadLog);
        }
#endif

#ifdef __WIIU__
        FILE* preResumeLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (preResumeLog) {
            fprintf(preResumeLog, "About to call lua_resume() with thread=%p, L=%p, argc=%d\n", thread, L, argc);
            fflush(preResumeLog);
            fclose(preResumeLog);
        }
        printf("[LUAX_RESUME] About to call lua_resume() - thread=%p, argc=%d\n", thread, argc);
        fflush(stdout);
#endif

        // CRITICAL: Add crash protection around lua_resume
        int result = -999; // Initialize to invalid value to detect if assignment fails
        
        // Store last known state information for debugging
        static char lastLuaState[512] = {0};
        static bool stateInitialized = false;
        static uint64_t startTime = 0;
        
        if (!stateInitialized) {
            snprintf(lastLuaState, sizeof(lastLuaState), "Initial lua_resume call - Thread: %p", thread);
            stateInitialized = true;
#ifdef __WIIU__
            startTime = OSGetSystemTime();
#endif
        }
        
        // Calculate elapsed time since first resume
        uint64_t currentElapsed = 0;
#ifdef __WIIU__
        currentElapsed = OSTicksToMilliseconds(OSGetSystemTime() - startTime);
#endif
        
        // Update current state info
        if (lua_gettop(thread) > 0) {
            const char* typeName = lua_typename(thread, lua_type(thread, -1));
            snprintf(lastLuaState, sizeof(lastLuaState), 
                     "Thread: %p, Stack size: %d, Top type: %s, Time: %lld ms", 
                     thread, lua_gettop(thread), typeName, currentElapsed);
        } else {
            snprintf(lastLuaState, sizeof(lastLuaState), 
                     "Thread: %p, Stack empty, Time: %lld ms", thread, currentElapsed);
        }
        
#ifdef __WIIU__
        // Write the current state to a separate file for crash recovery
        FILE* stateFile = fopen("/vol/external01/wiiu/apps/balatro/lua_state.log", "w");
        if (stateFile) {
            fprintf(stateFile, "Last known Lua state before potential hang:\n");
            fprintf(stateFile, "%s\n", lastLuaState);
            fprintf(stateFile, "Time elapsed since first resume: %lld ms\n", currentElapsed);
            fflush(stateFile);
            fclose(stateFile);
        }
#endif
        
#ifdef __WIIU__
        FILE* aboutToCallLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (aboutToCallLog) {
            fprintf(aboutToCallLog, "=== ABOUT TO CALL lua_resume() ===\n");
            fprintf(aboutToCallLog, "Thread: %p, argc: %d, result initialized to: %d\n", thread, argc, result);
            fprintf(aboutToCallLog, "Time elapsed since first resume: %lld ms\n", currentElapsed);
            fflush(aboutToCallLog);
            fclose(aboutToCallLog);
        }
#endif
        
    // Watchdog removed for stability on Wii U. We'll rely on elapsed timing after the call instead.
        try {
#ifdef __WIIU__
            printf("[LUAX_RESUME] Entering lua_resume call...\n");
            fflush(stdout);
            
            FILE* enteringLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (enteringLog) {
                fprintf(enteringLog, "=== ENTERING lua_resume() CALL ===\n");
                fprintf(enteringLog, "Thread: %p, argc: %d, result initialized to: %d\n", (void*)thread, argc, result);
                fflush(enteringLog);
                fclose(enteringLog);
            }
            
            // CRITICAL: Additional pre-call verification
            FILE* preCallLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (preCallLog) {
                fprintf(preCallLog, "=== FINAL PRE-CALL VERIFICATION ===\n");
                fprintf(preCallLog, "Thread status before call: %d\n", lua_status(thread));
                fprintf(preCallLog, "Thread stack size: %d\n", lua_gettop(thread));
                fprintf(preCallLog, "Main L stack size: %d\n", lua_gettop(L));
                fprintf(preCallLog, "About to call lua_resume with thread=%p, L=%p, argc=%d\n", 
                        (void*)thread, (void*)L, argc);
                fflush(preCallLog);
                fclose(preCallLog);
            }
#endif

#if LUA_VERSION_NUM >= 504
            result = lua_resume(thread, L, argc, nres);
#elif LUA_VERSION_NUM >= 502
            LOVE_UNUSED(nres);
            result = lua_resume(thread, L, argc);
#else
            LOVE_UNUSED(nres);
            result = lua_resume(thread, argc);
#endif

#ifdef __WIIU__
            printf("[LUAX_RESUME] lua_resume call completed normally\n");
            fflush(stdout);

            uint64_t endTicks = OSGetSystemTime();
            uint64_t elapsedSinceFirst = 0;
            if (startTime != 0) {
                elapsedSinceFirst = OSTicksToMilliseconds(endTicks - startTime);
            }
            FILE* completedLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (completedLog) {
                fprintf(completedLog, "=== lua_resume() CALL COMPLETED ===\n");
                fprintf(completedLog, "Result: %d (elapsed=%lld ms since first resume start)\n", result, (long long)elapsedSinceFirst);
                fprintf(completedLog, "Thread status after call: %d\n", lua_status(thread));
                fprintf(completedLog, "Thread stack size after call: %d\n", lua_gettop(thread));
                fprintf(completedLog, "Main L stack size after call: %d\n", lua_gettop(L));
                fflush(completedLog);
                fclose(completedLog);
            }
#endif
        } catch (...) {
#ifdef __WIIU__
            FILE* crashLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (crashLog) {
                fprintf(crashLog, "CRASH: Exception caught in lua_resume call!\n");
                fflush(crashLog);
                fclose(crashLog);
            }
            printf("[LUAX_RESUME] CRASH: Exception caught in lua_resume!\n");
            fflush(stdout);
#endif
            return 2; // LUA_ERRRUN - Return error on crash
        }

    // (Watchdog removed) No thread to join.

#ifdef __WIIU__
        FILE* postResumeLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (postResumeLog) {
            fprintf(postResumeLog, "lua_resume() completed successfully with result=%d\n", result);
            fflush(postResumeLog);
            fclose(postResumeLog);
        }
        printf("[LUAX_RESUME] lua_resume() completed with result=%d\n", result);
        fflush(stdout);
#endif

#ifdef __WIIU__
        // Log immediately after lua_resume returns
        printf("[LUAX_RESUME] lua_resume() call #%d returned %d", resumeCallCount, result);
        if (result == 1) { // 1 = LUA_YIELD in Lua 5.1
            printf(" (LUA_YIELD - coroutine yielded - NORMAL)\n");
        } else if (result == 0) {
            printf(" (LUA_OK - success/completed - UNEXPECTED)\n");
        } else {
            printf(" (ERROR - check for Lua error)\n");
        }
        fflush(stdout);
        
        // Also log to file for debugging with more detail
        FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile2) {
            fprintf(logFile2, "lua_resume() call #%d returned %d", resumeCallCount, result);
            if (result == 1) { // 1 = LUA_YIELD in Lua 5.1
                fprintf(logFile2, " (LUA_YIELD - coroutine yielded - NORMAL)\n");
            } else if (result == 0) {
                fprintf(logFile2, " (LUA_OK - success/completed)\n");
                fprintf(logFile2, "WARNING: Thread returned LUA_OK - main loop completed unexpectedly!\n");
                fprintf(logFile2, "This means love.run() finished instead of yielding.\n");
                
                // Check what's on the thread stack after completion
                int threadStackAfter = lua_gettop(thread);
                fprintf(logFile2, "Thread stack after completion: %d items\n", threadStackAfter);
                for (int i = 1; i <= threadStackAfter && i <= 5; i++) {
                    int type = lua_type(thread, i);
                    fprintf(logFile2, "  CompletedThread[%d]: %s\n", i, lua_typename(thread, type));
                }
                
                // Check if there's an error message
                if (threadStackAfter > 0 && lua_type(thread, -1) == LUA_TSTRING) {
                    const char* msg = lua_tostring(thread, -1);
                    fprintf(logFile2, "Thread has string on top: %s\n", msg ? msg : "(null)");
                }
            } else {
                fprintf(logFile2, " (ERROR - check for Lua error)\n");
                
                // Log the error details
                if (lua_gettop(thread) > 0 && lua_type(thread, -1) == LUA_TSTRING) {
                    const char* errorMsg = lua_tostring(thread, -1);
                    fprintf(logFile2, "Error message: %s\n", errorMsg ? errorMsg : "(null)");
                }
            }
            
            // Log thread status after resume
            int threadStatus = lua_status(thread);
            fprintf(logFile2, "Thread status after resume: %d\n", threadStatus);
            
            fflush(logFile2);
            fclose(logFile2);
        }
#endif

        return result;
    }

    int luax_register_searcher(lua_State* L, lua_CFunction function, int position)
    {
        lua_getglobal(L, "package");

        if (lua_isnil(L, -1))
            return luaL_error(L, "Can't register searcher: package table does not exist.");

        lua_getfield(L, -1, "loaders");

        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            lua_getfield(L, -1, "searchers");
        }

        if (lua_isnil(L, -1))
            return luaL_error(L, "Can't register searcher: package.loaders table does not exist.");

        lua_pushcfunction(L, function);
        luax_table_insert(L, -2, -1, position);

        lua_pop(L, 3);

        return 0;
    }

    // #endregion

    // #region Objects

    void luax_pushobjectkey(lua_State* L, ObjectKey key)
    {
        if (luax_isfulllightuserdatasupported(L))
            lua_pushlightuserdata(L, (void*)key);
        else if (key > LUAX_MAX_OBJECT_KEY)
            luaL_error(L, E_POINTER_TOO_LARGE, key);
        else
            lua_pushnumber(L, (lua_Number)key);
    }

    static ObjectKey luax_computeobjectkey(lua_State* L, Object* object)
    {
        const size_t min_align = sizeof(void*) == 8 ? alignof(std::max_align_t) : 1;
        uintptr_t key          = (uintptr_t)object;

        if ((key & (min_align - 1)) != 0)
            luaL_error(L, E_UNEXPECTED_ALIGNMENT, object, (int)min_align);

        static const size_t shift = (size_t)std::log2(min_align);

        key >>= shift;
        return (ObjectKey)key;
    }

    static int w_release(lua_State* L)
    {
        Proxy* proxy   = (Proxy*)lua_touserdata(L, 1);
        Object* object = proxy->object;

        if (object != nullptr)
        {
            proxy->object = nullptr;
            object->release();

            luax_getregistry(L, REGISTRY_OBJECTS);

            if (lua_istable(L, -1))
            {
                ObjectKey key = luax_computeobjectkey(L, object);
                luax_pushobjectkey(L, key);
                lua_pushnil(L);
                lua_settable(L, -3);
            }

            lua_pop(L, 1);
        }

        luax_pushboolean(L, object != nullptr);

        return 1;
    }

    static int w__gc(lua_State* L)
    {
        Proxy* proxy = (Proxy*)lua_touserdata(L, 1);

        if (proxy->object != nullptr)
        {
            proxy->object->release();
            proxy->object = nullptr;
        }

        return 0;
    }

    static int w__tostring(lua_State* L)
    {
        Proxy* proxy = (Proxy*)lua_touserdata(L, 1);

        const char* name = lua_tostring(L, lua_upvalueindex(1));
        lua_pushfstring(L, "%s: %p", name, proxy->object);

        return 1;
    }

    static int w_type(lua_State* L)
    {
        lua_pushvalue(L, lua_upvalueindex(1));

        return 1;
    }

    static int w_typeOf(lua_State* L)
    {
        Proxy* proxy = (Proxy*)lua_touserdata(L, 1);
        Type* type   = luax_type(L, 2);

        if (!type)
            luax_pushboolean(L, false);
        else
            luax_pushboolean(L, proxy->type->isA(*type));

        return 1;
    }

    static int w__eq(lua_State* L)
    {
        Proxy* proxy1 = (Proxy*)lua_touserdata(L, 1);
        Proxy* proxy2 = (Proxy*)lua_touserdata(L, 2);

        luax_pushboolean(L, proxy1->object == proxy2->object && proxy1->object != nullptr);

        return 1;
    }

    // #endregion

    // #region Helpers

    int luax_isfulllightuserdatasupported(lua_State* L)
    {
        static bool checked   = false;
        static bool supported = false;

        if (sizeof(void*) == 4)
            return true;

        if (!checked)
        {
            lua_pushcclosure(
                L,
                [](lua_State* L) -> int {
                    lua_pushlightuserdata(L, (void*)(~((size_t)0)));
                    return 1;
                },
                0);

            supported = lua_pcall(L, 0, 1, 0) == 0;
            checked   = true;

            lua_pop(L, 1);
        }

        return supported;
    }

    Type* luax_type(lua_State* L, int index)
    {
        return Type::byName(luaL_checkstring(L, index));
    }

    int luax_typeerror(lua_State* L, int argc, const char* name)
    {
        int argtype             = lua_type(L, argc);
        const char* argTypeName = nullptr;

        if (argtype == LUA_TUSERDATA && luaL_getmetafield(L, argc, "type") != 0)
        {
            lua_pushvalue(L, argc);
            if (lua_pcall(L, 1, 1, 0) == 0 && lua_type(L, -1) == LUA_TSTRING)
            {
                argTypeName = lua_tostring(L, -1);
                if (!Type::byName(argTypeName))
                    argTypeName = nullptr;
            }
        }

        if (argTypeName == nullptr)
            argTypeName = lua_typename(L, argtype);

        const char* message = lua_pushfstring(L, "%s expected, got %s", name, argTypeName);
        return luaL_argerror(L, argc, message);
    }

    bool luax_istype(lua_State* L, int index, Type& type)
    {
        if (lua_type(L, index) != LUA_TUSERDATA)
            return false;

        Proxy* userdata = (Proxy*)lua_touserdata(L, index);

        if (userdata->type != nullptr)
            return userdata->type->isA(type);
        else
            return false;
    }

    Proxy* luax_tryextractproxy(lua_State* L, int index)
    {
        Proxy* proxy = (Proxy*)lua_touserdata(L, index);

        if (!proxy || !proxy->type)
            return nullptr;

        if (dynamic_cast<Object*>(proxy->object) != nullptr)
            return proxy;

        return nullptr;
    }

    void luax_rawnewtype(lua_State* L, Type& type, Object* object)
    {
        Proxy* proxy = (Proxy*)lua_newuserdata(L, sizeof(Proxy));
        object->retain();

        proxy->object = object;
        proxy->type   = &type;

        const char* name = type.getName();
        luaL_newmetatable(L, name);

        lua_getfield(L, -1, "__gc");
        bool has_gc = !lua_isnoneornil(L, -1);
        lua_pop(L, 1);

        if (!has_gc)
        {
            lua_pushcfunction(L, w__gc);
            lua_setfield(L, -2, "__gc");
        }

        lua_setmetatable(L, -2);
    }

    void luax_pushtype(lua_State* L, Type& type, Object* object)
    {
        if (object == nullptr)
        {
            lua_pushnil(L);
            return;
        }

        luax_getregistry(L, REGISTRY_OBJECTS);

        if (lua_isnoneornil(L, -1))
        {
            lua_pop(L, 1);
            return luax_rawnewtype(L, type, object);
        }

        ObjectKey key = luax_computeobjectkey(L, object);
        luax_pushobjectkey(L, key);

        lua_gettable(L, -2);

        if (lua_type(L, -1) != LUA_TUSERDATA)
        {
            lua_pop(L, 1);

            luax_rawnewtype(L, type, object);

            luax_pushobjectkey(L, key);
            lua_pushvalue(L, -2);

            lua_settable(L, -4);
        }

        lua_remove(L, -2);
    }

    void luax_pushvariant(lua_State* L, const Variant& variant)
    {
        const Variant::Data& data = variant.getData();

        switch (variant.getType())
        {
            case Variant::BOOLEAN:
                lua_pushboolean(L, data.boolean);
                break;
            case Variant::NUMBER:
                lua_pushnumber(L, data.number);
                break;
            case Variant::STRING:
                lua_pushlstring(L, data.string->string, data.string->length);
                break;
            case Variant::SMALLSTRING:
                lua_pushlstring(L, data.smallstring.str, data.smallstring.len);
                break;
            case Variant::LUSERDATA:
                lua_pushlightuserdata(L, data.userdata);
                break;
            case Variant::LOVEOBJECT:
                luax_pushtype(L, *data.proxy.type, data.proxy.object);
                break;
            case Variant::TABLE:
            {
                auto& table = data.table->pairs;
                int tsize   = (int)table.size();

                lua_createtable(L, 0, tsize);

                for (int i = 0; i < tsize; ++i)
                {
                    std::pair<Variant, Variant>& kv = table[i];
                    luax_pushvariant(L, kv.first);
                    luax_pushvariant(L, kv.second);
                    lua_settable(L, -3);
                }

                break;
            }
            case Variant::NIL:
            default:
                lua_pushnil(L);
                break;
        }
    }

    Variant luax_checkvariant(lua_State* L, int index, bool allowuserdata, std::set<const void*>* tableSet)
    {
        size_t len;
        const char* str;
        Proxy* p = nullptr;

        if (index < 0) // Fix the stack position, we might modify it later
            index += lua_gettop(L) + 1;

        switch (lua_type(L, index))
        {
            case LUA_TBOOLEAN:
                return Variant(luax_toboolean(L, index));
            case LUA_TNUMBER:
                return Variant(lua_tonumber(L, index));
            case LUA_TSTRING:
                str = lua_tolstring(L, index, &len);
                return Variant(str, len);
            case LUA_TLIGHTUSERDATA:
                return Variant(lua_touserdata(L, index));
            case LUA_TUSERDATA:
                if (!allowuserdata)
                {
                    luax_typeerror(L, index, "copyable Lua value");
                    return Variant();
                }
                p = luax_tryextractproxy(L, index);
                if (p != nullptr)
                    return Variant(p->type, p->object);
                else
                {
                    luax_typeerror(L, index, "love type");
                    return Variant();
                }
            case LUA_TNIL:
                return Variant();
            case LUA_TTABLE:
            {
                bool success = true;
                std::set<const void*> topTableSet;

                // We can use a pointer to a stack-allocated variable because it's
                // never used after the stack-allocated variable is destroyed.
                if (tableSet == nullptr)
                    tableSet = &topTableSet;

                // Now make sure this table wasn't already serialised
                const void* tablePointer = lua_topointer(L, index);
                {
                    auto result = tableSet->insert(tablePointer);
                    if (!result.second) // insertion failed
                        throw love::Exception("Cycle detected in table");
                }

                Variant::SharedTable* table = new Variant::SharedTable();

                size_t len = luax_objlen(L, -1);
                if (len > 0)
                    table->pairs.reserve(len);

                lua_pushnil(L);

                while (lua_next(L, index))
                {
                    // clang-format off
                    table->pairs.emplace_back(
                        luax_checkvariant(L, -2, allowuserdata, tableSet),
                        luax_checkvariant(L, -1, allowuserdata, tableSet)
                    );
                    // clang-format on
                    lua_pop(L, 1);

                    const auto& p = table->pairs.back();
                    if (p.first.getType() == Variant::UNKNOWN || p.second.getType() == Variant::UNKNOWN)
                    {
                        success = false;
                        break;
                    }
                }

                // And remove the table from the set again
                tableSet->erase(tablePointer);

                if (success)
                    return Variant(table);
                else
                    table->release();
            }
            break;
        }

        return Variant::unknown();
    }

    bool luax_toboolean(lua_State* L, int index)
    {
        return (lua_toboolean(L, index) != 0);
    }

    bool luax_checkboolean(lua_State* L, int index)
    {
        luaL_checktype(L, index, LUA_TBOOLEAN);
        return luax_toboolean(L, index);
    }

    void luax_pushboolean(lua_State* L, bool boolean)
    {
        lua_pushboolean(L, boolean ? 1 : 0);
    }

    bool luax_optboolean(lua_State* L, int index, bool default_value)
    {
        if (lua_isboolean(L, index) == 1)
            return (lua_toboolean(L, index) == 1 ? true : false);

        return default_value;
    }

    std::string luax_tostring(lua_State* L, int index)
    {
        size_t length;
        const char* string = lua_tolstring(L, index, &length);

        return std::string(string, length);
    }

    std::string luax_checkstring(lua_State* L, int index)
    {
        size_t length;
        const char* string = luaL_checklstring(L, index, &length);

        return std::string(string, length);
    }

    void luax_pushstring(lua_State* L, std::string_view string)
    {
        lua_pushlstring(L, string.data(), string.size());
    }

    void luax_pushpointerasstring(lua_State* L, const void* pointer)
    {
        char string[sizeof(void*)] {};
        std::memcpy(string, &pointer, sizeof(void*));

        lua_pushlstring(L, string, sizeof(void*));
    }

    bool luax_checkboolflag(lua_State* L, int tableIndex, const char* key)
    {
        lua_getfield(L, tableIndex, key);

        bool result = false;

        if (lua_type(L, -1) != LUA_TBOOLEAN)
        {
            auto error = std::format("expected boolean field '{:s}' in table", key);
            luaL_argerror(L, tableIndex, error.c_str());
        }
        else
            result = luax_toboolean(L, -1);

        lua_pop(L, 1);

        return result;
    }

    bool luax_boolflag(lua_State* L, int index, const char* name, bool default_value)
    {
        lua_getfield(L, index, name);

        bool result;

        if (lua_isnoneornil(L, -1))
            result = default_value;
        else
            result = lua_toboolean(L, -1) != 0;

        lua_pop(L, 1);

        return result;
    }

    int luax_checkintflag(lua_State* L, int tableIndex, const char* key)
    {
        lua_getfield(L, tableIndex, key);

        int result = 0;

        if (!lua_isnumber(L, -1))
        {
            auto error = std::format("expected integer field '{:s}' in table", key);
            luaL_argerror(L, tableIndex, error.c_str());
        }
        else
            result = luaL_checkinteger(L, -1);

        lua_pop(L, 1);

        return result;
    }

    int luax_intflag(lua_State* L, int index, const char* name, int default_value)
    {
        lua_getfield(L, index, name);

        int result;

        if (!lua_isnumber(L, -1))
            result = default_value;
        else
            result = lua_tointeger(L, -1);

        lua_pop(L, 1);

        return result;
    }

    int luax_enumerror(lua_State* L, const char* enumName, const char* value)
    {
        return luaL_error(L, "Invalid %s: %s", enumName, value);
    }

    int luax_ioerror(lua_State* L, const char* format, ...)
    {
        std::va_list args;
        va_start(args, format);

        lua_pushnil(L);
        lua_pushvfstring(L, format, args);

        va_end(args);

        return 2;
    }

    size_t luax_objlen(lua_State* L, int index)
    {
#if LUA_VERSION_NUM == 501
        return lua_objlen(L, index);
#else
        return lua_rawlen(L, index);
#endif
    }

    int luax_table_insert(lua_State* L, int tableIndex, int vIndex, int position)
    {
        if (tableIndex < 0)
            tableIndex = lua_gettop(L) + 1 + tableIndex;

        if (vIndex < 0)
            vIndex = lua_gettop(L) + 1 + vIndex;

        if (position == -1)
        {
            lua_pushvalue(L, vIndex);
            lua_rawseti(L, tableIndex, (int)luax_objlen(L, tableIndex) + 1);

            return 0;
        }
        else if (position < 0)
            position = (int)luax_objlen(L, tableIndex) + 1 + position;

        for (int i = (int)luax_objlen(L, tableIndex) + 1; i > position; i--)
        {
            lua_rawgeti(L, tableIndex, i - 1);
            lua_rawseti(L, tableIndex, i);
        }

        lua_pushvalue(L, vIndex);
        lua_rawseti(L, tableIndex, position);

        return 0;
    }

    int luax_require(lua_State* L, const char* name)
    {
        lua_getglobal(L, "require");
        lua_pushstring(L, name);
        lua_call(L, 1, 1);

        return 1;
    }

    bool luax_isarrayoftables(lua_State* L, int index)
    {
        if (!lua_istable(L, index))
            return false;

        lua_rawgeti(L, index, 1);
        bool tableOfTables = lua_istable(L, -1);

        lua_pop(L, 1);

        return tableOfTables;
    }

    int luax_assert_nilerror(lua_State* L, int index)
    {
        if (lua_isnoneornil(L, index))
        {
            if (lua_isstring(L, index + 1))
                return luaL_error(L, lua_tostring(L, index + 1));
            else
                return luaL_error(L, "assertion failed!");
        }

        return 0;
    }

    int luax_getfunction(lua_State* L, const char* module, const char* function)
    {
        lua_getglobal(L, "love");
        if (lua_isnil(L, -1))
            return luaL_error(L, "Could not find global love!");

        lua_getfield(L, -1, module);
        if (lua_isnil(L, -1))
            return luaL_error(L, "Could not find module love.%s!", module);

        lua_getfield(L, -1, function);
        if (lua_isnil(L, -1))
            return luaL_error(L, "Could not find function love.%s.%s!", module, function);

        lua_remove(L, -2);
        lua_remove(L, -2);

        return 0;
    }

    int luax_convobj(lua_State* L, int index, const char* module, const char* function)
    {
        if (index < 0 && index > LUA_REGISTRYINDEX)
            index += lua_gettop(L) + 1;

        luax_getfunction(L, module, function);
        lua_pushvalue(L, index);
        lua_call(L, 1, 2);
        luax_assert_nilerror(L, -2);
        lua_pop(L, 1);
        lua_replace(L, index);

        return 0;
    }

    int luax_convobj(lua_State* L, std::span<int> indices, const char* module, const char* function)
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "luax_convobj() called with module=%s, function=%s, indices.size()=%zu\n", 
                    module, function, indices.size());
            fflush(logFile);
            fclose(logFile);
        }
#endif

        luax_getfunction(L, module, function);

#ifdef __WIIU__
        FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile2) {
            fprintf(logFile2, "luax_convobj() - got function, about to push %zu values\n", indices.size());
            fflush(logFile2);
            fclose(logFile2);
        }
#endif

        for (int index : indices)
            lua_pushvalue(L, index);

#ifdef __WIIU__
        FILE* logFile3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile3) {
            fprintf(logFile3, "luax_convobj() - about to call function with %d arguments\n", (int)indices.size());
            fflush(logFile3);
            fclose(logFile3);
        }
#endif

        lua_call(L, (int)indices.size(), 2);

#ifdef __WIIU__
        FILE* logFile4 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile4) {
            fprintf(logFile4, "luax_convobj() - function call completed, checking for errors\n");
            fflush(logFile4);
            fclose(logFile4);
        }
#endif

        luax_assert_nilerror(L, -2);
        lua_pop(L, 1);

        if (indices.size() > 0)
            lua_replace(L, indices[0]);

#ifdef __WIIU__
        FILE* logFile5 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile5) {
            fprintf(logFile5, "luax_convobj() - completed successfully\n");
            fflush(logFile5);
            fclose(logFile5);
        }
#endif

        return 0;
    }

    void luax_gettypemetatable(lua_State* L, const Type& type)
    {
        const char* name = type.getName();
        lua_getfield(L, LUA_REGISTRYINDEX, name);
    }

    void luax_runwrapper(lua_State* L, const char* filedata, size_t datalen, const char* filename,
                         const Type& type)
    {
        luax_gettypemetatable(L, type);

        if (lua_istable(L, -1))
        {
            const auto name = std::format("=[love \"{:s}\"]", filename);
            luaL_loadbuffer(L, filedata, datalen, name.c_str());
            lua_pushvalue(L, -2);
            lua_pushnil(L);
            lua_call(L, 2, 0);
        }

        lua_pop(L, 1);
    }

    // #endregion

    // #region Registry

    int luax_register_module(lua_State* L, const WrappedModule& module)
    {
#ifdef __WIIU__
        uint64_t regStart = OSGetSystemTime();
        {
            FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
            if (f) { fprintf(f, "[LUA_REG] ENTER name=%s instance=%p type=%s\n", module.name, (void*)module.instance, module.instance?module.instance->getName():"(null)"); fclose(f);}        
        }
#endif
        module.type->initialize();
#ifdef __WIIU__
        {
            FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
            if (f) { fprintf(f, "[LUA_REG] after type->initialize()\n"); fclose(f);}        
        }
#endif

        luax_insistregistry(L, REGISTRY_MODULES);
        Proxy* proxy = (Proxy*)lua_newuserdata(L, sizeof(Proxy));

        proxy->object = module.instance;
        proxy->type   = module.type;

        luaL_newmetatable(L, module.instance->getName());
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, w__gc);
        lua_setfield(L, -2, "__gc");

        lua_setmetatable(L, -2);
        lua_setfield(L, -2, module.name);
    lua_pop(L, 1);
#ifdef __WIIU__
    {
        FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f) { fprintf(f, "[LUA_REG] proxy/metatable setup complete\n"); fclose(f);}        
    }
#endif

    // Ensure we have the global 'love' table and capture its absolute index (Lua 5.1-compatible)
    luax_insistglobal(L, "love");
    int loveidx = lua_gettop(L); // 'love' table is now at the top
#ifdef __WIIU__
    {
        FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f) { fprintf(f, "[LUA_REG] obtained global 'love' table at idx=%d\n", loveidx); fclose(f);}        
    }
#endif

    // Create the module table and capture its absolute index (top)
    lua_newtable(L);
    int moduleidx = lua_gettop(L);
#ifdef __WIIU__
    {
        FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f) { fprintf(f, "[LUA_REG] created module table idx=%d\n", moduleidx); fclose(f);}        
    }
#endif

        // Register functions into the module table currently at moduleidx
    if (!module.functions.empty()) {
#ifdef __WIIU__
        FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f) { fprintf(f, "[LUA_REG] registering %zu module.functions\n", module.functions.size()); fclose(f);}        
#endif
        luax_register_type_inner(L, module.functions);
#ifdef __WIIU__
        {
        FILE* f2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f2) { fprintf(f2, "[LUA_REG] module.functions registration complete\n"); fclose(f2);}        
        }
#endif
    }

    if (!module.platformFunctions.empty()) {
#ifdef __WIIU__
        FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f) { fprintf(f, "[LUA_REG] registering %zu platformFunctions\n", module.platformFunctions.size()); fclose(f);}        
#endif
        luax_register_type_inner(L, module.platformFunctions);
#ifdef __WIIU__
        {
        FILE* f2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f2) { fprintf(f2, "[LUA_REG] platformFunctions registration complete\n"); fclose(f2);}        
        }
#endif
    }

    if (!module.types.empty()) {
#ifdef __WIIU__
        FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f) { fprintf(f, "[LUA_REG] registering %zu types\n", module.types.size()); fclose(f);}        
#endif
        luax_register_types(L, module.types);
#ifdef __WIIU__
        {
        FILE* f2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f2) { fprintf(f2, "[LUA_REG] types registration complete\n"); fclose(f2);}        
        }
#endif
    }

    // Set love[module.name] = module table using captured indices to avoid stack index bugs
    lua_pushvalue(L, moduleidx);
    lua_setfield(L, loveidx, module.name);
#ifdef __WIIU__
    {
        FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f) { fprintf(f, "[LUA_REG] love.%s assigned\n", module.name); fclose(f);}        
    }
#endif

    // Remove the 'love' table from the stack, leaving the module table to be returned
    lua_remove(L, loveidx);
#ifdef __WIIU__
    {
        uint64_t regEnd = OSGetSystemTime();
        double ms = (double)OSTicksToMilliseconds(regEnd - regStart);
        FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f) { fprintf(f, "[LUA_REG] EXIT name=%s total=%.2f ms\n", module.name, ms); fclose(f);}        
    }
#endif

        return 1;
    }

    void luax_register_type_init(lua_State* L, Type* type)
    {
        type->initialize();

        luax_getregistry(L, REGISTRY_OBJECTS);

        if (!lua_istable(L, -1))
        {
            lua_newtable(L);
            lua_replace(L, -2);

            lua_newtable(L);

            lua_pushliteral(L, "v");
            lua_setfield(L, -2, "__mode");

            lua_setmetatable(L, -2);

            lua_setfield(L, LUA_REGISTRYINDEX, OBJECTS_REGISTRY_KEY);
        }
        else
            lua_pop(L, 1);

        luaL_newmetatable(L, type->getName());

        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, w__gc);
        lua_setfield(L, -2, "__gc");

        lua_pushcfunction(L, w__eq);
        lua_setfield(L, -2, "__eq");

        lua_pushstring(L, type->getName());
        lua_pushcclosure(L, w__tostring, 1);
        lua_setfield(L, -2, "__tostring");

        lua_pushstring(L, type->getName());
        lua_pushcclosure(L, w_type, 1);
        lua_setfield(L, -2, "type");

        lua_pushcfunction(L, w_typeOf);
        lua_setfield(L, -2, "typeOf");

        lua_pushcfunction(L, w_release);
        lua_setfield(L, -2, "release");
#ifdef __WIIU__
        // Log after creating metatable for this type
        FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f) {
            fprintf(f, "[LUA_REG][TYPE_INIT] metatable created for type=%s\n", type->getName());
            fclose(f);
        }
#endif
    }

    void luax_register_type_inner(lua_State* L, std::span<const luaL_Reg> functions)
    {
        for (const auto& registry : functions)
        {
            // Some function tables (e.g., Shader) previously included a null sentinel like { nullptr, nullptr }.
            // The original loop assumed no sentinel. Hitting a nullptr func caused a hang/crash when
            // calling lua_pushcfunction. Guard against this and break on first null to mimic typical
            // luaL_Reg termination semantics.
            if (registry.name == nullptr || registry.func == nullptr)
            {
#ifdef __WIIU__
                FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
                if (f) { fprintf(f, "[LUA_REG][TYPE_INNER] encountered null sentinel, breaking out of functions loop\n"); fclose(f);}            
#endif
                break;
            }
#ifdef __WIIU__
            FILE* f2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
            if (f2) { fprintf(f2, "[LUA_REG][TYPE_INNER] registering method name=%s\n", registry.name); fclose(f2);}            
#endif
            lua_pushcfunction(L, registry.func);
            lua_setfield(L, -2, registry.name);
        }
    }

    void luax_register_types(lua_State* L, std::span<const lua_CFunction> types)
    {
    int idx = 0;
    for (const auto& registry : types) {
#ifdef __WIIU__
        FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f) { fprintf(f, "[LUA_REG][TYPES] calling type index=%d\n", idx); fclose(f);}        
#endif
        registry(L);
#ifdef __WIIU__
        FILE* f2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f2) { fprintf(f2, "[LUA_REG][TYPES] returned from type index=%d\n", idx); fclose(f2);}        
#endif
        idx++;
    }
    }

    int luax_traceback(lua_State* L)
    {
        if (!lua_isstring(L, 1))
            return 1;

        lua_getglobal(L, "debug");
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            return 1;
        }

        lua_getfield(L, -1, "traceback");
        if (!lua_isfunction(L, -1))
        {
            lua_pop(L, 2);
            return 1;
        }

        lua_pushvalue(L, 1);
        lua_pushinteger(L, 2);
        lua_call(L, 2, 1);

        return 1;
    }

    Reference* luax_refif(lua_State* L, int type)
    {
        Reference* r = nullptr;

        // Create a reference only if the test succeeds.
        if (lua_type(L, -1) == type)
            r = new Reference(L);
        else // Pop the value manually if it fails (done by Reference if it succeeds).
            lua_pop(L, 1);

        return r;
    }
    // #endregion
} // namespace love

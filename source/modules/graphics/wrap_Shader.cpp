#include <modules/graphics/wrap_Shader.hpp>
#include <common/luax.hpp>
#include <modules/graphics/Shader.hpp>

using namespace love;

ShaderBase* Wrap_Shader::CheckShader(lua_State* L, int index)
{
#if defined(__WIIU__) && (__DEBUG__)
    FILE* logFile = fopen("fs:/vol/external01/simple_debug.log", "a");
    if (logFile) {
        fprintf(logFile, "[WII U DEBUG] Wrap_Shader::CheckShader() CALLED with index=%d\n", index);
        fprintf(logFile, "[WII U DEBUG] Lua state: %p\n", L);
        fflush(logFile);
        fclose(logFile);
    }
#endif

    try {
        ShaderBase* result = luax_checktype<ShaderBase>(L, index);
        
#if defined(__WIIU__) && (__DEBUG__)
        FILE* logFile2 = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile2) {
            fprintf(logFile2, "[WII U DEBUG] Wrap_Shader::CheckShader() - luax_checktype returned: %p\n", result);
            fflush(logFile2);
            fclose(logFile2);
        }
#endif

        return result;
        
    } catch (const std::exception& e) {
#if defined(__WIIU__) && (__DEBUG__)
        FILE* logFile3 = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile3) {
            fprintf(logFile3, "[WII U DEBUG] Wrap_Shader::CheckShader() - EXCEPTION: %s\n", e.what());
            fflush(logFile3);
            fclose(logFile3);
        }
#endif
        throw;
    } catch (...) {
#if defined(__WIIU__) && (__DEBUG__)
        FILE* logFile4 = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile4) {
            fprintf(logFile4, "[WII U DEBUG] Wrap_Shader::CheckShader() - UNKNOWN EXCEPTION\n");
            fflush(logFile4);
            fclose(logFile4);
        }
#endif
        throw;
    }
}

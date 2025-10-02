#include <modules/graphics/wrap_Shader.hpp>
#include <modules/graphics/Shader.tcc>

using namespace love;

int Wrap_Shader::send(lua_State* L)
{
    // LÖVE API: shader:send(name, ...)
    // For Wii U stub we accept any inputs and report success to avoid script errors.
    // Args: 1 = self (Shader), 2 = name (string), 3..n = values
    if (!lua_isuserdata(L, 1))
        return luaL_error(L, "Expected Shader userdata as self");
    if (lua_type(L, 2) != LUA_TSTRING)
        return luaL_error(L, "Expected uniform name (string)");

    // No-op: we don't actually set uniforms in this stub path.
    // Return true to indicate 'success' like LÖVE does.
    lua_pushboolean(L, 1);
    return 1;
}

int Wrap_Shader::hasUniform(lua_State* L)
{
    // Args: 1 = self (Shader), 2 = name (string)
    // Safe behavior: report false when reflection isn't wired.
    lua_pushboolean(L, false);
    return 1;
}

int Wrap_Shader::getWarnings(lua_State* L)
{
    // Return an empty string; Wii U shaders don't expose compile warnings here.
    lua_pushstring(L, "");
    return 1;
}

// Register minimal set of Shader methods so Lua code can call them safely.
// NOTE: Do NOT add a null sentinel here; our luax_register_type_inner now guards but
// other tables omit sentinels and rely on known array length via std::span.
const luaL_Reg Wrap_Shader::functions[] = {
    { "send",       Wrap_Shader::send },
    { "hasUniform", Wrap_Shader::hasUniform },
    { "getWarnings",Wrap_Shader::getWarnings }
};

int love::open_shader(lua_State* L)
{
    // Bind methods to the Shader type so userdata pushed by luax_pushtype has these methods.
#ifdef __WIIU__
    FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
    if (f) { fprintf(f, "[LUA_REG][SHADER] open_shader ENTER\n"); fclose(f);}    
#endif
    int r = luax_register_type(L, &ShaderBase::type, Wrap_Shader::functions);
#ifdef __WIIU__
    FILE* f2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
    if (f2) { fprintf(f2, "[LUA_REG][SHADER] open_shader EXIT ret=%d\n", r); fclose(f2);}    
#endif
    return r;
}

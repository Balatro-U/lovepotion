#pragma once

#include <common/luax.hpp>

namespace love
{
    int open_shader(lua_State* L);

    namespace Wrap_Shader
    {
        int send(lua_State* L);
        int hasUniform(lua_State* L);
    int getWarnings(lua_State* L);
        extern const luaL_Reg functions[];
    }
}

#include "common/screen.hpp"
#include "common/config.hpp" // for LOVE_UNUSED macro if needed

#include "modules/graphics/wrap_Graphics.hpp"

#include "modules/filesystem/wrap_Filesystem.hpp"

#include "modules/graphics/wrap_Font.hpp"
#include "modules/graphics/wrap_Quad.hpp"
#include "modules/graphics/wrap_Shader.hpp"
#include "modules/graphics/wrap_SpriteBatch.hpp"
#include "modules/graphics/wrap_TextBatch.hpp"
#include "modules/graphics/wrap_Texture.hpp"
#include "modules/video/wrap_Video.hpp"

#include "modules/image/Image.hpp"
#include "modules/image/ImageData.hpp"
#include "modules/image/wrap_CompressedImageData.hpp"
#include "modules/image/wrap_Image.hpp"
#include "modules/image/wrap_ImageData.hpp"

#include <fstream>
#include <cctype>
#include <string>

#ifdef __WIIU__
#include <coreinit/time.h>
#endif

#ifdef USE_CAFEGLSL
#include "common/CafeGLSL.hpp"
#endif
#ifdef __WIIU__
#include "modules/filesystem/physfs/Filesystem.hpp"
#include "modules/filesystem/FileData.hpp"
#include "common/StrongRef.hpp"
#include "modules/graphics/Shader.hpp"
#endif

using namespace love;

#define instance() (Module::getInstance<Graphics>(Module::M_GRAPHICS))

// Love2D shader conversion function
static std::string ProcessLove2DShader(const std::string& source)
{
    std::string processed = source;
    
    // Check if this is a Love2D effect function shader
    bool hasEffectFunction = processed.find("vec4 effect(") != std::string::npos;
    
    if (hasEffectFunction) {
        // This is a Love2D effect shader - convert it to GLSL main function
        std::string glslShader = R"(#version 150 core
#define number float
#define Image sampler2D
#define ArrayImage sampler2DArray  
#define VolumeImage sampler3D

uniform sampler2D MainTex;
uniform vec2 love_ScreenSize;
varying vec2 VaryingTexCoord;
varying vec4 VaryingColor;

// Love2D Texel function
vec4 Texel(sampler2D tex, vec2 coords) {
    return texture2D(tex, coords);
}

)";
        
        // Add the original Love2D shader code
        glslShader += processed;
        
        // Add GLSL main function that calls effect()
        glslShader += R"(

void main() {
    gl_FragColor = effect(VaryingColor, MainTex, VaryingTexCoord, gl_FragCoord.xy);
}
)";
        
        return glslShader;
    } else {
        // Already GLSL or simple shader - do basic conversions
        size_t pos = 0;
        
        // Convert Love2D types and functions
        while ((pos = processed.find("number ", pos)) != std::string::npos) {
            processed.replace(pos, 7, "float ");
            pos += 6;
        }
        
        pos = 0;
        while ((pos = processed.find("Image ", pos)) != std::string::npos) {
            processed.replace(pos, 6, "sampler2D ");
            pos += 10;
        }
        
        // Texel function calls
        pos = 0;
        while ((pos = processed.find("Texel(", pos)) != std::string::npos) {
            processed.replace(pos, 6, "texture2D(");
            pos += 10;
        }
        
        // Add basic GLSL header if missing
        if (processed.find("#version") == std::string::npos) {
            processed = "#version 150 core\n" + processed;
        }
        
        return processed;
    }
}

static int luax_checkgraphicscreated(lua_State* L)
{
    if (!instance()->isCreated())
    {
#ifdef __WIIU__
        // On Wii U, graphics can work without a traditional window
        // so we just return without error
        return 0;
#else
        luaL_error(L, "love.graphics cannot function without a window!");
#endif
    }

    return 0;
}

int Wrap_Graphics::reset(lua_State* L)
{
    (void)L; // avoid unused warning without relying on macro
    instance()->reset();
    return 0;
}

int Wrap_Graphics::clear(lua_State* L)
{
    OptionalColor color(Color(0, 0, 0, 0));
    std::vector<OptionalColor> colors {};

    OptionalInt stencil(0);
    OptionalDouble depth(1.0);

    int argType = lua_type(L, 1);
    int start   = -1;

    if (argType == LUA_TTABLE)
    {
        int max = lua_gettop(L);
        colors.reserve(max);

        for (int index = 0; index < max; index++)
        {
            argType = lua_type(L, index + 1);

            if (argType == LUA_TNUMBER || argType == LUA_TBOOLEAN)
            {
                start = index + 1;
                break;
            }
            else if (argType == LUA_TNIL || argType == LUA_TNONE || luax_objlen(L, index + 1) == 0)
            {
                colors.push_back(OptionalColor());
                continue;
            }

            for (int j = 1; j <= 4; j++)
                lua_rawgeti(L, index + 1, j);

            OptionalColor _color {};
            _color.hasValue = true;

            _color.value.r = luaL_checknumber(L, -4);
            _color.value.g = luaL_checknumber(L, -3);
            _color.value.b = luaL_checknumber(L, -2);
            _color.value.a = luaL_checknumber(L, -1);

            colors.push_back(_color);

            lua_pop(L, 4);
        }
    }
    else if (argType == LUA_TBOOLEAN)
    {
        color.hasValue = luax_toboolean(L, 1);
        start          = 2;
    }
    else if (argType != LUA_TNONE && argType != LUA_TNIL)
    {
        color.hasValue = true;

        color.value.r = luaL_checknumber(L, 1);
        color.value.g = luaL_checknumber(L, 2);
        color.value.b = luaL_checknumber(L, 3);
        color.value.a = luaL_optnumber(L, 4, 1.0);

        start = 5;
    }

    if (start >= 0)
    {
        argType = lua_type(L, start);

        if (argType == LUA_TBOOLEAN)
            stencil.hasValue = luax_toboolean(L, start);
        else if (argType == LUA_TNUMBER)
            stencil.value = luaL_checkinteger(L, start);

        argType = lua_type(L, start + 1);

        if (argType == LUA_TBOOLEAN)
            depth.hasValue = luax_toboolean(L, start + 1);
        else if (argType == LUA_TNUMBER)
            depth.value = luaL_checknumber(L, start + 1);
    }

    if (colors.empty())
        luax_catchexcept(L, [&]() { instance()->clear(color, stencil, depth); });
    else
        luax_catchexcept(L, [&]() { instance()->clear(colors, stencil, depth); });

    return 0;
}

int Wrap_Graphics::present(lua_State* L)
{
#ifdef __WIIU__
    static int presentCount = 0;
    presentCount++;
    if (presentCount <= 5 || presentCount % 60 == 0) {
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "present() called from Lua (#%d)\n", presentCount);
            fflush(logFile);
            fclose(logFile);
        }
    }
#endif
    luax_catchexcept(L, [&]() { instance()->present(L); });

    return 0;
}

int Wrap_Graphics::setColor(lua_State* L)
{
#ifdef __WIIU__
    static int setColorCount = 0;
    setColorCount++;
    if (setColorCount <= 10 || setColorCount % 120 == 0) {
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "setColor() called from Lua (#%d)\n", setColorCount);
            fflush(logFile);
            fclose(logFile);
        }
    }
#endif
    Color color {};

    if (lua_istable(L, 1))
    {
        for (int index = 1; index <= 4; index++)
            lua_rawgeti(L, 1, index);

        color.r = luax_checknumberclamped01(L, -4);
        color.g = luax_checknumberclamped01(L, -3);
        color.b = luax_checknumberclamped01(L, -2);
        color.a = luax_optnumberclamped01(L, -1, 1.0);

        lua_pop(L, 4);
    }
    else
    {
        color.r = luax_checknumberclamped01(L, 1);
        color.g = luax_checknumberclamped01(L, 2);
        color.b = luax_checknumberclamped01(L, 3);
        color.a = luax_optnumberclamped01(L, 4, 1.0);
    }

    instance()->setColor(color);

    return 0;
}

int Wrap_Graphics::getColor(lua_State* L)
{
    Color color = instance()->getColor();

    lua_pushnumber(L, color.r);
    lua_pushnumber(L, color.g);
    lua_pushnumber(L, color.b);
    lua_pushnumber(L, color.a);

    return 4;
}

int Wrap_Graphics::setBackgroundColor(lua_State* L)
{
#ifdef __WIIU__
    FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile) {
        fprintf(logFile, "setBackgroundColor() called from Lua\n");
        fflush(logFile);
        fclose(logFile);
    }
#endif
    Color color {};

    if (lua_istable(L, 1))
    {
        for (int index = 1; index <= 4; index++)
            lua_rawgeti(L, 1, index);

        color.r = luaL_checknumber(L, -4);
        color.g = luaL_checknumber(L, -3);
        color.b = luaL_checknumber(L, -2);
        color.a = luaL_optnumber(L, -1, 1.0);

        lua_pop(L, 4);
    }
    else
    {
        color.r = luaL_checknumber(L, 1);
        color.g = luaL_checknumber(L, 2);
        color.b = luaL_checknumber(L, 3);
        color.a = luaL_optnumber(L, 4, 1.0);
    }

    instance()->setBackgroundColor(color);

    return 0;
}

int Wrap_Graphics::getBackgroundColor(lua_State* L)
{
    auto color = instance()->getBackgroundColor();

    lua_pushnumber(L, color.r);
    lua_pushnumber(L, color.g);
    lua_pushnumber(L, color.b);
    lua_pushnumber(L, color.a);

    return 4;
}

int Wrap_Graphics::setColorMask(lua_State* L)
{
    ColorChannelMask mask {};

    if (lua_isnoneornil(L, 1))
        mask.r = mask.g = mask.b = mask.a = true;
    else if (lua_gettop(L) <= 1)
    {
        mask.r = mask.g = mask.b = mask.a = luax_checkboolean(L, 1);
    }
    else
    {
        mask.r = luax_toboolean(L, 1);
        mask.g = luax_toboolean(L, 2);
        mask.b = luax_toboolean(L, 3);
        mask.a = luax_toboolean(L, 4);
    }

    instance()->setColorMask(mask);

    return 0;
}

int Wrap_Graphics::getColorMask(lua_State* L)
{
    auto mask = instance()->getColorMask();

    lua_pushboolean(L, mask.r);
    lua_pushboolean(L, mask.g);
    lua_pushboolean(L, mask.b);
    lua_pushboolean(L, mask.a);

    return 4;
}

static BlendOperation luax_checkblendop(lua_State* L, int idx)
{
    auto blendOperation = BLENDOP_ADD;
    const char* str     = luaL_checkstring(L, idx);

    if (!love::getConstant(str, blendOperation))
        luax_enumerror(L, "blend operation", BlendOperations, str);

    return blendOperation;
}

static BlendFactor luax_checkblendfactor(lua_State* L, int idx)
{
    BlendFactor factor = BLENDFACTOR_ZERO;
    const char* str    = luaL_checkstring(L, idx);

    if (!love::getConstant(str, factor))
        luax_enumerror(L, "blend factor", BlendFactors, str);

    return factor;
}

static void luax_pushblendop(lua_State* L, BlendOperation operation)
{
    std::string_view name;
    if (!love::getConstant(operation, name))
        luaL_error(L, "unknown blend operation");

    luax_pushstring(L, name);
}

static void luax_pushblendfactor(lua_State* L, BlendFactor factor)
{
    std::string_view name;
    if (!getConstant(factor, name))
        luaL_error(L, "unknown blend factor");

    luax_pushstring(L, name);
}

int Wrap_Graphics::setBlendMode(lua_State* L)
{
    BlendMode mode;
    const char* name = luaL_checkstring(L, 1);

    if (!love::getConstant(name, mode))
        return luax_enumerror(L, "blend mode", BlendModes, name);

    auto alphaMode = BLENDALPHA_MULTIPLY;
    if (!lua_isnoneornil(L, 2))
    {
        const char* alphaName = luaL_checkstring(L, 2);
        if (!love::getConstant(alphaName, alphaMode))
            return luax_enumerror(L, "blend alpha mode", BlendAlphaModes, alphaName);
    }

    luax_catchexcept(L, [&]() { instance()->setBlendMode(mode, alphaMode); });

    return 0;
}

int Wrap_Graphics::getBlendMode(lua_State* L)
{
    std::string_view name;
    std::string_view alphaName;

    BlendAlpha alphaMode;
    const auto mode = instance()->getBlendMode(alphaMode);

    if (!love::getConstant(mode, name))
        luaL_error(L, "Unknown blend mode");

    if (!getConstant(alphaMode, alphaName))
        luaL_error(L, "Unknown blend alpha mode");

    luax_pushstring(L, name);
    luax_pushstring(L, alphaName);

    return 2;
}

int Wrap_Graphics::setBlendState(lua_State* L)
{
    BlendState state {};

    if (!lua_isnoneornil(L, 1))
    {
        state.enable = true;

        if (lua_gettop(L) >= 4)
        {
            state.operationRGB = luax_checkblendop(L, 1);
            state.operationA   = luax_checkblendop(L, 2);
            state.srcFactorRGB = luax_checkblendfactor(L, 3);
            state.srcFactorA   = luax_checkblendfactor(L, 4);
            state.dstFactorRGB = luax_checkblendfactor(L, 5);
            state.dstFactorA   = luax_checkblendfactor(L, 6);
        }
        else
        {
            state.operationRGB = state.operationA = luax_checkblendop(L, 1);
            state.srcFactorRGB = state.srcFactorA = luax_checkblendfactor(L, 2);
            state.dstFactorRGB = state.dstFactorA = luax_checkblendfactor(L, 3);
        }
    }

    luax_catchexcept(L, [&]() { instance()->setBlendState(state); });

    return 0;
}

int Wrap_Graphics::getBlendState(lua_State* L)
{
    const auto& state = instance()->getBlendState();

    if (state.enable)
    {
        luax_pushblendop(L, state.operationRGB);
        luax_pushblendop(L, state.operationA);
        luax_pushblendfactor(L, state.srcFactorRGB);
        luax_pushblendfactor(L, state.srcFactorA);
        luax_pushblendfactor(L, state.dstFactorRGB);
        luax_pushblendfactor(L, state.dstFactorA);
    }
    else
    {
        for (int i = 0; i < 6; i++)
            lua_pushnil(L);
    }

    return 6;
}

int Wrap_Graphics::setLineWidth(lua_State* L)
{
    float width = luaL_checknumber(L, 1);

    luax_catchexcept(L, [&]() { instance()->setLineWidth(width); });

    return 0;
}

int Wrap_Graphics::getLineWidth(lua_State* L)
{
    lua_pushnumber(L, instance()->getLineWidth());

    return 1;
}

int Wrap_Graphics::setLineJoin(lua_State* L)
{
    Graphics::LineJoin join;
    const char* str = luaL_checkstring(L, 1);

    if (!Graphics::getConstant(str, join))
        return luax_enumerror(L, "line join", Graphics::LineJoins, str);

    instance()->setLineJoin(join);

    return 0;
}

int Wrap_Graphics::getLineJoin(lua_State* L)
{
    Graphics::LineJoin join = instance()->getLineJoin();
    std::string_view name;

    if (!Graphics::getConstant(join, name))
        luaL_error(L, "Unknown line join");

    luax_pushstring(L, name);

    return 1;
}

int Wrap_Graphics::setLineStyle(lua_State* L)
{
    Graphics::LineStyle style;
    const char* str = luaL_checkstring(L, 1);

    if (!Graphics::getConstant(str, style))
        return luax_enumerror(L, "line style", Graphics::LineStyles, str);

    instance()->setLineStyle(style);

    return 0;
}

int Wrap_Graphics::getLineStyle(lua_State* L)
{
    Graphics::LineStyle style = instance()->getLineStyle();
    std::string_view name;

    if (!Graphics::getConstant(style, name))
        luaL_error(L, "Unknown line style");

    luax_pushstring(L, name);

    return 1;
}

int Wrap_Graphics::setPointSize(lua_State* L)
{
    float size = luaL_checknumber(L, 1);
    instance()->setPointSize(size);

    return 0;
}

int Wrap_Graphics::getPointSize(lua_State* L)
{
    lua_pushnumber(L, instance()->getPointSize());

    return 1;
}

int Wrap_Graphics::setMeshCullMode(lua_State* L)
{
    CullMode mode;
    const char* str = luaL_checkstring(L, 1);

    if (!love::getConstant(str, mode))
        return luax_enumerror(L, "cull mode", love::CullModes, str);

    instance()->setMeshCullMode(mode);

    return 0;
}

int Wrap_Graphics::getMeshCullMode(lua_State* L)
{
    auto cullMode = instance()->getMeshCullMode();
    std::string_view name;

    if (!love::getConstant(cullMode, name))
        luaL_error(L, "Unknown cull mode");

    luax_pushstring(L, name);

    return 1;
}

int Wrap_Graphics::setFrontFaceWinding(lua_State* L)
{
    Winding winding;
    const char* str = luaL_checkstring(L, 1);

    if (!love::getConstant(str, winding))
        return luax_enumerror(L, "winding", love::WindingModes, str);

    instance()->setFrontFaceWinding(winding);

    return 0;
}

int Wrap_Graphics::getFrontFaceWinding(lua_State* L)
{
    auto winding = instance()->getFrontFaceWinding();
    std::string_view name;

    if (!love::getConstant(winding, name))
        luaL_error(L, "Unknown winding");

    luax_pushstring(L, name);

    return 1;
}

int Wrap_Graphics::isCreated(lua_State* L)
{
#ifdef __WIIU__
    // On Wii U, return boolean value instead of error
    lua_pushboolean(L, instance()->isCreated());
    return 1;
#else
    if (!instance()->isCreated())
        return luaL_error(L, "love.graphics cannot function without a window!");

    return 0;
#endif
}

int Wrap_Graphics::isActive(lua_State* L)
{
    lua_pushboolean(L, instance()->isActive());

    return 1;
}

int Wrap_Graphics::isGammaCorrect(lua_State* L)
{
    lua_pushboolean(L, love::isGammaCorrect());

    return 1;
}

int Wrap_Graphics::getRendererInfo(lua_State* L)
{
    auto info = instance()->getRendererInfo();

    lua_pushstring(L, info.name.c_str());
    lua_pushstring(L, info.version.c_str());
    lua_pushstring(L, info.vendor.c_str());
    lua_pushstring(L, info.device.c_str());

    return 4;
}

int Wrap_Graphics::getDPIScale(lua_State* L)
{
    lua_pushnumber(L, instance()->getScreenDPIScale());

    return 1;
}

int Wrap_Graphics::setScissor(lua_State* L)
{
    int argc = lua_gettop(L);

    if (argc == 0 || (argc == 4 && lua_isnil(L, 1) && lua_isnil(L, 2) && lua_isnil(L, 3) && lua_isnil(L, 4)))
    {
        instance()->setScissor();
        return 0;
    }

    Rect scissor {};
    scissor.x = luaL_checkinteger(L, 1);
    scissor.y = luaL_checkinteger(L, 2);
    scissor.w = luaL_checkinteger(L, 3);
    scissor.h = luaL_checkinteger(L, 4);

    if (scissor.w < 0 || scissor.h < 0)
        return luaL_error(L, "Can't set scissor with negative width and/or height.");

    instance()->setScissor(scissor);

    return 0;
}

int Wrap_Graphics::intersectScissor(lua_State* L)
{
    Rect scissor {};
    scissor.x = luaL_checkinteger(L, 1);
    scissor.y = luaL_checkinteger(L, 2);
    scissor.w = luaL_checkinteger(L, 3);
    scissor.h = luaL_checkinteger(L, 4);

    if (scissor.w < 0 || scissor.h < 0)
        return luaL_error(L, "Can't set scissor with negative width and/or height.");

    instance()->intersectScissor(scissor);

    return 0;
}

int Wrap_Graphics::getScissor(lua_State* L)
{
    Rect scissor {};
    if (!instance()->getScissor(scissor))
        return 0;

    lua_pushinteger(L, scissor.x);
    lua_pushinteger(L, scissor.y);
    lua_pushinteger(L, scissor.w);
    lua_pushinteger(L, scissor.h);

    return 4;
}

int Wrap_Graphics::getStackDepth(lua_State* L)
{
    lua_pushinteger(L, instance()->getStackDepth());

    return 1;
}

int Wrap_Graphics::push(lua_State* L)
{
    auto stackType   = Graphics::STACK_TRANSFORM;
    const char* name = lua_isnoneornil(L, 1) ? nullptr : luaL_checkstring(L, 1);

    if (name && !Graphics::getConstant(name, stackType))
        return luax_enumerror(L, "graphics stack type", Graphics::StackTypes, name);

    luax_catchexcept(L, [&]() { instance()->push(stackType); });

    /* TODO: check transform type */

    return 0;
}

int Wrap_Graphics::pop(lua_State* L)
{
    luax_catchexcept(L, [&]() { instance()->pop(); });

    return 0;
}

int Wrap_Graphics::rotate(lua_State* L)
{
    float angle = luaL_checknumber(L, 1);
    instance()->rotate(angle);

    return 0;
}

int Wrap_Graphics::scale(lua_State* L)
{
    float sx = luaL_checknumber(L, 1);
    float sy = luaL_optnumber(L, 2, sx);

    instance()->scale(sx, sy);

    return 0;
}

int Wrap_Graphics::translate(lua_State* L)
{
    float dx = luaL_checknumber(L, 1);
    float dy = luaL_checknumber(L, 2);

    instance()->translate(dx, dy);

    return 0;
}

int Wrap_Graphics::shear(lua_State* L)
{
    float kx = luaL_checknumber(L, 1);
    float ky = luaL_checknumber(L, 2);

    instance()->shear(kx, ky);

    return 0;
}

int Wrap_Graphics::origin(lua_State*)
{
    instance()->origin();

    return 0;
}

int Wrap_Graphics::applyTransform(lua_State* L)
{
    // clang-format off
    luax_checkstandardtransform(L, 1, [&](const Matrix4& transform) {
        instance()->applyTransform(transform);
    });
    // clang-format on

    return 0;
}

int Wrap_Graphics::replaceTransform(lua_State* L)
{
    // clang-format off
    luax_checkstandardtransform(L, 1, [&](const Matrix4& transform) {
        instance()->replaceTransform(transform);
    });
    // clang-format on

    return 0;
}

int Wrap_Graphics::transformPoint(lua_State* L)
{
    Vector2 point {};
    point.x = luaL_checknumber(L, 1);
    point.y = luaL_checknumber(L, 2);

    point = instance()->transformPoint(point);

    lua_pushnumber(L, point.x);
    lua_pushnumber(L, point.y);

    return 2;
}

int Wrap_Graphics::inverseTransformPoint(lua_State* L)
{
    Vector2 point {};
    point.x = luaL_checknumber(L, 1);
    point.y = luaL_checknumber(L, 2);

    point = instance()->inverseTransformPoint(point);

    lua_pushnumber(L, point.x);
    lua_pushnumber(L, point.y);

    return 2;
}

static void parseDPIScale(Data*, float*)
{}

// clang-format off
static std::pair<StrongRef<ImageData>, StrongRef<CompressedImageData>>
getImageData(lua_State* L, int index, bool allowCompressed, float* dpiScale)
{
    StrongRef<ImageData> imageData;
    StrongRef<CompressedImageData> compressedImageData;

    if (luax_istype(L, index, ImageData::type))
        imageData.set(luax_checkimagedata(L, index));
    else if (luax_istype(L, index, CompressedImageData::type))
        compressedImageData.set(luax_checkcompressedimagedata(L, index));
    else if (luax_cangetdata(L, index))
    {
        auto* module = Module::getInstance<Image>(Module::M_IMAGE);
        if (module == nullptr)
            luaL_error(L, "Cannot load images without the love.image module.");

        StrongRef<Data> fileData(luax_getdata(L, index), Acquire::NO_RETAIN);

        if (dpiScale != nullptr)
            parseDPIScale(fileData, dpiScale);

        if (allowCompressed && module->isCompressed(fileData))
            luax_catchexcept(L, [&]() { compressedImageData.set(module->newCompressedData(fileData), Acquire::NO_RETAIN); });
        else
            luax_catchexcept(L, [&]() { imageData.set(module->newImageData(fileData), Acquire::NO_RETAIN); });
    }
    else
        imageData.set(luax_checkimagedata(L, index));

    return { imageData, compressedImageData };
}
// clang-format on

static void luax_checktexturesettings(lua_State* L, int index, bool optional, bool checkType,
                                      bool checkDimensions, OptionalBool forceRenderTarget,
                                      Texture::Settings& settings, bool& setDPIScale)
{
    setDPIScale = false;

    if (forceRenderTarget.hasValue)
        settings.renderTarget = forceRenderTarget.value;

    if (optional && lua_isnoneornil(L, index))
        return;

    luax_checktablefields<Texture::SettingType>(L, index, "texture setting name", Texture::getConstant);

    if (!forceRenderTarget.hasValue)
    {
        const char* name      = Texture::getConstant(Texture::SETTING_RENDER_TARGET);
        settings.renderTarget = luax_boolflag(L, index, name, settings.renderTarget);
    }

    lua_getfield(L, index, Texture::getConstant(Texture::SETTING_DEBUGNAME));
    if (!lua_isnoneornil(L, -1))
        settings.debugName = luaL_checkstring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, Texture::getConstant(Texture::SETTING_FORMAT));
    if (!lua_isnoneornil(L, -1))
    {
        const char* formatName = luaL_checkstring(L, -1);
        if (!love::getConstant(formatName, settings.format))
            luax_enumerror(L, "pixel format", formatName);
    }
    lua_pop(L, 1);

    if (checkType)
    {
        lua_getfield(L, index, Texture::getConstant(Texture::SETTING_TYPE));
        if (!lua_isnoneornil(L, -1))
        {
            const char* typeName = luaL_checkstring(L, -1);
            if (!Texture::getConstant(typeName, settings.type))
                luax_enumerror(L, "texture type", Texture::TextureTypes, typeName);
        }
        lua_pop(L, 1);
    }

    // clang-format off
    if (checkDimensions)
    {
        settings.width = luax_checkintflag(L, index, Texture::getConstant(Texture::SETTING_WIDTH));
        settings.height = luax_checkintflag(L, index, Texture::getConstant(Texture::SETTING_HEIGHT));

        if (settings.type == TEXTURE_2D_ARRAY || settings.type == TEXTURE_VOLUME)
            settings.layers = luax_checkintflag(L, index, Texture::getConstant(Texture::SETTING_LAYERS));
    }
    else
    {
        settings.width = luax_intflag(L, index, Texture::getConstant(Texture::SETTING_WIDTH), settings.width);
        settings.height = luax_intflag(L, index, Texture::getConstant(Texture::SETTING_HEIGHT), settings.height);

        if (settings.type == TEXTURE_2D_ARRAY || settings.type == TEXTURE_VOLUME)
            settings.layers = luax_intflag(L, index, Texture::getConstant(Texture::SETTING_LAYERS), settings.layers);
    }
    // clang-format on

    lua_getfield(L, index, Texture::getConstant(Texture::SETTING_MIPMAPS));
    if (!lua_isnoneornil(L, -1))
    {
        if (lua_type(L, -1) == LUA_TBOOLEAN)
            settings.mipmaps = luax_toboolean(L, -1) ? Texture::MIPMAPS_MANUAL : Texture::MIPMAPS_NONE;
        else
        {
            const char* name = luaL_checkstring(L, -1);
            if (!Texture::getConstant(name, settings.mipmaps))
                luax_enumerror(L, "mipmap mode", Texture::MipmapsModes, name);
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, index, Texture::getConstant(Texture::SETTING_MIPMAP_COUNT));
    if (!lua_isnoneornil(L, -1))
        settings.mipmapCount = luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    settings.linear = luax_boolflag(L, index, Texture::getConstant(Texture::SETTING_LINEAR), settings.linear);
    settings.msaa   = luax_intflag(L, index, Texture::getConstant(Texture::SETTING_MSAA), settings.msaa);

    settings.computeWrite =
        luax_boolflag(L, index, Texture::getConstant(Texture::SETTING_COMPUTE_WRITE), settings.computeWrite);

    lua_getfield(L, index, Texture::getConstant(Texture::SETTING_VIEW_FORMATS));
    if (!lua_isnoneornil(L, -1))
    {
        if (lua_type(L, -1) != LUA_TTABLE)
            luaL_argerror(L, index, "expected field 'viewformats' to be a table type");

        for (int index = 1; index <= (int)luax_objlen(L, -1); index++)
        {
            lua_rawgeti(L, -1, index);
            const char* formatName = luaL_checkstring(L, -1);

            PixelFormat format = PIXELFORMAT_UNKNOWN;
            if (!love::getConstant(formatName, format))
                luax_enumerror(L, "pixel format", formatName);

            settings.viewFormats.push_back(format);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, index, Texture::getConstant(Texture::SETTING_READABLE));
    if (!lua_isnoneornil(L, -1))
        settings.readable.set(luax_checkboolean(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, index, Texture::getConstant(Texture::SETTING_DPI_SCALE));
    if (lua_isnumber(L, -1))
    {
        settings.dpiScale = luaL_checknumber(L, -1);
        setDPIScale       = true;
    }
    lua_pop(L, 1);
}

static int pushNewTexture(lua_State* L, TextureBase::Slices* slices, const Texture::Settings& settings)
{
    StrongRef<TextureBase> texture;

#ifdef __WIIU__
    FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile) {
        fprintf(logFile, "pushNewTexture() called - about to create texture\n");
        fprintf(logFile, "  settings.type: %d, slices: %p\n", settings.type, slices);
        fflush(logFile);
        fclose(logFile);
    }
#endif

    // clang-format off
    luax_catchexcept(L,
        [&]() { 
#ifdef __WIIU__
            FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile2) {
                fprintf(logFile2, "pushNewTexture() - calling instance()->newTexture()\n");
                fflush(logFile2);
                fclose(logFile2);
            }
#endif
            texture.set(instance()->newTexture(settings, slices), Acquire::NO_RETAIN);
#ifdef __WIIU__
            FILE* logFile3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile3) {
                fprintf(logFile3, "pushNewTexture() - newTexture() returned: %p\n", texture.get());
                fflush(logFile3);
                fclose(logFile3);
            }
#endif
        },
        [&](bool) { 
#ifdef __WIIU__
            FILE* logFile4 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile4) {
                fprintf(logFile4, "pushNewTexture() - EXCEPTION occurred in newTexture()\n");
                fflush(logFile4);
                fclose(logFile4);
            }
#endif
            if (slices) slices->clear(); 
        }
    );
    // clang-format on

#ifdef __WIIU__
    FILE* logFile5 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile5) {
        fprintf(logFile5, "pushNewTexture() - about to push texture to Lua stack\n");
        fprintf(logFile5, "pushNewTexture() - texture.get() = %p\n", texture.get());
        if (texture.get() == nullptr) {
            fprintf(logFile5, "pushNewTexture() - ERROR: texture is NULL, cannot push to Lua\n");
        } else {
            fprintf(logFile5, "pushNewTexture() - texture is valid, pushing to Lua\n");
        }
        fflush(logFile5);
        fclose(logFile5);
    }
#endif

    if (texture.get() == nullptr) {
#ifdef __WIIU__
        FILE* logFile6 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile6) {
            fprintf(logFile6, "pushNewTexture() - returning luaL_error due to NULL texture\n");
            fflush(logFile6);
            fclose(logFile6);
        }
#endif
        return luaL_error(L, "Texture creation failed");
    }

    luax_pushtype(L, texture);
    
#ifdef __WIIU__
    FILE* logFile7 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile7) {
        fprintf(logFile7, "pushNewTexture() - texture successfully pushed to Lua stack\n");
        fflush(logFile7);
        fclose(logFile7);
    }
#endif
    
    return 1;
}

int Wrap_Graphics::newTexture(lua_State* L)
{
#ifdef __WIIU__
    FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile) {
        fprintf(logFile, "newTexture() called from Lua with %d arguments\n", lua_gettop(L));
        if (lua_gettop(L) > 0) {
            int argType = lua_type(L, 1);
            fprintf(logFile, "  First argument type: %d (%s)\n", argType, lua_typename(L, argType));
            if (argType == LUA_TSTRING) {
                const char* str = lua_tostring(L, 1);
                fprintf(logFile, "  First argument string: %s\n", str ? str : "NULL");
            }
        }
        fflush(logFile);
        fclose(logFile);
    }
#endif
    
    luax_checkgraphicscreated(L);

    Texture::Slices slices(TEXTURE_2D);
    Texture::Slices* slicesRef = &slices;

    Texture::Settings settings {};
    settings.type    = TEXTURE_2D;
    bool dpiScaleSet = false;

    if (lua_type(L, 1) == LUA_TNUMBER)
    {
        slicesRef = nullptr;

        settings.width  = luaL_checkinteger(L, 1);
        settings.height = luaL_checkinteger(L, 2);

        int start = 3;

        if (lua_type(L, 3) == LUA_TNUMBER)
        {
            settings.layers = luaL_checkinteger(L, 3);
            settings.type   = TEXTURE_2D_ARRAY;
            start           = 4;
        }

        luax_checktexturesettings(L, start, true, true, false, OptionalBool(), settings, dpiScaleSet);
    }
    else
    {
        luax_checktexturesettings(L, 2, true, false, false, OptionalBool(), settings, dpiScaleSet);
        float* autoDpiScale = dpiScaleSet ? nullptr : &settings.dpiScale;

        if (lua_istable(L, 1))
        {
            const int length = luax_objlen(L, 1);

            for (int index = 0; index < length; index++)
            {
                lua_rawgeti(L, 1, index + 1);

                auto data = getImageData(L, -1, true, index == 0 ? autoDpiScale : nullptr);

                if (data.first.get())
                    slices.set(0, index, data.first);
                else
                    slices.set(0, index, data.second->getSlice(0, 0));
            }

            lua_pop(L, length);
        }
        else
        {
            auto data = getImageData(L, 1, true, autoDpiScale);

            if (data.first.get())
                slices.set(0, 0, data.first);
            else
                slices.add(data.second, 0, 0, false, settings.mipmaps != Texture::MIPMAPS_NONE);
        }
    }

    return pushNewTexture(L, slicesRef, settings);
}

int Wrap_Graphics::newQuad(lua_State* L)
{
    luax_checkgraphicscreated(L);

    Quad::Viewport viewport {};
    viewport.x = luaL_checknumber(L, 1);
    viewport.y = luaL_checknumber(L, 2);
    viewport.w = luaL_checknumber(L, 3);
    viewport.h = luaL_checknumber(L, 4);

    double sourceWidth  = 0.0f;
    double sourceHeight = 0.0f;

    int layer = 0;

    if (luax_istype(L, 5, TextureBase::type))
    {
        TextureBase* texture = luax_checktexture(L, 5);
        sourceWidth          = texture->getWidth();
        sourceHeight         = texture->getHeight();
    }
    else if (luax_istype(L, 6, TextureBase::type))
    {
        layer                = (int)luaL_checkinteger(L, 5) - 1;
        TextureBase* texture = luax_checktexture(L, 6);
        sourceWidth          = texture->getWidth();
        sourceHeight         = texture->getHeight();
    }
    else if (!lua_isnoneornil(L, 7))
    {
        layer        = (int)luaL_checkinteger(L, 5) - 1;
        sourceWidth  = luaL_checknumber(L, 6);
        sourceHeight = luaL_checknumber(L, 7);
    }
    else
    {
        sourceWidth  = luaL_checknumber(L, 5);
        sourceHeight = luaL_checknumber(L, 6);
    }

    Quad* quad = instance()->newQuad(viewport, sourceWidth, sourceHeight);
    quad->setLayer(layer);

    luax_pushtype(L, quad);
    quad->release();

    return 1;
}

int Wrap_Graphics::newImage(lua_State* L)
{
    return newTexture(L);
}

int Wrap_Graphics::newVideo(lua_State* L)
{
    Video* video = nullptr;

    if (lua_type(L, 1) == LUA_TSTRING)
    {
        const char* filename = luaL_checkstring(L, 1);
        luax_catchexcept(L, [&]() { 
            video = new Video();
            video->setSource(filename);
        });
    }
    else
    {
        luax_catchexcept(L, [&]() { video = new Video(); });
    }

    luax_pushtype(L, video);
    video->release();

    return 1;
}

int Wrap_Graphics::newArrayTexture(lua_State* L)
{
    luax_checkgraphicscreated(L);

    Texture::Slices slices(TEXTURE_2D_ARRAY);
    Texture::Slices* slicesReference = &slices;

    Texture::Settings settings {};
    settings.type = TEXTURE_2D_ARRAY;

    bool dpiScaleSet = false;

    if (lua_type(L, 1) == LUA_TNUMBER)
    {
        slicesReference = nullptr;
        settings.width  = luaL_checkinteger(L, 1);
        settings.height = luaL_checkinteger(L, 2);
        settings.layers = luaL_checkinteger(L, 3);

        luax_checktexturesettings(L, 4, true, false, false, OptionalBool(), settings, dpiScaleSet);
    }
    else
    {
        luax_checktexturesettings(L, 2, true, false, false, OptionalBool(), settings, dpiScaleSet);
        float* autoDpiScale = dpiScaleSet ? nullptr : &settings.dpiScale;

        if (lua_istable(L, 1))
        {
            int length = std::max(1, (int)luax_objlen(L, 1));

            if (luax_isarrayoftables(L, 1))
            {
                for (int slice = 0; slice < length; slice++)
                {
                    lua_rawgeti(L, 1, slice + 1);
                    luaL_checktype(L, -1, LUA_TTABLE);

                    int mipLength = std::max(1, (int)luax_objlen(L, -1));

                    for (int mip = 0; mip < mipLength; mip++)
                    {
                        lua_rawgeti(L, -1, mip + 1);

                        auto* dpiScale = slice == 0 && mip == 0 ? autoDpiScale : nullptr;
                        auto data      = getImageData(L, -1, true, dpiScale);

                        if (data.first.get())
                            slices.set(slice, mip, data.first);
                        else
                            slices.set(slice, mip, data.second->getSlice(0, 0));

                        lua_pop(L, 1);
                    }
                }
            }
            else // no mipmaps!?!?
            {
                for (int slice = 0; slice < length; slice++)
                {
                    lua_rawgeti(L, 1, slice + 1);

                    auto* dpiScale = slice == 0 ? autoDpiScale : nullptr;
                    auto data      = getImageData(L, -1, true, dpiScale);

                    if (data.first.get())
                        slices.set(slice, 0, data.first);
                    else
                    {
                        auto addAllMips = settings.mipmaps != Texture::MIPMAPS_NONE;
                        slices.add(data.second, slice, 0, false, addAllMips);
                    }
                }
            }

            lua_pop(L, length);
        }
        else
        {
            auto data = getImageData(L, 1, true, autoDpiScale);

            if (data.first.get())
                slices.set(0, 0, data.first);
            else
                slices.add(data.second, 0, 0, true, settings.mipmaps != Texture::MIPMAPS_NONE);
        }
    }

    return pushNewTexture(L, slicesReference, settings);
}

int Wrap_Graphics::draw(lua_State* L)
{
#ifdef __WIIU__
    static int drawCount = 0;
    drawCount++;
    if (drawCount <= 10 || drawCount % 120 == 0) {
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "draw() called from Lua (#%d)\n", drawCount);
            fflush(logFile);
            fclose(logFile);
        }
    }
#endif
    Drawable* drawable   = nullptr;
    TextureBase* texture = nullptr;

    Quad* quad = luax_totype<Quad>(L, 2);
    int start  = 2;

    if (quad != nullptr)
    {
        texture = luax_checktexture(L, 1);
        start   = 3;
    }
    else if (lua_isnil(L, 2) && !lua_isnoneornil(L, 3))
        return luax_typeerror(L, 2, "Quad");
    else
    {
        drawable = luax_checktype<Drawable>(L, 1);
        start    = 2;
    }

    luax_checkstandardtransform(L, start, [&](const Matrix4& transform) {
        if (texture && quad)
            instance()->draw(texture, quad, transform);
        else
            instance()->draw(drawable, transform);
    });

    return 0;
}

int Wrap_Graphics::setFont(lua_State* L)
{
    auto* font = luax_checktype<FontBase>(L, 1);
    
#ifdef __WIIU__
    FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile) {
        fprintf(logFile, "setFont() called with font: %p\n", font);
        fflush(logFile);
        fclose(logFile);
    }
#endif
    
    instance()->setFont(font);

    return 0;
}

int Wrap_Graphics::getFont(lua_State* L)
{
    luax_checkgraphicscreated(L);

    FontBase* font = nullptr;
    luax_catchexcept(L, [&]() { font = instance()->getFont(); });

#ifdef __WIIU__
    FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile) {
        fprintf(logFile, "getFont() called: returning font=%p\n", font);
        fflush(logFile);
        fclose(logFile);
    }
#endif

    luax_pushtype(L, font);

    return 1;
}

int Wrap_Graphics::setNewFont(lua_State* L)
{
    // Create a new font and set it as current
    int ret = Wrap_Graphics::newFont(L);  // This pushes the new font onto the stack
    
    // Get the font from the stack
    auto* font = luax_checktype<FontBase>(L, -1);
    instance()->setFont(font);
    
    return ret;  // Return the font
}

static BufferDataUsage luax_optdatausage(lua_State* L, int idx, BufferDataUsage def)
{
    const char* usagestr = lua_isnoneornil(L, idx) ? nullptr : luaL_checkstring(L, idx);

    if (usagestr && !getConstant(usagestr, def))
        luax_enumerror(L, "usage hint", BufferUsages, usagestr);

    return def;
}

static PrimitiveType luax_checkmeshdrawmode(lua_State* L, int idx)
{
    const char* modestr = luaL_checkstring(L, idx);

    PrimitiveType mode = PRIMITIVE_TRIANGLES;
    if (!getConstant(modestr, mode))
        luax_enumerror(L, "mesh draw mode", PrimitiveTypes, modestr);

    return mode;
}

int Wrap_Graphics::newTextBatch(lua_State* L)
{
    luax_checkgraphicscreated(L);

    auto* font       = luax_checkfont(L, 1);
    TextBatch* batch = nullptr;

    // Check if second argument (text) is nil, none, or empty string
    if (lua_isnoneornil(L, 2))
    {
        luax_catchexcept(L, [&]() { batch = instance()->newTextBatch(font); });
    }
    else if (lua_isstring(L, 2))
    {
        const char* str = lua_tostring(L, 2);
        if (str == nullptr || strlen(str) == 0)
        {
            // Handle empty string case - create empty text batch
            luax_catchexcept(L, [&]() { batch = instance()->newTextBatch(font); });
        }
        else
        {
            std::vector<ColoredString> text {};
            luax_checkcoloredstring(L, 2, text);
            luax_catchexcept(L, [&]() { batch = instance()->newTextBatch(font, text); });
        }
    }
    else
    {
        // Handle table case or other types
        std::vector<ColoredString> text {};
        luax_checkcoloredstring(L, 2, text);
        luax_catchexcept(L, [&]() { batch = instance()->newTextBatch(font, text); });
    }

    luax_pushtype(L, batch);
    batch->release();

    return 1;
}

int Wrap_Graphics::newText(lua_State* L)
{
    luax_checkgraphicscreated(L);

    FontBase* font = nullptr;
    
    // If first argument is nil or not provided, use default font
    if (lua_isnoneornil(L, 1))
    {
        font = instance()->getFont();
    }
    else
    {
        font = luax_checkfont(L, 1);
    }

    TextBatch* batch = nullptr;

    // Check if second argument (text) is nil, none, or empty string
    if (lua_isnoneornil(L, 2))
    {
        luax_catchexcept(L, [&]() { batch = instance()->newTextBatch(font); });
    }
    else if (lua_isstring(L, 2))
    {
        const char* str = lua_tostring(L, 2);
        if (str == nullptr || strlen(str) == 0)
        {
            // Handle empty string case - create empty text batch
            luax_catchexcept(L, [&]() { batch = instance()->newTextBatch(font); });
        }
        else
        {
            std::vector<ColoredString> text {};
            luax_checkcoloredstring(L, 2, text);
            luax_catchexcept(L, [&]() { batch = instance()->newTextBatch(font, text); });
        }
    }
    else
    {
        // Handle table case or other types
        std::vector<ColoredString> text {};
        luax_checkcoloredstring(L, 2, text);
        luax_catchexcept(L, [&]() { batch = instance()->newTextBatch(font, text); });
    }

    luax_pushtype(L, batch);
    batch->release();

    return 1;
}

int Wrap_Graphics::newSpriteBatch(lua_State* L)
{
    luax_checkgraphicscreated(L);

    TextureBase* texture  = luax_checktexture(L, 1);
    int size              = luaL_optinteger(L, 2, 1000);
    BufferDataUsage usage = BUFFERDATAUSAGE_DYNAMIC;

    if (lua_gettop(L) > 2)
    {
        const char* usageType = luaL_checkstring(L, 3);
        if (!getConstant(usageType, usage))
            return luax_enumerror(L, "usage hint", BufferUsages, usageType);
    }

    SpriteBatch* batch = nullptr;
    luax_catchexcept(L, [&]() { batch = instance()->newSpriteBatch(texture, size, usage); });

    luax_pushtype(L, batch);
    batch->release();

    return 1;
}

int Wrap_Graphics::newFont(lua_State* L)
{
    luax_checkgraphicscreated(L);

#ifdef __WIIU__
    FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile) {
        fprintf(logFile, "newFont() called from Lua with %d arguments\n", lua_gettop(L));
        for (int i = 1; i <= lua_gettop(L); i++) {
            fprintf(logFile, "  Argument %d type: %d\n", i, lua_type(L, i));
            if (lua_type(L, i) == LUA_TSTRING) {
                fprintf(logFile, "  Argument %d string: %s\n", i, lua_tostring(L, i));
            } else if (lua_type(L, i) == LUA_TNUMBER) {
                fprintf(logFile, "  Argument %d number: %f\n", i, lua_tonumber(L, i));
            }
        }
        fflush(logFile);
        fclose(logFile);
    }
#endif

    FontBase* font = nullptr;

#ifdef __WIIU__
    FILE* logFile_step1 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile_step1) {
        fprintf(logFile_step1, "newFont() - checking if need to create rasterizer\n");
        fflush(logFile_step1);
        fclose(logFile_step1);
    }
#endif

    if (!luax_istype(L, 1, Rasterizer::type))
    {
#ifdef __WIIU__
        FILE* logFile_rast = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile_rast) {
            fprintf(logFile_rast, "newFont() - creating rasterizer from arguments\n");
            fflush(logFile_rast);
            fclose(logFile_rast);
        }
#endif
        std::vector<int> indices {};

        for (int index = 0; index < lua_gettop(L); index++)
            indices.push_back(index + 1);

        luax_convobj(L, indices, "font", "newRasterizer");
#ifdef __WIIU__
        FILE* logFile_rast2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile_rast2) {
            fprintf(logFile_rast2, "newFont() - rasterizer created successfully\n");
            fflush(logFile_rast2);
            fclose(logFile_rast2);
        }
#endif
    }

#ifdef __WIIU__
    FILE* logFile_check = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile_check) {
        fprintf(logFile_check, "newFont() - about to get rasterizer from Lua stack\n");
        fflush(logFile_check);
        fclose(logFile_check);
    }
#endif

    auto* rasterizer = luax_checktype<Rasterizer>(L, 1);

#ifdef __WIIU__
    FILE* logFile_rast_ptr = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile_rast_ptr) {
        fprintf(logFile_rast_ptr, "newFont() - got rasterizer: %p\n", rasterizer);
        fflush(logFile_rast_ptr);
        fclose(logFile_rast_ptr);
    }
#endif

    luax_catchexcept(L, [&]() { font = instance()->newFont(rasterizer); });

#ifdef __WIIU__
    FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile2) {
        fprintf(logFile2, "newFont() created font: %p\n", font);
        fflush(logFile2);
        fclose(logFile2);
    }
#endif

#ifdef __WIIU__
    FILE* logFile_push = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile_push) {
        fprintf(logFile_push, "newFont() - about to push font to Lua stack\n");
        fflush(logFile_push);
        fclose(logFile_push);
    }
#endif

    luax_pushtype(L, font);

#ifdef __WIIU__
    FILE* logFile_release = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile_release) {
        fprintf(logFile_release, "newFont() - about to release font\n");
        fflush(logFile_release);
        fclose(logFile_release);
    }
#endif

    font->release();

#ifdef __WIIU__
    FILE* logFile_end = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile_end) {
        fprintf(logFile_end, "newFont() - completed successfully\n");
        fflush(logFile_end);
        fclose(logFile_end);
    }
#endif

    return 1;
}

int Wrap_Graphics::print(lua_State* L)
{
#ifdef __WIIU__
    static int printCount = 0;
    printCount++;
    if (printCount <= 5 || printCount % 60 == 0) {
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "print() called from Lua (#%d)\n", printCount);
            fflush(logFile);
            fclose(logFile);
        }
    }
#endif
    std::vector<ColoredString> strings {};
    luax_checkcoloredstring(L, 1, strings);

#ifdef __WIIU__
    static int printDetailCount = 0;
    printDetailCount++;
    
    // Enhanced logging for error handler debugging
    FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile) {
        bool shouldLog = false;
        
        // Always log first 15 prints, or prints 10-25, or error messages, or long texts
        if (printDetailCount <= 15 || 
            (printDetailCount > 10 && printDetailCount <= 25) ||
            (!strings.empty() && (strings[0].string.find("Error") != std::string::npos || 
                                 strings[0].string.find("error") != std::string::npos ||
                                 strings[0].string.find("ERROR") != std::string::npos)) ||
            (!strings.empty() && strings[0].string.length() > 100)) {
            shouldLog = true;
        }
        
        if (shouldLog) {
            fprintf(logFile, "print() ENHANCED (#%d): strings.size()=%zu\n", 
                   printDetailCount, strings.size());
            
            if (!strings.empty()) {
                // For very long texts, print more to help debug
                if (strings[0].string.length() > 300) {
                    fprintf(logFile, "  LONG_TEXT_DETECTED: text_length=%zu\n", strings[0].string.length());
                    fprintf(logFile, "  text_start='%.200s'\n", strings[0].string.c_str());
                    if (strings[0].string.length() > 200) {
                        size_t startPos = strings[0].string.length() - 200;
                        fprintf(logFile, "  text_end='%.200s'\n", strings[0].string.substr(startPos).c_str());
                    }
                } else {
                    fprintf(logFile, "  text='%.100s'\n", strings[0].string.c_str());
                    fprintf(logFile, "  text_length=%zu\n", strings[0].string.length());
                }
            } else {
                fprintf(logFile, "  text=EMPTY_STRINGS_VECTOR\n");
            }
            
            // Check font status
            FontBase* currentFont = nullptr;
            try {
                currentFont = instance()->getFont();
            } catch (...) {
                currentFont = nullptr;
            }
            
            if (currentFont) {
                fprintf(logFile, "  font=VALID_FONT_PTR(%p)\n", currentFont);
            } else {
                fprintf(logFile, "  font=NULL_OR_INVALID\n");
            }
            
            // Check graphics state
            bool graphicsCreated = instance()->isCreated();
            bool graphicsActive = instance()->isActive();
            fprintf(logFile, "  graphics_created=%s, graphics_active=%s\n", 
                   graphicsCreated ? "true" : "false",
                   graphicsActive ? "true" : "false");
            
            // Check current color
            Color currentColor = instance()->getColor();
            fprintf(logFile, "  color=R:%.2f G:%.2f B:%.2f A:%.2f\n", 
                   currentColor.r, currentColor.g, currentColor.b, currentColor.a);
        }
        fflush(logFile);
        fclose(logFile);
    }
#endif

    // clang-format off
    if (luax_istype(L, 2, FontBase::type))
    {
        auto* font = luax_checkfont(L, 2);
#ifdef __WIIU__
        FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile2) {
            fprintf(logFile2, "print() using explicit font: %p\n", font);
            fflush(logFile2);
            fclose(logFile2);
        }
#endif
        luax_checkstandardtransform(L, 3, [&](const Matrix4& transform) {
            instance()->print(strings, font, transform);
        });
    }
    else
    {
#ifdef __WIIU__
        FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile2) {
            FontBase* defaultFont = nullptr;
            try {
                defaultFont = instance()->getFont();
            } catch (...) {
                defaultFont = nullptr;
            }
            fprintf(logFile2, "print() using default font: %p\n", defaultFont);
            fflush(logFile2);
            fclose(logFile2);
        }
#endif
        luax_checkstandardtransform(L, 2, [&](const Matrix4& transform) {
            instance()->print(strings, transform);
        });
    }
    // clang-format on

    return 0;
}

int Wrap_Graphics::printf(lua_State* L)
{
    std::vector<ColoredString> strings {};
    luax_checkcoloredstring(L, 1, strings);

    FontBase* font = nullptr;
    int start      = 2;

    if (luax_istype(L, 2, FontBase::type))
    {
        font = luax_checkfont(L, start);
        start++;
    }

    auto align = FontBase::ALIGN_LEFT;
    Matrix4 matrix;

    int formatIndex = start + 2;

    if (luax_istype(L, start, Transform::type))
    {
        auto* transform = luax_totype<Transform>(L, start);
        matrix          = transform->getMatrix();

        formatIndex = start + 1;
    }
    else
    {
        float x = luaL_checknumber(L, start + 0);
        float y = luaL_checknumber(L, start + 1);

        float angle = luaL_optnumber(L, start + 4, 0.0f);
        float sx    = luaL_optnumber(L, start + 5, 1.0f);
        float sy    = luaL_optnumber(L, start + 6, sx);
        float ox    = luaL_optnumber(L, start + 7, 0.0f);
        float oy    = luaL_optnumber(L, start + 8, 0.0f);
        float kx    = luaL_optnumber(L, start + 9, 0.0f);
        float ky    = luaL_optnumber(L, start + 10, 0.0f);

        matrix = Matrix4(x, y, angle, sx, sy, ox, oy, kx, ky);
    }

    float wrap = luaL_checknumber(L, formatIndex);

    auto* alignment = lua_isnoneornil(L, formatIndex + 1) ? nullptr : luaL_checkstring(L, formatIndex + 1);
    if (alignment != nullptr && !FontBase::getConstant(alignment, align))
        return luax_enumerror(L, "alignment", FontBase::AlignModes, alignment);

    if (font != nullptr)
        luax_catchexcept(L, [&]() { instance()->printf(strings, font, wrap, align, matrix); });
    else
        luax_catchexcept(L, [&]() { instance()->printf(strings, wrap, align, matrix); });

    return 0;
}

int Wrap_Graphics::getStats(lua_State* L)
{
    auto stats = instance()->getStats();

    if (lua_istable(L, 1))
        lua_pushvalue(L, 1);
    else
        lua_createtable(L, 0, 7);

    lua_pushinteger(L, stats.drawCalls);
    lua_setfield(L, -2, "drawcalls");

    lua_pushinteger(L, stats.drawCallsBatched);
    lua_setfield(L, -2, "drawcallsbatched");

    lua_pushinteger(L, stats.shaderSwitches);
    lua_setfield(L, -2, "shaderswitches");

    lua_pushinteger(L, stats.textures);
    lua_setfield(L, -2, "textures");

    lua_pushnumber(L, (lua_Number)stats.textureMemory);
    lua_setfield(L, -2, "texturememory");

    lua_pushnumber(L, (lua_Number)stats.cpuProcessingTime);
    lua_setfield(L, -2, "cpuprocessingtime");

    lua_pushnumber(L, (lua_Number)stats.gpuDrawingTime);
    lua_setfield(L, -2, "gpudrawingtime");

    return 1;
}

int Wrap_Graphics::polygon(lua_State* L)
{
    int argc = lua_gettop(L) - 1;

    Graphics::DrawMode mode;
    const char* name = luaL_checkstring(L, 1);

    if (!Graphics::getConstant(name, mode))
        return luax_enumerror(L, "draw mode", Graphics::DrawModes, name);

    bool isTable = false;

    if (argc == 1 && lua_istable(L, 2))
    {
        argc    = (int)luax_objlen(L, 2);
        isTable = true;
    }

    if (argc % 2 != 0)
        return luaL_error(L, "Number of vertex components must be a multiple of two.");
    else if (argc < 6)
        return luaL_error(L, "Need at least three vertices to draw a polygon.");

    int numVertices = argc / 2;
    auto* coords    = instance()->getScratchBuffer<Vector2>(numVertices + 1);

    if (isTable)
    {
        for (int index = 0; index < numVertices; index++)
        {
            lua_rawgeti(L, 2, (index * 2) + 1);
            lua_rawgeti(L, 2, (index * 2) + 2);

            coords[index].x = luax_checkfloat(L, -2);
            coords[index].y = luax_checkfloat(L, -1);

            lua_pop(L, 2);
        }
    }
    else
    {
        for (int index = 0; index < numVertices; ++index)
        {
            coords[index].x = luax_checkfloat(L, (index * 2) + 2);
            coords[index].y = luax_checkfloat(L, (index * 2) + 3);
        }
    }

    coords[numVertices] = coords[0];

    luax_catchexcept(L, [&]() { instance()->polygon(mode, std::span(coords, numVertices + 1)); });

    return 0;
}

int Wrap_Graphics::rectangle(lua_State* L)
{
#ifdef __WIIU__
    static int rectCount = 0;
    rectCount++;
    if (rectCount <= 5 || rectCount % 60 == 0) {
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "rectangle() called from Lua (#%d)\n", rectCount);
            fflush(logFile);
            fclose(logFile);
        }
    }
#endif
    Graphics::DrawMode mode;
    const char* name = luaL_checkstring(L, 1);

    if (!Graphics::getConstant(name, mode))
        return luax_enumerror(L, "draw mode", Graphics::DrawModes, name);

    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    float w = luaL_checknumber(L, 4);
    float h = luaL_checknumber(L, 5);

#ifdef __WIIU__
    static int rectDetailCount = 0;
    rectDetailCount++;
    if (rectDetailCount <= 3) {
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "rectangle() details (#%d): mode='%s', x=%.1f, y=%.1f, w=%.1f, h=%.1f\n", 
                   rectDetailCount, name, x, y, w, h);
            fflush(logFile);
            fclose(logFile);
        }
    }
#endif

    if (lua_isnoneornil(L, 6))
    {
        luax_catchexcept(L, [&]() { instance()->rectangle(mode, x, y, w, h); });
        return 0;
    }

    float rx = luaL_optnumber(L, 6, 0.0f);
    float ry = luaL_optnumber(L, 7, rx);

    if (lua_isnoneornil(L, 8))
        luax_catchexcept(L, [&]() { instance()->rectangle(mode, x, y, w, h, rx, ry); });
    else
    {
        int points = luaL_checkinteger(L, 8);
        luax_catchexcept(L, [&]() { instance()->rectangle(mode, x, y, w, h, rx, ry, points); });
    }

    return 0;
}

int Wrap_Graphics::circle(lua_State* L)
{
    Graphics::DrawMode mode;
    const char* str = luaL_checkstring(L, 1);

    if (!Graphics::getConstant(str, mode))
        return luax_enumerror(L, "draw mode", Graphics::DrawModes, str);

    float x      = luaL_checknumber(L, 2);
    float y      = luaL_checknumber(L, 3);
    float radius = luaL_checknumber(L, 4);

    if (lua_isnoneornil(L, 5))
        luax_catchexcept(L, [&]() { instance()->circle(mode, x, y, radius); });
    else
    {
        int points = luaL_checkinteger(L, 5);
        luax_catchexcept(L, [&]() { instance()->circle(mode, x, y, radius, points); });
    }

    return 0;
}

int Wrap_Graphics::ellipse(lua_State* L)
{
    Graphics::DrawMode mode;
    const char* str = luaL_checkstring(L, 1);

    if (!Graphics::getConstant(str, mode))
        return luax_enumerror(L, "draw mode", Graphics::DrawModes, str);

    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    float a = luaL_checknumber(L, 4);
    float b = luaL_optnumber(L, 5, a);

    if (lua_isnoneornil(L, 6))
        luax_catchexcept(L, [&]() { instance()->ellipse(mode, x, y, a, b); });
    else
    {
        int points = luaL_checkinteger(L, 6);
        luax_catchexcept(L, [&]() { instance()->ellipse(mode, x, y, a, b, points); });
    }

    return 0;
}

int Wrap_Graphics::arc(lua_State* L)
{
    Graphics::DrawMode mode;
    const char* str = luaL_checkstring(L, 1);

    if (!Graphics::getConstant(str, mode))
        return luax_enumerror(L, "draw mode", Graphics::DrawModes, str);

    int start = 2;

    Graphics::ArcMode arcMode = Graphics::ARC_PIE;

    if (lua_type(L, 2) == LUA_TSTRING)
    {
        const char* arcName = luaL_checkstring(L, 2);

        if (!Graphics::getConstant(arcName, arcMode))
            return luax_enumerror(L, "arc mode", Graphics::ArcModes, arcName);

        start = 3;
    }

    float x      = luaL_checknumber(L, start + 0);
    float y      = luaL_checknumber(L, start + 1);
    float radius = luaL_checknumber(L, start + 2);
    float angle1 = luaL_checknumber(L, start + 3);
    float angle2 = luaL_checknumber(L, start + 4);

    if (lua_isnoneornil(L, start + 5))
        luax_catchexcept(L, [&]() { instance()->arc(mode, arcMode, x, y, radius, angle1, angle2); });
    else
    {
        int points = luaL_checkinteger(L, start + 5);
        luax_catchexcept(L, [&]() { instance()->arc(mode, arcMode, x, y, radius, angle1, angle2, points); });
    }

    return 0;
}

int Wrap_Graphics::points(lua_State* L)
{
    int argc = lua_gettop(L) - 1;
    int arg1Type = lua_type(L, 1);
    bool isTable = false;

    if (argc == 1 && arg1Type == LUA_TTABLE)
    {
        argc    = (int)luax_objlen(L, 1);
        isTable = true;
    }

    if (arg1Type != LUA_TTABLE && arg1Type != LUA_TNUMBER)
        return luax_typeerror(L, 1, "table or number");
    else if (argc % 2 != 0 && !isTable)
        return luaL_error(L, "Number of vertex components must be a multiple of two.");
    else if (argc < 4)
        return luaL_error(L, "Need at least two vertices to draw a line.");

    int numVertices = argc / 2;

    auto* coords = instance()->getScratchBuffer<Vector2>(numVertices);

    if (isTable)
    {
        for (int index = 0; index < numVertices; ++index)
        {
            lua_rawgeti(L, 1, (index * 2) + 1);
            lua_rawgeti(L, 1, (index * 2) + 2);

            coords[index].x = luax_checkfloat(L, -2);
            coords[index].y = luax_checkfloat(L, -1);

            lua_pop(L, 2);
        }
    }
    else
    {
        for (int index = 0; index < numVertices; ++index)
        {
            coords[index].x = luax_checkfloat(L, (index * 2) + 1);
            coords[index].y = luax_checkfloat(L, (index * 2) + 2);
        }
    }

    luax_catchexcept(L, [&]() { instance()->points(coords, nullptr, numVertices); });

    return 0;
}

int Wrap_Graphics::line(lua_State* L)
{
    int argc     = lua_gettop(L);
    int arg1Type = lua_type(L, 1);
    bool isTable = false;

    if (argc == 1 && arg1Type == LUA_TTABLE)
    {
        argc    = (int)luax_objlen(L, 1);
        isTable = true;
    }

    if (arg1Type != LUA_TTABLE && arg1Type != LUA_TNUMBER)
        return luax_typeerror(L, 1, "table or number");
    else if (argc % 2 != 0)
        return luaL_error(L, "Number of vertex components must be a multiple of two.");
    else if (argc < 4)
        return luaL_error(L, "Need at least two vertices to draw a line.");

    int numVertices = argc / 2;

    auto* coords = instance()->getScratchBuffer<Vector2>(numVertices);

    if (isTable)
    {
        for (int index = 0; index < numVertices; ++index)
        {
            lua_rawgeti(L, 1, (index * 2) + 1);
            lua_rawgeti(L, 1, (index * 2) + 2);

            coords[index].x = luax_checkfloat(L, -2);
            coords[index].y = luax_checkfloat(L, -1);

            lua_pop(L, 2);
        }
    }
    else
    {
        for (int index = 0; index < numVertices; ++index)
        {
            coords[index].x = luax_checkfloat(L, (index * 2) + 1);
            coords[index].y = luax_checkfloat(L, (index * 2) + 2);
        }
    }

    luax_catchexcept(L, [&]() { instance()->polyline(std::span(coords, numVertices)); });

    return 0;
}

int Wrap_Graphics::getDefaultFilter(lua_State* L)
{
    const auto& state = instance()->getDefaultSamplerState();

    std::string_view minStr {};
    std::string_view magStr {};

    if (!SamplerState::getConstant(state.minFilter, minStr))
        return luaL_error(L, "Unknown minification filter mode.");

    if (!SamplerState::getConstant(state.magFilter, magStr))
        return luaL_error(L, "Unknown magnification filter mode.");

    luax_pushstring(L, minStr);
    luax_pushstring(L, magStr);
    lua_pushnumber(L, state.maxAnisotropy);

    return 3;
}

int Wrap_Graphics::setDefaultFilter(lua_State* L)
{
    auto state = instance()->getDefaultSamplerState();

    const char* minStr = luaL_checkstring(L, 1);
    const char* magStr = luaL_optstring(L, 2, minStr);

    if (!SamplerState::getConstant(minStr, state.minFilter))
        return luax_enumerror(L, "filter mode", SamplerState::FilterModes, minStr);

    if (!SamplerState::getConstant(magStr, state.magFilter))
        return luax_enumerror(L, "filter mode", SamplerState::FilterModes, magStr);

    state.maxAnisotropy = std::clamp((int)luaL_optnumber(L, 3, 1.0), 1, LOVE_UINT8_MAX);
    instance()->setDefaultSamplerState(state);

    return 0;
}

int Wrap_Graphics::getWidth(lua_State* L)
{
    lua_pushinteger(L, instance()->getWidth());

    return 1;
}

int Wrap_Graphics::getHeight(lua_State* L)
{
    lua_pushinteger(L, instance()->getHeight());

    return 1;
}

int Wrap_Graphics::getDimensions(lua_State* L)
{
    lua_pushinteger(L, instance()->getWidth());
    lua_pushinteger(L, instance()->getHeight());

    return 2;
}

int Wrap_Graphics::getPixelWidth(lua_State* L)
{
    lua_pushinteger(L, instance()->getWidth());

    return 1;
}

int Wrap_Graphics::getPixelHeight(lua_State* L)
{
    lua_pushinteger(L, instance()->getHeight());

    return 1;
}

int Wrap_Graphics::getPixelDimensions(lua_State* L)
{
    lua_pushinteger(L, instance()->getWidth());
    lua_pushinteger(L, instance()->getHeight());

    return 2;
}

// Homebrew Stuff™

int Wrap_Graphics::getScreens(lua_State* L)
{
    auto screens = love::getScreenInfo();
    lua_createtable(L, screens.size(), 0);

    for (size_t i = 0; i < screens.size(); i++)
    {
        luax_pushstring(L, screens[i].name);
        lua_rawseti(L, -2, i + 1);
    }

    return 1;
}

int Wrap_Graphics::getActiveScreen(lua_State* L)
{
#ifdef __WIIU__
    FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile) {
        fprintf(logFile, "getActiveScreen() called from Lua\n");
        fflush(logFile);
        fclose(logFile);
    }
#endif
    
    auto& info = love::getScreenInfo(love::currentScreen);
    
#ifdef __WIIU__
    FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile2) {
        fprintf(logFile2, "getActiveScreen() - current screen: %d ('%.*s')\n", 
               love::currentScreen, (int)info.name.size(), info.name.data());
        fflush(logFile2);
        fclose(logFile2);
    }
#endif
    
    luax_pushstring(L, info.name);

    return 1;
}

int Wrap_Graphics::setActiveScreen(lua_State* L)
{
#ifdef __WIIU__
    FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile) {
        fprintf(logFile, "setActiveScreen() called from Lua\n");
        fflush(logFile);
        fclose(logFile);
    }
#endif
    
    std::string name = luax_checkstring(L, 1);
    auto value       = love::getScreenId(name);

    if (value == INVALID_SCREEN)
        return luaL_error(L, "Invalid screen '%s'", name.c_str());

#ifdef __WIIU__
    FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile2) {
        fprintf(logFile2, "setActiveScreen() - setting screen '%s' to: %d\n", name.c_str(), value);
        fflush(logFile2);
        fclose(logFile2);
    }
#endif

    love::currentScreen = value;
    instance()->setActiveScreen();

#ifdef __WIIU__
    FILE* logFile3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile3) {
        fprintf(logFile3, "setActiveScreen() completed\n");
        fflush(logFile3);
        fclose(logFile3);
    }
#endif

    return 0;
}

#if !defined(__WIIU__)
int Wrap_Graphics::copyCurrentScanBuffer(lua_State* L)
{
    return 0;
}
#endif

// clang-format off
#if defined(__3DS__)
int Wrap_Graphics::is3D(lua_State* L)
{
    lua_pushboolean(L, instance()->is3D());

    return 1;
}

int Wrap_Graphics::set3D(lua_State* L)
{
    bool enable = luax_toboolean(L, 1);
    instance()->set3D(enable);

    return 0;
}

int Wrap_Graphics::isWide(lua_State* L)
{
    lua_pushboolean(L, instance()->isWide());

    return 1;
}

int Wrap_Graphics::setWide(lua_State* L)
{
    bool enable = luax_toboolean(L, 1);
    instance()->setWide(enable);

    return 0;
}

int Wrap_Graphics::getDepth(lua_State* L)
{
    lua_pushnumber(L, instance()->getDepth());

    return 1;
}

static constexpr std::array<luaL_Reg, 5> platformFunctions =
{{
    { "is3D",     Wrap_Graphics::is3D     },
    { "set3D",    Wrap_Graphics::set3D    },
    { "isWide",   Wrap_Graphics::isWide   },
    { "setWide",  Wrap_Graphics::setWide  },
    { "getDepth", Wrap_Graphics::getDepth }
}};
#elif defined(__WIIU__)
int Wrap_Graphics::copyCurrentScanBuffer(lua_State* L)
{
    instance()->copyCurrentScanBuffer();

    return 0;
}
#endif

#if !defined(__3DS__)
static constexpr std::span<const luaL_Reg> platformFunctions = {};
#endif

static constexpr luaL_Reg functions[] =
{
    { "reset",                  Wrap_Graphics::reset                 },
    { "clear",                  Wrap_Graphics::clear                 },
    { "present",                Wrap_Graphics::present               },
    { "setColor",               Wrap_Graphics::setColor              },
    { "getColor",               Wrap_Graphics::getColor              },
    { "setBackgroundColor",     Wrap_Graphics::setBackgroundColor    },
    { "getBackgroundColor",     Wrap_Graphics::getBackgroundColor    },
    { "setColorMask",           Wrap_Graphics::setColorMask          },
    { "getColorMask",           Wrap_Graphics::getColorMask          },
    { "setBlendMode",           Wrap_Graphics::setBlendMode          },
    { "getBlendMode",           Wrap_Graphics::getBlendMode          },
    { "setBlendState",          Wrap_Graphics::setBlendState         },
    { "getBlendState",          Wrap_Graphics::getBlendState         },
    { "setLineWidth",           Wrap_Graphics::setLineWidth          },
    { "getLineWidth",           Wrap_Graphics::getLineWidth          },
    { "setLineJoin",            Wrap_Graphics::setLineJoin           },
    { "getLineJoin",            Wrap_Graphics::getLineJoin           },
    { "setLineStyle",           Wrap_Graphics::setLineStyle          },
    { "getLineStyle",           Wrap_Graphics::getLineStyle          },
    { "setPointSize",           Wrap_Graphics::setPointSize          },
    { "getPointSize",           Wrap_Graphics::getPointSize          },
    { "setMeshCullMode",        Wrap_Graphics::setMeshCullMode       },
    { "getMeshCullMode",        Wrap_Graphics::getMeshCullMode       },
    { "setFrontFaceWinding",    Wrap_Graphics::setFrontFaceWinding   },
    { "getFrontFaceWinding",    Wrap_Graphics::getFrontFaceWinding   },
    { "isCreated",              Wrap_Graphics::isCreated             },
    { "isActive",               Wrap_Graphics::isActive              },
    { "isGammaCorrect",         Wrap_Graphics::isGammaCorrect        },
    { "getRendererInfo",        Wrap_Graphics::getRendererInfo       },
    { "getTextureFormats",      Wrap_Graphics::getTextureFormats     },
    { "getSystemLimits",        Wrap_Graphics::getSystemLimits       },
    { "getTextureTypes",        Wrap_Graphics::getTextureTypes       },
    { "getDPIScale",            Wrap_Graphics::getDPIScale           },
    { "setScissor",             Wrap_Graphics::setScissor            },
    { "intersectScissor",       Wrap_Graphics::intersectScissor      },
    { "getScissor",             Wrap_Graphics::getScissor            },
    { "getStackDepth",          Wrap_Graphics::getStackDepth         },
    { "push",                   Wrap_Graphics::push                  },
    { "pop",                    Wrap_Graphics::pop                   },
    { "rotate",                 Wrap_Graphics::rotate                },
    { "scale",                  Wrap_Graphics::scale                 },
    { "translate",              Wrap_Graphics::translate             },
    { "shear",                  Wrap_Graphics::shear                 },
    { "origin",                 Wrap_Graphics::origin                },
    { "applyTransform",         Wrap_Graphics::applyTransform        },
    { "replaceTransform",       Wrap_Graphics::replaceTransform      },
    { "transformPoint",         Wrap_Graphics::transformPoint        },
    { "inverseTransformPoint",  Wrap_Graphics::inverseTransformPoint },
    { "getStats",               Wrap_Graphics::getStats              },

    { "getWidth",               Wrap_Graphics::getWidth              },
    { "getHeight",              Wrap_Graphics::getHeight             },
    { "getDimensions",          Wrap_Graphics::getDimensions         },
    { "getPixelWidth",          Wrap_Graphics::getPixelWidth         },
    { "getPixelHeight",         Wrap_Graphics::getPixelHeight        },
    { "getPixelDimensions",     Wrap_Graphics::getPixelDimensions    },

    { "setDefaultFilter",       Wrap_Graphics::setDefaultFilter      },
    { "getDefaultFilter",       Wrap_Graphics::getDefaultFilter      },

    { "setShader",              Wrap_Graphics::setShader             },
    { "getShader",              Wrap_Graphics::getShader             },
    { "newShader",              Wrap_Graphics::newShader             },

    { "draw",                   Wrap_Graphics::draw                  },

    { "polygon",                Wrap_Graphics::polygon               },
    { "rectangle",              Wrap_Graphics::rectangle             },
    { "circle",                 Wrap_Graphics::circle                },
    { "ellipse",                Wrap_Graphics::ellipse               },
    { "arc",                    Wrap_Graphics::arc                   },
    { "points",                 Wrap_Graphics::points                },
    { "line",                   Wrap_Graphics::line                  },

    { "newTexture",                         Wrap_Graphics::newTexture            },
    { "newQuad",                Wrap_Graphics::newQuad               },
    { "newImage",               Wrap_Graphics::newImage              },
    { "newVideo",               Wrap_Graphics::newVideo              },
    { "newCanvas",             Wrap_Graphics::newCanvas             },

    // Canvas functions
    { "setCanvas",              Wrap_Graphics::setCanvas             },
    { "getCanvas",              Wrap_Graphics::getCanvas             },

    { "newTextBatch",           Wrap_Graphics::newTextBatch          },
    { "newText",                Wrap_Graphics::newText               },
    { "newSpriteBatch",         Wrap_Graphics::newSpriteBatch        },

    { "newFont",                Wrap_Graphics::newFont               },
    { "setFont",                Wrap_Graphics::setFont               },
    { "getFont",                Wrap_Graphics::getFont               },
    { "setNewFont",             Wrap_Graphics::setNewFont            },
    { "print",                  Wrap_Graphics::print                 },
    { "printf",                 Wrap_Graphics::printf                },

    { "getScreens",            Wrap_Graphics::getScreens             },
    { "getActiveScreen",       Wrap_Graphics::getActiveScreen        },
    { "setActiveScreen",       Wrap_Graphics::setActiveScreen        },
    { "copyCurrentScanBuffer", Wrap_Graphics::copyCurrentScanBuffer  }
};

static int open_drawable(lua_State* L)
{
    return luax_register_type(L, &Drawable::type);
}

static constexpr lua_CFunction types[] =
{
    open_drawable,
    love::open_texture,
    love::open_quad,
    love::open_font,
    love::open_textbatch,
    love::open_spritebatch,
    love::open_video,
    love::open_shader
};
// clang-format on

int Wrap_Graphics::open(lua_State* L)
{
    auto* gfx = instance();
#ifdef __WIIU__
    uint64_t startTicks = OSGetSystemTime();
    {
        FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f) { fprintf(f, "[WRAP_GFX] open ENTER (existing=%s)\n", gfx?"yes":"no"); fflush(f); fclose(f);}    
    }
#endif
    if (gfx == nullptr)
    {
#ifdef __WIIU__
        FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f) { fprintf(f, "[WRAP_GFX] constructing Graphics...\n"); fflush(f); fclose(f);}    
#endif
        luax_catchexcept(L, [&]() { gfx = new Graphics(); });
#ifdef __WIIU__
        {
            FILE* f2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
            if (f2) { fprintf(f2, "[WRAP_GFX] Graphics constructed (%p)\n", (void*)gfx); fflush(f2); fclose(f2);}    
        }
#endif
    }
    else
        gfx->retain();

    WrappedModule module {};
    module.instance          = gfx;
    module.name              = "graphics";
    module.type              = &Module::type;
    module.functions         = functions;
    module.platformFunctions = platformFunctions;
    module.types             = types;

#ifdef __WIIU__
    {
        FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
        if (f) { fprintf(f, "[WRAP_GFX] registering module...\n"); fflush(f); fclose(f);}    
    }
#endif
    int r = luax_register_module(L, module);
#ifdef __WIIU__
    uint64_t endTicks = OSGetSystemTime();
    double ms = (double)OSTicksToMilliseconds(endTicks - startTicks);
    FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
    if (f) { fprintf(f, "[WRAP_GFX] open EXIT ret=%d (%.2f ms)\n", r, ms); fflush(f); fclose(f);}    
#endif
    return r;
}

int Wrap_Graphics::newCanvas(lua_State* L)
{
    int argCount = lua_gettop(L);
    
    // Get default window dimensions if not specified
    int width = 0, height = 0;
    auto* graphics = instance();
    if (graphics)
    {
        width = graphics->getPixelWidth();
        height = graphics->getPixelHeight();
    }
    
    // If no args, use window size
    if (argCount == 0)
    {
        // canvas = love.graphics.newCanvas()
        // Use default window size
    }
    else if (argCount >= 2)
    {
        // canvas = love.graphics.newCanvas(width, height [, settings])
        width = luaL_checkint(L, 1);
        height = luaL_checkint(L, 2);
    }
    
    if (width <= 0 || height <= 0)
    {
        return luaL_error(L, "Canvas dimensions must be positive (got %d x %d)", width, height);
    }
    
    // Create texture settings for render target
    TextureBase::Settings settings;
    settings.width = width;
    settings.height = height;
    settings.type = TEXTURE_2D;
    settings.format = PIXELFORMAT_NORMAL; // Default format
    settings.renderTarget = true; // This makes it a Canvas
    settings.readable = true;
    settings.mipmaps = TextureBase::MIPMAPS_NONE;
    settings.msaa = 0;
    settings.layers = 1;
    
    // TODO: Parse settings table if provided as 3rd argument
    // For now just use defaults for Wii U compatibility
    
    TextureBase* texture = nullptr;
    luax_catchexcept(L, [&]() {
        texture = graphics->newTexture(settings, nullptr);
    });
    
    luax_pushtype(L, texture);
    return 1;
}

int Wrap_Graphics::setCanvas(lua_State* L)
{
    int argCount = lua_gettop(L);
    auto* graphics = instance();
    
    luax_checkgraphicscreated(L);
    
    if (argCount == 0)
    {
        // love.graphics.setCanvas() - reset to screen
        luax_catchexcept(L, [&]() {
            graphics->setRenderTargets();
        });
    }
    else if (argCount == 1)
    {
        if (lua_isnil(L, 1))
        {
            // love.graphics.setCanvas(nil) - reset to screen
            luax_catchexcept(L, [&]() {
                graphics->setRenderTargets();
            });
        }
        else
        {
            // love.graphics.setCanvas(canvas)
            TextureBase* canvas = luax_checktype<TextureBase>(L, 1);
            
            if (!canvas->isRenderTarget())
            {
                return luaL_error(L, "Texture is not a render target (Canvas)");
            }
            
            luax_catchexcept(L, [&]() {
                graphics->setRenderTargets(canvas);
            });
        }
    }
    else if (argCount == 2)
    {
        // love.graphics.setCanvas(canvas, mipmap) or love.graphics.setCanvas(canvas, slice)
        TextureBase* canvas = luax_checktype<TextureBase>(L, 1);
        int level = luaL_checkint(L, 2);
        
        if (!canvas->isRenderTarget())
        {
            return luaL_error(L, "Texture is not a render target (Canvas)");
        }
        
        luax_catchexcept(L, [&]() {
            graphics->setRenderTargets(canvas, 0, level); // slice=0, mipmap=level
        });
    }
    else if (argCount == 3)
    {
        // love.graphics.setCanvas(canvas, slice, mipmap)
        TextureBase* canvas = luax_checktype<TextureBase>(L, 1);
        int slice = luaL_checkint(L, 2);
        int mipmap = luaL_checkint(L, 3);
        
        if (!canvas->isRenderTarget())
        {
            return luaL_error(L, "Texture is not a render target (Canvas)");
        }
        
        luax_catchexcept(L, [&]() {
            graphics->setRenderTargets(canvas, slice, mipmap);
        });
    }
    else
    {
        return luaL_error(L, "Too many arguments to setCanvas");
    }
    
    return 0;
}

int Wrap_Graphics::getCanvas(lua_State* L)
{
    auto* graphics = instance();
    luax_checkgraphicscreated(L);
    
    const auto& targets = graphics->getActiveRenderTargets();
    
    if (targets.colors.empty() || targets.colors[0].texture == nullptr)
    {
        lua_pushnil(L);
    }
    else
    {
        luax_pushtype(L, targets.colors[0].texture);
    }
    
    return 1;
}

int Wrap_Graphics::getTextureFormats(lua_State* L)
{
    // Stub implementation - return empty table for now
    lua_newtable(L);
    return 1;
}

int Wrap_Graphics::getSystemLimits(lua_State* L)
{
    // Stub implementation - return empty table for now
    lua_newtable(L);
    return 1;
}

int Wrap_Graphics::getTextureTypes(lua_State* L)
{
    // Stub implementation - return empty table for now
    lua_newtable(L);
    return 1;
}

int Wrap_Graphics::setShader(lua_State* L)
{
    auto* graphics = instance();
    luax_checkgraphicscreated(L);

    if (lua_gettop(L) == 0 || lua_isnil(L, 1))
    {
        // love.graphics.setShader() or love.graphics.setShader(nil) - reset to default
        graphics->setShader();
    }
    else
    {
        // love.graphics.setShader(shader)
        ShaderBase* shader = luax_checktype<ShaderBase>(L, 1);
        graphics->setShader(shader);
    }

    return 0;
}

int Wrap_Graphics::getShader(lua_State* L)
{
    auto* graphics = instance();
    luax_checkgraphicscreated(L);
    
    ShaderBase* shader = graphics->getShader();
    
    if (shader == nullptr)
    {
        lua_pushnil(L);
    }
    else
    {
        luax_pushtype(L, shader);
    }
    
    return 1;
}

int Wrap_Graphics::newShader(lua_State* L)
{
    int argc = lua_gettop(L); // Přidání deklarace argc
    auto* graphics = instance();
    luax_checkgraphicscreated(L);
    
    std::vector<std::string> filepaths;
    ShaderBase::CompileOptions options;

    const std::string logPath = "/vol/external01/wiiu/apps/balatro/simple_debug.log";

#if defined(__WIIU__) && defined(USE_CAFEGLSL)
    auto logMessage = [&](const std::string& msg) {
        std::ofstream log(logPath, std::ios::app);
        if (log.is_open())
        {
            log << msg << std::endl;
            log.close();
        }
    };

    auto tryCompileLoveShader = [&](const std::string& pixelSource,
                                    const std::string* vertexSource,
                                    const std::string& label) -> ShaderBase* {
        if (!love::CafeGLSLCompiler::IsAvailable())
        {
            logMessage("[DEBUG] CafeGLSL unavailable; cannot runtime compile " + label);
            return nullptr;
        }

        std::string vsSource = vertexSource && !vertexSource->empty() ? *vertexSource : love::CafeGLSLCompiler::GetDefaultVertexShaderSource();
        std::string psSource = love::CafeGLSLCompiler::ConvertLoveShaderToGLSL(pixelSource);

        logMessage("[DEBUG] Converted Love shader to GLSL for " + label);

        GX2VertexShader* vs = love::CafeGLSLCompiler::CompileVertexShader(vsSource);
        if (!vs)
        {
            logMessage("[DEBUG] Vertex shader compilation failed for " + label);
            return nullptr;
        }

        GX2PixelShader* ps = love::CafeGLSLCompiler::CompilePixelShader(psSource);
        if (!ps)
        {
            logMessage("[DEBUG] Pixel shader compilation failed for " + label);
            love::CafeGLSLCompiler::FreeVertexShader(vs);
            return nullptr;
        }

        WHBGfxShaderGroup group{};
        group.vertexShader = vs;
        group.pixelShader = ps;

        try
        {
            love::Shader* sh = new love::Shader(group, options);
            logMessage("[DEBUG] Runtime shader compilation succeeded for " + label);
            return sh;
        }
        catch (const std::exception& e)
        {
            logMessage(std::string("[DEBUG] Failed to create Shader from runtime group for ") + label + ": " + e.what());
            love::CafeGLSLCompiler::FreeVertexShader(vs);
            love::CafeGLSLCompiler::FreePixelShader(ps);
            return nullptr;
        }
    };
#endif

    // Debug: Log shader creation at Lua wrapper level
    std::ofstream debugFile(logPath, std::ios::app);
    if (debugFile.is_open())
    {
        debugFile << "[DEBUG] Wrap_Graphics::newShader() - Lua wrapper called with " << lua_gettop(L) << " arguments" << std::endl;
        debugFile.close();
    }
    
    // Handle different argument patterns:
    // newShader(vertexCode, fragmentCode) - string codes
    // newShader(vertexFile, fragmentFile) - file paths
    // newShader(shaderCode) - single code for both vertex and fragment
    
    if (lua_type(L, 1) == LUA_TSTRING)
    {
        const char* code1 = luaL_checkstring(L, 1);
        
        // Debug: Log shader paths/code
        std::ofstream debugFile2(logPath, std::ios::app);
        if (debugFile2.is_open())
        {
            debugFile2 << "[DEBUG] Wrap_Graphics::newShader() - First argument: " << std::string(code1) << std::endl;
            debugFile2.close();
        }
        
    if (lua_gettop(L) >= 2 && lua_type(L, 2) == LUA_TSTRING)
        {
            // Two strings: vertex and fragment code/files
            const char* code2 = luaL_checkstring(L, 2);
            
            std::ofstream debugFile3(logPath, std::ios::app);
            if (debugFile3.is_open())
            {
                debugFile3 << "[DEBUG] Wrap_Graphics::newShader() - Second argument: " << std::string(code2) << std::endl;
                debugFile3.close();
            }
            
            filepaths.push_back(std::string(code1));
            filepaths.push_back(std::string(code2));
        }
        else
        {
            // Single string: combined shader code/file
            filepaths.push_back(std::string(code1));
        }

        // Check if the provided string looks like GLSL source (not a path)
        // On Wii U without CafeGLSL, we'll return a dummy shader instead of crashing
        auto looksLikeGLSL = [](const std::string& s) {
            if (s.find("\n") != std::string::npos) return true;
            if (s.find("void main") != std::string::npos) return true;
            if (s.find("vec4 effect") != std::string::npos) return true; // LÖVE2D shaders
            if (s.find("extern") != std::string::npos) return true; // LÖVE2D uniforms
            if (s.find("uniform") != std::string::npos) return true;
            if (s.find("sampler") != std::string::npos) return true;
            // crude: paths usually have a dot and a slash
            bool hasSlash = (s.find('/') != std::string::npos) || (s.find('\\') != std::string::npos);
            bool hasDot = (s.find('.') != std::string::npos);
            return !hasSlash && !hasDot; // bare code string
        };
        if (looksLikeGLSL(filepaths[0]))
        {
#if defined(__WIIU__) && defined(USE_CAFEGLSL)
            logMessage("[DEBUG] Detected inline shader source; attempting CafeGLSL runtime compile");

            std::string pixelSource = std::string(lua_tostring(L, 1));
            std::string vertexSource;
            bool hasVertexSource = false;
            if (lua_gettop(L) >= 2 && lua_type(L, 2) == LUA_TSTRING)
            {
                vertexSource = std::string(lua_tostring(L, 2));
                hasVertexSource = true;
            }

            if (ShaderBase* runtimeShader = tryCompileLoveShader(pixelSource, hasVertexSource ? &vertexSource : nullptr, "inline source"))
            {
                luax_pushtype(L, runtimeShader);
                runtimeShader->release();
                return 1;
            }
#endif
        }
    }
    else
    {
        return luaL_error(L, "newShader expects string arguments");
    }
    
#if defined(__WIIU__) && defined(USE_CAFEGLSL)
    auto endsWithIgnoreCase = [](const std::string& str, const std::string& suffix) {
        if (suffix.size() > str.size())
            return false;
        for (size_t i = 0; i < suffix.size(); ++i)
        {
            char a = (char)tolower((unsigned char)str[str.size() - suffix.size() + i]);
            char b = (char)tolower((unsigned char)suffix[i]);
            if (a != b)
                return false;
        }
        return true;
    };

    // Attempt runtime compilation when a Love2D-style shader file is provided.
    if (!filepaths.empty())
    {
        const std::string& primary = filepaths[0];
        if (endsWithIgnoreCase(primary, ".fs") || endsWithIgnoreCase(primary, ".frag"))
        {
            love::Filesystem* fs = Module::getInstance<love::Filesystem>(Module::M_FILESYSTEM);
            if (fs && fs->exists(primary.c_str()))
            {
                StrongRef<love::FileData> file(fs->read(primary));
                if (file.get() != nullptr)
                {
                    std::string pixelSource(static_cast<const char*>(file->getData()), file->getSize());
                    std::string vertexSource;
                    bool hasVertexSource = false;
                    if (filepaths.size() >= 2 && endsWithIgnoreCase(filepaths[1], ".vs"))
                    {
                        StrongRef<love::FileData> vsFile(fs->read(filepaths[1]));
                        if (vsFile.get() != nullptr)
                        {
                            vertexSource = std::string(static_cast<const char*>(vsFile->getData()), vsFile->getSize());
                            hasVertexSource = true;
                        }
                    }

                    if (ShaderBase* runtimeShader = tryCompileLoveShader(pixelSource, hasVertexSource ? &vertexSource : nullptr, primary))
                    {
                        luax_pushtype(L, runtimeShader);
                        runtimeShader->release();
                        return 1;
                    }
                }
                else
                {
                    logMessage("[DEBUG] Failed to read shader file for runtime compilation: " + primary);
                }
            }
            else
            {
                logMessage("[DEBUG] Shader file not found for runtime compilation: " + primary);
            }
        }
    }
#endif

    // Wii U: map GLSL sources (*.fs, *.frag) to precompiled GSH location
    auto mapToGsh = [](const std::string& in) {
        // If already a .gsh or absolute shaders path, keep as-is
        auto lower = in;
        for (auto& c : lower) c = (char)tolower((unsigned char)c);
        if (lower.rfind(".gsh") != std::string::npos)
            return in;
        // Extract base filename without extension
        size_t slash = in.find_last_of("/\\");
        std::string name = (slash == std::string::npos) ? in : in.substr(slash + 1);
        size_t dot = name.find_last_of('.');
        if (dot != std::string::npos)
            name = name.substr(0, dot);
        return std::string("/vol/content/shaders/") + name + ".gsh";
    };

    std::vector<std::string> mappedPaths;
    mappedPaths.reserve(filepaths.size());
    if (!filepaths.empty())
    {
        if (filepaths.size() == 1)
        {
            std::string g = mapToGsh(filepaths[0]);
            mappedPaths.push_back(g); // Wii U GSH holds both stages
        }
        else
        {
            // Map both to the same .gsh (vertex+pixel extracted by loader)
            std::string g = mapToGsh(filepaths[0]);
            mappedPaths.push_back(g);
            mappedPaths.push_back(g);
        }
    }

    ShaderBase* shader = nullptr;

    // Early existence check for mapped .gsh to avoid throwing repeatedly on Wii U
    auto allPathsExist = [](const std::vector<std::string>& paths) -> bool {
        if (paths.empty()) return false;
        for (const auto& p : paths)
        {
            FILE* f = fopen(p.c_str(), "rb");
            if (!f) return false;
            fclose(f);
        }
        return true;
    };

    bool triedPrimary = false;
    try
    {
        // Try custom/effect shader only if files exist, else skip to fallback
        {
            std::ofstream debugFile4(logPath, std::ios::app);
            if (debugFile4.is_open())
            {
                debugFile4 << "[DEBUG] Wrap_Graphics::newShader() - Attempting graphics->newShader() with " << mappedPaths.size() << " mapped paths" << std::endl;
                for (size_t i = 0; i < mappedPaths.size(); ++i)
                    debugFile4 << "  mapped[" << i << "]: " << mappedPaths[i] << std::endl;
                debugFile4.close();
            }
        }
        const auto& primary = mappedPaths.empty() ? filepaths : mappedPaths;
        if (allPathsExist(primary))
        {
            triedPrimary = true;
            shader = graphics->newShader(primary, options);
        }
    }
    catch (const std::exception& e)
    {
        // Try Love2D shader conversion instead of basic texture fallback
        {
            std::ofstream debugFileF(logPath, std::ios::app);
            if (debugFileF.is_open())
            {
                debugFileF << "[DEBUG] Wrap_Graphics::newShader() - Primary shader " << (triedPrimary ? "failed" : "skipped (missing files)") << ": " << e.what() << std::endl;
                debugFileF << "[DEBUG] Wrap_Graphics::newShader() - Attempting Love2D shader conversion instead of fallback" << std::endl;
                debugFileF.close();
            }
        }

        // Try to create a compatible Love2D shader using string source conversion
        if (argc >= 1 && lua_isstring(L, 1))
        {
            const char* sourceCode = lua_tostring(L, 1);
            
            // Convert Love2D shader source to GLSL
#ifdef USE_CAFEGLSL
            // Use CafeGLSL for Love2D → GLSL conversion and GX2 compilation
            std::string convertedGLSL = CafeGLSLCompiler::ConvertLoveShaderToGLSL(std::string(sourceCode));
#else
            // Use our fallback Love2D → GLSL converter
            std::string convertedGLSL = ProcessLove2DShader(std::string(sourceCode));
#endif
            if (!convertedGLSL.empty()) {
                {
                    std::ofstream debugFileC(logPath, std::ios::app);
                    if (debugFileC.is_open())
                    {
                        debugFileC << "[DEBUG] Wrap_Graphics::newShader() - Love2D conversion produced " << convertedGLSL.size() << " bytes of GLSL" << std::endl;
                        debugFileC.close();
                    }
                }
                
                // Try creating shader with converted GLSL - this would need implementation
                // For now, fall back to basic texture shader
            }
            
            // If conversion failed or not available, use basic fallback
            const char* fallback = "/vol/content/shaders/texture.gsh";
            std::vector<std::string> fb = { fallback, fallback };
            
            try
            {
                shader = graphics->newShader(fb, options);
            }
            catch (const std::exception& e2)
            {
                return luaL_error(L, "Failed to create shader: %s (Love2D conversion and fallback both failed: %s)", e.what(), e2.what());
            }
        }
        else
        {
            // No string source to convert, use basic fallback
            const char* fallback = "/vol/content/shaders/texture.gsh";
            std::vector<std::string> fb = { fallback, fallback };
            
            try
            {
                shader = graphics->newShader(fb, options);
            }
            catch (const std::exception& e2)
            {
                return luaL_error(L, "Failed to create shader: %s (fallback failed: %s)", e.what(), e2.what());
            }
        }
    }

    // If primary path was missing (no exception thrown) try Love2D conversion
    if (shader == nullptr)
    {
        {
            std::ofstream debugFileF2(logPath, std::ios::app);
            if (debugFileF2.is_open())
            {
                debugFileF2 << "[DEBUG] Wrap_Graphics::newShader() - Primary shader not created (likely missing files). Trying Love2D conversion instead of basic fallback" << std::endl;
                debugFileF2.close();
            }
        }

        // Try Love2D shader conversion if we have string source
        bool conversionAttempted = false;
        if (argc >= 1 && lua_isstring(L, 1))
        {
            const char* sourceCode = lua_tostring(L, 1);
            
            // Convert Love2D shader source to GLSL
#ifdef USE_CAFEGLSL
            // Use CafeGLSL for Love2D → GLSL conversion and GX2 compilation
            std::string convertedGLSL = CafeGLSLCompiler::ConvertLoveShaderToGLSL(std::string(sourceCode));
#else
            // Use our fallback Love2D → GLSL converter
            std::string convertedGLSL = ProcessLove2DShader(std::string(sourceCode));
#endif
            if (!convertedGLSL.empty()) {
                conversionAttempted = true;
                {
                    std::ofstream debugFileC2(logPath, std::ios::app);
                    if (debugFileC2.is_open())
                    {
                        debugFileC2 << "[DEBUG] Wrap_Graphics::newShader() - Love2D conversion #2 produced " << convertedGLSL.size() << " bytes of GLSL" << std::endl;
                        debugFileC2.close();
                    }
                }
                // Conversion successful but we still need implementation for GLSL compilation
            }
        }
        
        // If conversion not possible or failed, use basic texture fallback as last resort
        if (!conversionAttempted)
        {
            const char* fallback = "/vol/content/shaders/texture.gsh";
            std::vector<std::string> fb = { fallback, fallback };
            
            try
            {
                shader = graphics->newShader(fb, options);
            }
            catch (const std::exception& e3)
            {
                return luaL_error(L, "Failed to create shader: primary missing; Love2D conversion unavailable; basic fallback failed: %s", e3.what());
            }
        }
        else
        {
            // For now, still use fallback even after conversion until we implement GLSL compilation
            const char* fallback = "/vol/content/shaders/texture.gsh";
            std::vector<std::string> fb = { fallback, fallback };
            
            try
            {
                shader = graphics->newShader(fb, options);
            }
            catch (const std::exception& e3)
            {
                return luaL_error(L, "Failed to create shader: Love2D conversion completed but GLSL compilation not implemented; fallback failed: %s", e3.what());
            }
        }
    }

    // Push shader userdata back to Lua
    luax_pushtype(L, shader);
    {
        std::ofstream debugFileEnd(logPath, std::ios::app);
        if (debugFileEnd.is_open())
        {
            debugFileEnd << "[DEBUG] Wrap_Graphics::newShader() - returning shader userdata for ptr=" << shader << std::endl;
            debugFileEnd.close();
        }
    }
    shader->release();
    
    return 1;
}

#include "common/CafeGLSL.hpp"

// CafeGLSL is always enabled - using DYNAMIC RPL LOADING

#ifdef __WIIU__
// Use dynamic RPL loading instead of static linking
#include <gx2/shaders.h>
#include <coreinit/debug.h>
#include <whb/log.h>
#include <coreinit/dynload.h>

// Dynamic function pointers for CafeGLSL RPL functions (renamed to avoid conflicts)
static void (*rpl_InitGLSLCompiler)() = nullptr;
static void (*rpl_DestroyGLSLCompiler)() = nullptr;
static GX2VertexShader* (*rpl_CompileVertexShader)(const char* shaderSource, char* infoLogOut, int infoLogMaxLength, int flags) = nullptr;
static GX2PixelShader* (*rpl_CompilePixelShader)(const char* shaderSource, char* infoLogOut, int infoLogMaxLength, int flags) = nullptr;
static void (*rpl_FreeVertexShader)(GX2VertexShader* shader) = nullptr;
static void (*rpl_FreePixelShader)(GX2PixelShader* shader) = nullptr;

// RPL handle
static OSDynLoad_Module s_rplHandle = 0;
#else
// Dummy implementations for non-Wii U builds
struct GX2VertexShader {};
struct GX2PixelShader {};
#endif

namespace love
{
    // Static member definitions - for static linking
    bool CafeGLSLCompiler::s_initialized = false;
    bool CafeGLSLCompiler::s_available = false;

    bool CafeGLSLCompiler::Initialize()
    {
        if (s_initialized)
            return s_available;

        s_initialized = true;

#ifdef __WIIU__
        // Log directly to debug file
        {
            FILE* debugFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (debugFile) {
                fprintf(debugFile, "[CafeGLSL] Initialize() called - using DYNAMIC RPL LOADING\n");
                fflush(debugFile);
                fclose(debugFile);
            }
        }

        WHBLogPrintf("CafeGLSL: Loading glslcompiler.rpl...");

        const char* fileProbePaths[] = {
            "/vol/content/glslcompiler.rpl",
            "/vol/external01/wiiu/apps/balatro/glslcompiler.rpl",
            "glslcompiler.rpl",
        };

        const char* dynloadNames[] = {
            "glslcompiler",
            "glslcompiler.rpl",
            "/vol/content/glslcompiler.rpl",
            "/vol/external01/wiiu/apps/balatro/glslcompiler.rpl",
            "glslcompiler.rpl",
        };

        OSDynLoad_Error result = OS_DYNLOAD_OK;
        const char* loadedPath = nullptr;

        for (const char* path : fileProbePaths)
        {
            FILE* debugFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (debugFile) {
                FILE* probe = fopen(path, "rb");
                fprintf(debugFile, "[CafeGLSL] Probe file path: %s => %s\n", path, probe ? "readable" : "missing");
                if (probe)
                    fclose(probe);
                fflush(debugFile);
                fclose(debugFile);
            }
        }

        for (const char* path : dynloadNames)
        {
            FILE* debugFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (debugFile) {
                fprintf(debugFile, "[CafeGLSL] Trying dynload name: %s\n", path);
                fflush(debugFile);
                fclose(debugFile);
            }

            result = OSDynLoad_Acquire(path, &s_rplHandle);
            if (result == OS_DYNLOAD_OK)
            {
                loadedPath = path;
                break;
            }
        }

        if (!loadedPath || result != OS_DYNLOAD_OK) {
            FILE* debugFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (debugFile) {
                fprintf(debugFile, "[CafeGLSL] Failed to load glslcompiler.rpl, error: %d\n", result);
                fflush(debugFile);
                fclose(debugFile);
            }
            
            WHBLogPrintf("CafeGLSL: Failed to load glslcompiler.rpl, error: %d", result);
            s_available = false;
            return false;
        }

        {
            FILE* debugFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (debugFile) {
                fprintf(debugFile, "[CafeGLSL] Loaded glslcompiler.rpl from: %s\n", loadedPath ? loadedPath : "<unknown>");
                fflush(debugFile);
                fclose(debugFile);
            }
        }
        
        // Get function pointers from RPL
        bool allSymbolsFound = true;
        
        if (OSDynLoad_FindExport(s_rplHandle, OS_DYNLOAD_EXPORT_FUNC, "InitGLSLCompiler", (void**)&rpl_InitGLSLCompiler) != OS_DYNLOAD_OK) {
            WHBLogPrintf("CafeGLSL: Failed to find InitGLSLCompiler");
            allSymbolsFound = false;
        }
        
        if (OSDynLoad_FindExport(s_rplHandle, OS_DYNLOAD_EXPORT_FUNC, "DestroyGLSLCompiler", (void**)&rpl_DestroyGLSLCompiler) != OS_DYNLOAD_OK) {
            WHBLogPrintf("CafeGLSL: Failed to find DestroyGLSLCompiler");
            allSymbolsFound = false;
        }
        
        if (OSDynLoad_FindExport(s_rplHandle, OS_DYNLOAD_EXPORT_FUNC, "CompileVertexShader", (void**)&rpl_CompileVertexShader) != OS_DYNLOAD_OK) {
            WHBLogPrintf("CafeGLSL: Failed to find CompileVertexShader");
            allSymbolsFound = false;
        }
        
        if (OSDynLoad_FindExport(s_rplHandle, OS_DYNLOAD_EXPORT_FUNC, "CompilePixelShader", (void**)&rpl_CompilePixelShader) != OS_DYNLOAD_OK) {
            WHBLogPrintf("CafeGLSL: Failed to find CompilePixelShader");
            allSymbolsFound = false;
        }
        
        if (OSDynLoad_FindExport(s_rplHandle, OS_DYNLOAD_EXPORT_FUNC, "FreeVertexShader", (void**)&rpl_FreeVertexShader) != OS_DYNLOAD_OK) {
            WHBLogPrintf("CafeGLSL: Failed to find FreeVertexShader");
            allSymbolsFound = false;
        }
        
        if (OSDynLoad_FindExport(s_rplHandle, OS_DYNLOAD_EXPORT_FUNC, "FreePixelShader", (void**)&rpl_FreePixelShader) != OS_DYNLOAD_OK) {
            WHBLogPrintf("CafeGLSL: Failed to find FreePixelShader");
            allSymbolsFound = false;
        }
        
        if (!allSymbolsFound) {
            FILE* debugFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (debugFile) {
                fprintf(debugFile, "[CafeGLSL] Not all RPL symbols found\n");
                fflush(debugFile);
                fclose(debugFile);
            }
            
            OSDynLoad_Release(s_rplHandle);
            s_rplHandle = 0;
            s_available = false;
            return false;
        }
        
        // Initialize the compiler
        if (rpl_InitGLSLCompiler) {
            rpl_InitGLSLCompiler();
            
            FILE* debugFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (debugFile) {
                fprintf(debugFile, "[CafeGLSL] RPL compiler initialized successfully\n");
                fflush(debugFile);
                fclose(debugFile);
            }
            
            s_available = true;
            WHBLogPrintf("CafeGLSL: RPL compiler ready");
            return true;
        }
        
        s_available = false;
        return false;
#else
        WHBLogPrintf("CafeGLSL: CafeGLSL not available on this platform");
        s_available = false;
        return false;
#endif
    }

    void CafeGLSLCompiler::Shutdown()
    {
        if (!s_initialized || !s_available)
            return;

#ifdef __WIIU__
        if (rpl_DestroyGLSLCompiler) {
            rpl_DestroyGLSLCompiler();
            WHBLogPrintf("CafeGLSL: RPL compiler destroyed");
        }
        
        if (s_rplHandle) {
            OSDynLoad_Release(s_rplHandle);
            s_rplHandle = 0;
        }
        
        // Reset function pointers
        rpl_InitGLSLCompiler = nullptr;
        rpl_DestroyGLSLCompiler = nullptr;
        rpl_CompileVertexShader = nullptr;
        rpl_CompilePixelShader = nullptr;
        rpl_FreeVertexShader = nullptr;
        rpl_FreePixelShader = nullptr;
#endif

        s_available = false;
        s_initialized = false;
    }

    GX2VertexShader* CafeGLSLCompiler::CompileVertexShader(const std::string& source)
    {
        if (!s_available)
        {
            WHBLogPrintf("CafeGLSL: Compiler not available");
            return nullptr;
        }

#ifdef __WIIU__
        char infoLog[1024] = {0};
        GX2VertexShader* shader = rpl_CompileVertexShader ? rpl_CompileVertexShader(source.c_str(), infoLog, sizeof(infoLog), 0) : nullptr;
        
        if (!shader)
        {
            WHBLogPrintf("CafeGLSL: Failed to compile vertex shader: %s", infoLog);
            return nullptr;
        }
        
        WHBLogPrintf("CafeGLSL: Vertex shader compiled successfully");
        return shader;
#else
        return nullptr;
#endif
    }

    GX2PixelShader* CafeGLSLCompiler::CompilePixelShader(const std::string& source)
    {
        if (!s_available)
        {
            WHBLogPrintf("CafeGLSL: Compiler not available");
            return nullptr;
        }

#ifdef __WIIU__
        char infoLog[1024] = {0};
        GX2PixelShader* shader = rpl_CompilePixelShader ? rpl_CompilePixelShader(source.c_str(), infoLog, sizeof(infoLog), 0) : nullptr;
        
        if (!shader)
        {
            WHBLogPrintf("CafeGLSL: Failed to compile pixel shader: %s", infoLog);
            return nullptr;
        }
        
        WHBLogPrintf("CafeGLSL: Pixel shader compiled successfully");
        return shader;
#else
        return nullptr;
#endif
    }

    bool CafeGLSLCompiler::IsAvailable()
    {
        return s_available;
    }

    std::string CafeGLSLCompiler::GetDefaultVertexShaderSource()
    {
        return R"(
#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

uniform mat4 uProjection;
uniform mat4 uModelView;

out vec2 vTexCoord;
out vec4 vColor;

void main()
{
    gl_Position = uProjection * uModelView * vec4(aPosition, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
        )";
    }

    std::string CafeGLSLCompiler::GetDefaultPixelShaderSource()
    {
        return R"(
#version 330 core

in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uTexture;
uniform bool uHasTexture;

out vec4 FragColor;

void main()
{
    if (uHasTexture)
        FragColor = texture(uTexture, vTexCoord) * vColor;
    else
        FragColor = vColor;
}
        )";
    }
    
    void CafeGLSLCompiler::FreeVertexShader(GX2VertexShader* shader)
    {
        if (!s_available || !shader)
            return;

#ifdef __WIIU__
        if (rpl_FreeVertexShader) {
            rpl_FreeVertexShader(shader);
            WHBLogPrintf("CafeGLSL: Vertex shader freed");
        }
#endif
    }

    void CafeGLSLCompiler::FreePixelShader(GX2PixelShader* shader)
    {
        if (!s_available || !shader)
            return;

#ifdef __WIIU__
        if (rpl_FreePixelShader) {
            rpl_FreePixelShader(shader);
            WHBLogPrintf("CafeGLSL: Pixel shader freed");
        }
#endif
    }

    std::string CafeGLSLCompiler::ConvertLoveShaderToGLSL(const std::string& loveShaderSource)
    {
        // Quick fix: Block problematic CRT shader compilation temporarily
        if (loveShaderSource.size() > 7000 && loveShaderSource.find("MY_HIGHP_OR_MEDIUMP") != std::string::npos) {
            WHBLogPrintf("CafeGLSL: Blocking complex CRT shader compilation temporarily");
            return "";  // Return empty to force fallback
        }
        
        // Based on Love2D official implementation from Shader.cpp
        // Use Love2D's global_syntax approach with #define macros
        std::string glslHeader = 
            "#version 330 core\n"
            "#define LOVE_HIGHP_OR_MEDIUMP highp\n"
            "#define number float\n"
            "#define Image sampler2D\n" 
            "#define ArrayImage sampler2DArray\n"
            "#define CubeImage samplerCube\n"
            "#define VolumeImage sampler3D\n"
            "#define extern uniform\n"
            "#define varying in\n"
            "#define attribute in\n"
            "#ifdef GL_ES\n"
            "    precision mediump float;\n"
            "#endif\n"
            "in vec2 VaryingTexCoord;\n"
            "in vec4 VaryingColor;\n"
            "uniform sampler2D MainTex;\n";
            
        // Add Love2D's Texel function equivalent to texture()
        std::string glslFunctions = 
            "vec4 Texel(sampler2D s, vec2 c) { return texture(s, c); }\n"
            "vec4 Texel(sampler2DArray s, vec3 c) { return texture(s, c); }\n"
            "vec4 Texel(samplerCube s, vec3 c) { return texture(s, c); }\n"
            "vec4 Texel(sampler3D s, vec3 c) { return texture(s, c); }\n";
            
        // Check if shader has effect() function
        std::string shaderBody = loveShaderSource;
        bool hasEffectFunction = shaderBody.find("vec4 effect(") != std::string::npos;
        
        std::string result;
        
        if (hasEffectFunction) {
            // Love2D style pixel shader with effect() function
            result = glslHeader + 
                    "layout(location = 0) out vec4 love_PixelColor;\n" +
                    glslFunctions + shaderBody +
                    "\nvoid main() {\n"
                    "    love_PixelColor = effect(VaryingColor, MainTex, VaryingTexCoord.st, gl_FragCoord.xy);\n"
                    "}\n";
        } else {
            // Raw shader or other format
            result = glslHeader + 
                    "layout(location = 0) out vec4 love_PixelColor;\n" +
                    glslFunctions + shaderBody;
        }
        
        WHBLogPrintf("CafeGLSL: Converted Love2D shader to GLSL (size: %d, hasEffect: %s)", 
                    (int)result.size(), hasEffectFunction ? "yes" : "no");
        return result;
    }

} // namespace love

#pragma once

// CafeGLSL is always enabled

#include <memory>
#include <string>

#ifdef __WIIU__
#include <gx2/shaders.h>
#include <coreinit/dynload.h>
#include <coreinit/debug.h>
#else
// Forward declarations for non-Wii U builds
struct GX2VertexShader;
struct GX2PixelShader;
#endif

namespace love
{
    /**
     * @brief CafeGLSL shader compiler wrapper for LÖVE Potion
     * 
     * This class provides a C++ interface to the CafeGLSL shader compiler
     * allowing runtime GLSL to GX2 shader compilation on Wii U.
     */
    class CafeGLSLCompiler
    {
    public:
        enum class ShaderType
        {
            VERTEX,
            PIXEL
        };

        enum CompilerFlags
        {
            NONE = 0,
            GENERATE_DISASSEMBLY = 1 << 0,
            PRINT_DISASSEMBLY_TO_STDERR = 1 << 0,
        };

        /**
         * @brief Initialize the CafeGLSL compiler
         * @return true if initialization was successful
         */
        static bool Initialize();

        /**
         * @brief Shutdown and cleanup the CafeGLSL compiler
         */
        static void Shutdown();

        /**
         * @brief Check if the compiler is available and initialized
         * @return true if compiler is ready to use
         */
        static bool IsAvailable();

        /**
         * @brief Compile a GLSL vertex shader to GX2VertexShader
         * @param source GLSL source code string
         * @return Compiled vertex shader or nullptr on failure
         */
        static GX2VertexShader* CompileVertexShader(const std::string& source);

        /**
         * @brief Compile a GLSL pixel shader to GX2PixelShader
         * @param source GLSL source code string
         * @return Compiled pixel shader or nullptr on failure
         */
        static GX2PixelShader* CompilePixelShader(const std::string& source);

        /**
         * @brief Get the default vertex shader source for UI rendering
         * @return GLSL source code for basic vertex shader
         */
        static std::string GetDefaultVertexShaderSource();

        /**
         * @brief Get the default pixel shader source for UI rendering
         * @return GLSL source code for basic pixel shader
         */
        static std::string GetDefaultPixelShaderSource();

        /**
         * @brief Convert LÖVE2D shader source to standard GLSL
         * @param loveShaderSource LÖVE2D fragment shader source code
         * @return GLSL source code compatible with CafeGLSL
         */
        static std::string ConvertLoveShaderToGLSL(const std::string& loveShaderSource);

        /**
         * @brief Free a compiled vertex shader
         * @param shader Pointer to shader to free
         */
        static void FreeVertexShader(GX2VertexShader* shader);

        /**
         * @brief Free a compiled pixel shader
         * @param shader Pointer to shader to free
         */
        static void FreePixelShader(GX2PixelShader* shader);

    private:
        static bool s_initialized;
        static bool s_available;

#ifdef __WIIU__
        static OSDynLoad_Module s_compilerModule;
        
        // Function pointers for dynamically loaded CafeGLSL functions
        static GX2VertexShader* (*s_compileVertexShader)(const char*, char*, int, int);
        static GX2PixelShader* (*s_compilePixelShader)(const char*, char*, int, int);
        static void (*s_freeVertexShader)(GX2VertexShader*);
        static void (*s_freePixelShader)(GX2PixelShader*);
        static void (*s_initCompiler)();
        static void (*s_destroyCompiler)();
#endif
    };

    /**
     * @brief RAII wrapper for GX2 shaders
     */
    template<typename ShaderType>
    class GX2ShaderWrapper
    {
    public:
        explicit GX2ShaderWrapper(std::unique_ptr<ShaderType> shader)
            : m_shader(std::move(shader)) {}

        ~GX2ShaderWrapper()
        {
            if (m_shader)
            {
                if constexpr (std::is_same_v<ShaderType, GX2VertexShader>)
                    CafeGLSLCompiler::FreeVertexShader(m_shader.get());
                else if constexpr (std::is_same_v<ShaderType, GX2PixelShader>)
                    CafeGLSLCompiler::FreePixelShader(m_shader.get());
            }
        }

        ShaderType* get() const { return m_shader.get(); }
        ShaderType* operator->() const { return m_shader.get(); }
        ShaderType& operator*() const { return *m_shader; }

        explicit operator bool() const { return m_shader != nullptr; }

    private:
        std::unique_ptr<ShaderType> m_shader;
    };

    using GX2VertexShaderWrapper = GX2ShaderWrapper<GX2VertexShader>;
    using GX2PixelShaderWrapper = GX2ShaderWrapper<GX2PixelShader>;
}

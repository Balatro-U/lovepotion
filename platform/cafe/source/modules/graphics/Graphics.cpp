#include "driver/display/GX2.hpp"

#include "modules/graphics/Graphics.hpp"
#include "modules/window/Window.hpp"

#include "modules/graphics/Shader.hpp"
#include "modules/graphics/ShaderStage.hpp"
#include "modules/graphics/Texture.hpp"
#include "modules/graphics/freetype/Font.hpp"
#ifdef __WIIU__
extern "C" void love_gx2IncrementIssuedDraws();
#endif

#ifdef USE_CAFEGLSL
#include "common/CafeGLSL.hpp"
#endif

#ifdef USE_PPC_DEBUGGER
#include "common/PPCDebugger.hpp"
#endif

#include <gx2/draw.h>
#include <gx2/event.h>
#include <gx2/state.h>
#include <gx2r/draw.h>

#ifdef __WIIU__
#include "WiiUGraphicsOptimizer.hpp"
#endif

namespace love
{
    Graphics::Graphics() : GraphicsBase("love.graphics.gx2")
    {
#ifdef __WIIU__
        uint64_t startTicks = OSGetSystemTime();
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "[GFX] ctor ENTER (this=%p)\n", (void*)this);
            fprintf(logFile, "[GFX] ctor phase 0: pre-initialization startTicks=%llu\n", (unsigned long long)startTicks);
            fflush(logFile);
            fclose(logFile);
        }

#ifdef USE_PPC_DEBUGGER
        // Initialize PPC debugger first for maximum debugging coverage
        PPCDebugger::Initialize();
        PPCDebugger::DebugPoint("GRAPHICS_CONSTRUCTOR_START", "Graphics module initialization beginning");
        PPCDebugger::StartPerformanceTimer("graphics_init");
        PPCDebugger::LogMemoryDump(0x10000000, 256, "GRAPHICS_INIT_MEMORY_STATE");
#endif
#endif

#ifdef USE_CAFEGLSL
        // Initialize CafeGLSL for shader compilation
#ifdef __WIIU__
        {
            FILE* cafeLogFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (cafeLogFile) {
                fprintf(cafeLogFile, "[GFX] Initializing CafeGLSL for shader compilation\n");
                fflush(cafeLogFile);
                fclose(cafeLogFile);
            }
        }
        
        bool cafeGLSLReady = CafeGLSLCompiler::Initialize();
        
        {
            FILE* cafeLogFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (cafeLogFile) {
                fprintf(cafeLogFile, "[GFX] CafeGLSL::Initialize() returned: %s\n", cafeGLSLReady ? "true" : "false");
                fflush(cafeLogFile);
                fclose(cafeLogFile);
            }
        }
#endif
#endif
        
        auto* window = Module::getInstance<Window>(M_WINDOW);

#ifdef __WIIU__
#ifdef USE_PPC_DEBUGGER
        PPCDebugger::DebugPoint("WINDOW_INSTANCE_GET", "Retrieved window instance");
#endif
        FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile2) {
            fprintf(logFile2, "Graphics: got window instance %p\n", (void*)window);
            fflush(logFile2);
            fclose(logFile2);
        }
#endif

        if (window != nullptr)
        {
#ifdef __WIIU__
#ifdef USE_PPC_DEBUGGER
            PPCDebugger::DebugPoint("WINDOW_SET_GRAPHICS", "Setting graphics on window");
#endif
            FILE* logFile3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile3) {
                fprintf(logFile3, "Graphics: setting graphics on window\n");
                fflush(logFile3);
                fclose(logFile3);
            }
#endif
            window->setGraphics(this);

            if (window->isOpen())
            {
#ifdef __WIIU__
#ifdef USE_PPC_DEBUGGER
                PPCDebugger::DebugPoint("WINDOW_IS_OPEN", "Window is open, proceeding with initialization");
#endif
                FILE* logFile4 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (logFile4) {
                    fprintf(logFile4, "Graphics: window is open, setting window parameters\n");
                    fflush(logFile4);
                    fclose(logFile4);
                }
#endif
                int width, height;
                Window::WindowSettings settings {};

                window->getWindow(width, height, settings);
                window->setWindow(width, height, &settings);
#ifdef __WIIU__
                FILE* logFile5 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (logFile5) {
                    fprintf(logFile5, "Graphics: window parameters set (%dx%d)\n", width, height);
                    fflush(logFile5);
                    fclose(logFile5);
                }
#endif
            }
        }
#ifdef __WIIU__
        uint64_t endTicks = OSGetSystemTime();
        FILE* logFile6 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile6) {
            fprintf(logFile6, "[GFX] ctor EXIT total=%.2f ms\n", (double)OSTicksToMilliseconds(endTicks - startTicks));
            fprintf(logFile6, "=== POST-CONSTRUCTOR ANALYSIS ===\nGraphics object: %p\nReturning from Graphics constructor...\n", (void*)this);
            fflush(logFile6);
            fclose(logFile6);
        }
#endif
    }

    Graphics::~Graphics()
    {
#ifdef USE_CAFEGLSL
        // Shutdown CafeGLSL
#ifdef __WIIU__
        FILE* cafeLogFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (cafeLogFile) {
            fprintf(cafeLogFile, "Graphics: Shutting down CafeGLSL\n");
            fflush(cafeLogFile);
            fclose(cafeLogFile);
        }
#endif
        CafeGLSLCompiler::Shutdown();
#endif

#ifdef __WIIU__
        FILE* destructorLogFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (destructorLogFile) {
            fprintf(destructorLogFile, "Graphics: Destructor completed\n");
            fflush(destructorLogFile);
            fclose(destructorLogFile);
        }
#endif
    }

    void Graphics::initCapabilities()
    {
        // clang-format off
        this->capabilities.features[FEATURE_MULTI_RENDER_TARGET_FORMATS]  = false;
        this->capabilities.features[FEATURE_CLAMP_ZERO]                   = true;
        this->capabilities.features[FEATURE_CLAMP_ONE]                    = true;
        this->capabilities.features[FEATURE_BLEND_MINMAX]                 = true;
        this->capabilities.features[FEATURE_LIGHTEN]                      = true;
        this->capabilities.features[FEATURE_FULL_NPOT]                    = true;
        this->capabilities.features[FEATURE_PIXEL_SHADER_HIGHP]           = false;
        this->capabilities.features[FEATURE_SHADER_DERIVATIVES]           = false;
        this->capabilities.features[FEATURE_GLSL3]                        = false;
        this->capabilities.features[FEATURE_GLSL4]                        = false;
        this->capabilities.features[FEATURE_INSTANCING]                   = false;
        this->capabilities.features[FEATURE_TEXEL_BUFFER]                 = false;
        this->capabilities.features[FEATURE_INDEX_BUFFER_32BIT]           = false;
        this->capabilities.features[FEATURE_COPY_BUFFER_TO_TEXTURE]       = false; //< might be possible
        this->capabilities.features[FEATURE_COPY_TEXTURE_TO_BUFFER]       = false; //< might be possible
        this->capabilities.features[FEATURE_COPY_RENDER_TARGET_TO_BUFFER] = false; //< might be possible
        this->capabilities.features[FEATURE_MIPMAP_RANGE]                 = false;
        this->capabilities.features[FEATURE_INDIRECT_DRAW]                = false;
        static_assert(FEATURE_MAX_ENUM == 19,  "Graphics::initCapabilities must be updated when adding a new graphics feature!");

        this->capabilities.limits[LIMIT_POINT_SIZE]                 = 8.0f;
        this->capabilities.limits[LIMIT_TEXTURE_SIZE]               = 4096;
        this->capabilities.limits[LIMIT_TEXTURE_LAYERS]             = 1;
        this->capabilities.limits[LIMIT_VOLUME_TEXTURE_SIZE]        = 4096;
        this->capabilities.limits[LIMIT_CUBE_TEXTURE_SIZE]          = 4096;
        this->capabilities.limits[LIMIT_TEXEL_BUFFER_SIZE]          = 0;
        this->capabilities.limits[LIMIT_SHADER_STORAGE_BUFFER_SIZE] = 0;
        this->capabilities.limits[LIMIT_THREADGROUPS_X]             = 0;
        this->capabilities.limits[LIMIT_THREADGROUPS_Y]             = 0;
        this->capabilities.limits[LIMIT_THREADGROUPS_Z]             = 0;
        this->capabilities.limits[LIMIT_RENDER_TARGETS]             = 1; //< max simultaneous render targets
        this->capabilities.limits[LIMIT_TEXTURE_MSAA]               = 0;
        this->capabilities.limits[LIMIT_ANISOTROPY]                 = 0;
        static_assert(LIMIT_MAX_ENUM == 13, "Graphics::initCapabilities must be updated when adding a new system limit!");
        // clang-format on

        this->capabilities.textureTypes[TEXTURE_2D]       = true;
        this->capabilities.textureTypes[TEXTURE_VOLUME]   = false;
        this->capabilities.textureTypes[TEXTURE_CUBE]     = true;
        this->capabilities.textureTypes[TEXTURE_2D_ARRAY] = false;
    }

    void Graphics::setActiveScreen()
    {
        gx2.ensureInFrame();
        // gx2.copyCurrentScanBuffer();
    }

    void Graphics::clear(OptionalColor color, OptionalInt stencil, OptionalDouble depth)
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            if (color.hasValue) {
                fprintf(logFile, "Graphics::clear() with color: R=%.2f G=%.2f B=%.2f A=%.2f\n", 
                       color.value.r, color.value.g, color.value.b, color.value.a);
            } else {
                fprintf(logFile, "Graphics::clear() with background color\n");
            }
            fflush(logFile);
            fclose(logFile);
        }
#endif
        
        // Ensure we're in a frame before clearing
        gx2.ensureInFrame();
        
        if (color.hasValue)
        {
            bool hasIntegerFormat = false;
            const auto& targets   = this->states.back().renderTargets;

            for (const auto& target : targets.colors)
            {
                if (target.texture.get() && love::isPixelFormatInteger(target.texture->getPixelFormat()))
                    hasIntegerFormat = true;

                if (hasIntegerFormat)
                {
                    std::vector<OptionalColor> colors(targets.colors.size());

                    for (size_t index = 0; index < colors.size(); index++)
                        colors[index] = color;

                    this->clear(colors, stencil, depth);
                    return;
                }
            }
        }

        if (color.hasValue || stencil.hasValue || depth.hasValue) {
#ifdef __WIIU__
            // Only flush if we're not in emergency mode and have drawn enough to matter
            if (!WiiUGraphicsOptimizer::isInEmergencyMode()) {
                this->flushBatchedDraws();
            }
#else
            this->flushBatchedDraws();
#endif
        }

        if (color.hasValue)
        {
            gammaCorrectColor(color.value);
            gx2.clear(color.value);
        }

        gx2.bindFramebuffer(&gx2.getInternalBackbuffer());
        
#ifdef __WIIU__
        FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile2) {
            fprintf(logFile2, "Graphics::clear() completed\n");
            fflush(logFile2);
            fclose(logFile2);
        }
#endif
    }

    GX2ColorBuffer Graphics::getInternalBackbuffer() const
    {
        return gx2.getInternalBackbuffer();
    }

    void Graphics::clear(const std::vector<OptionalColor>& colors, OptionalInt stencil, OptionalDouble depth)
    {
        if (colors.size() == 0 && !stencil.hasValue && !depth.hasValue)
            return;

        int numColors = (int)colors.size();

        const auto& targets       = this->states.back().renderTargets.colors;
        const int numColorTargets = targets.size();

        if (numColors <= 1 &&
            (numColorTargets == 0 || (numColorTargets == 1 && targets[0].texture.get() != nullptr &&
                                      !isPixelFormatInteger(targets[0].texture->getPixelFormat()))))
        {
            this->clear(colors.size() > 0 ? colors[0] : OptionalColor(), stencil, depth);
            return;
        }

        this->flushBatchedDraws();

        numColors = std::min(numColors, numColorTargets);

        for (int index = 0; index < numColors; index++)
        {
            OptionalColor current = colors[index];

            if (!current.hasValue)
                continue;

            Color value(current.value.r, current.value.g, current.value.b, current.value.a);

            gammaCorrectColor(value);
            gx2.clear(value);
        }

        gx2.bindFramebuffer(&gx2.getInternalBackbuffer());
    }

    void Graphics::copyCurrentScanBuffer()
    {
        gx2.copyCurrentScanBuffer();
    }

    void Graphics::present(void* screenshotCallbackData)
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "Graphics::present() called - about to flush batched draws\n");
            fflush(logFile);
            fclose(logFile);
        }
#endif
        try {
        if (!this->isActive())
            return;

        if (this->isRenderTargetActive())
            throw love::Exception("present cannot be called while a render target is active.");

        // CRITICAL: Flush all batched draws before presenting the frame
        this->flushBatchedDraws();

#ifdef USE_CAFEGLSL
        // Enhanced rendering path with CafeGLSL shaders
        if (CafeGLSLCompiler::IsAvailable())
        {
#ifdef __WIIU__
            static int cafeGLSLFrameCount = 0;
            cafeGLSLFrameCount++;
            if (cafeGLSLFrameCount <= 5 || cafeGLSLFrameCount % 120 == 0) {
                FILE* cafeLogFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (cafeLogFile) {
                    fprintf(cafeLogFile, "present() using CafeGLSL enhanced rendering (frame %d)\n", cafeGLSLFrameCount);
                    fflush(cafeLogFile);
                    fclose(cafeLogFile);
                }
            }
#endif
            // TODO: Add enhanced UI shader rendering here
        }
#endif

#ifdef __WIIU__
        static int presentFlushCount = 0;
        presentFlushCount++;
        if (presentFlushCount <= 10 || presentFlushCount % 60 == 0) {
            FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile2) {
                fprintf(logFile2, "present() flushed batched draws (#%d)\n", presentFlushCount);
                fflush(logFile2);
                fclose(logFile2);
            }
        }
#endif

#ifdef __WIIU__
        FILE* logFile3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile3) {
            fprintf(logFile3, "Graphics::present() - about to call gx2.present()\n");
            fflush(logFile3);
            fclose(logFile3);
        }
#endif

    gx2.present();

#ifdef __WIIU__
        FILE* logFile4 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile4) {
            fprintf(logFile4, "Graphics::present() completed\n");
            fflush(logFile4);
            fclose(logFile4);
        }
#endif

    this->drawCalls        = 0;
    this->drawCallsBatched = 0;
    Shader::shaderSwitches = 0;
    } catch (const std::exception& e) {
#ifdef __WIIU__
        FILE* logErr = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logErr) { fprintf(logErr, "Graphics::present() exception: %s\n", e.what()); fflush(logErr); fclose(logErr);} 
#endif
    } catch (...) {
#ifdef __WIIU__
        FILE* logErr2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logErr2) { fprintf(logErr2, "Graphics::present() unknown exception\n"); fflush(logErr2); fclose(logErr2);} 
#endif
    }
    }

    void Graphics::setScissor(const Rect& scissor)
    {
        this->flushBatchedDraws();

        auto& state     = this->states.back();
        double dpiscale = this->getCurrentDPIScale();

        Rect rectangle {};
        rectangle.x = scissor.x * dpiscale;
        rectangle.y = scissor.y * dpiscale;
        rectangle.w = scissor.w * dpiscale;
        rectangle.h = scissor.h * dpiscale;

        gx2.setScissor(rectangle);

        state.scissor     = true;
        state.scissorRect = scissor;
    }

    void Graphics::setPointSize(float size)
    {
        if (size != this->states.back().pointSize)
            this->flushBatchedDraws();

        this->states.back().pointSize = size;
    }

    void Graphics::setScissor()
    {
        if (this->states.back().scissor)
            this->flushBatchedDraws();

        this->states.back().scissor = false;
        gx2.setScissor(Rect::EMPTY);
    }

    void Graphics::setFrontFaceWinding(Winding winding)
    {
        auto& state = this->states.back();

        if (state.winding != winding)
            this->flushBatchedDraws();

        state.winding = winding;

        if (this->isRenderTargetActive())
            winding = (winding == WINDING_CW) ? WINDING_CCW : WINDING_CW; // ???

        gx2.setVertexWinding(winding);
    }

    void Graphics::setColorMask(ColorChannelMask mask)
    {
        this->flushBatchedDraws();

        gx2.setColorMask(mask);
        this->states.back().colorMask = mask;
    }

    void Graphics::setBlendState(const BlendState& state)
    {
        if (!(state == this->states.back().blend))
            this->flushBatchedDraws();

        if (state.operationRGB == BLENDOP_MAX || state.operationA == BLENDOP_MAX ||
            state.operationRGB == BLENDOP_MIN || state.operationA == BLENDOP_MIN)
        {
            if (!capabilities.features[FEATURE_BLEND_MINMAX])
                throw love::Exception(E_BLEND_MIN_MAX_NOT_SUPPORTED);
        }

        if (state.enable)
            gx2.setBlendState(state);

        this->states.back().blend = state;
    }

    bool Graphics::isPixelFormatSupported(PixelFormat format, uint32_t usage)
    {
        format        = this->getSizedFormat(format);
        bool readable = (usage & PIXELFORMATUSAGEFLAGS_SAMPLE) != 0;

        GX2SurfaceFormat color;
        bool supported = GX2::getConstant(format, color);

        return readable && supported;
    }

    void Graphics::setRenderTargetsInternal(const RenderTargets& targets, int pixelWidth, int pixelHeight,
                                            bool hasSRGBTexture)
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "setRenderTargetsInternal() called with pixelWidth=%d, pixelHeight=%d\n", 
                    pixelWidth, pixelHeight);
            fflush(logFile);
            fclose(logFile);
        }
#endif
        
        const auto& state = this->states.back();

        bool isWindow = targets.getFirstTarget().texture == nullptr;

        if (isWindow)
            gx2.bindFramebuffer(&gx2.getInternalBackbuffer());
        else
            gx2.bindFramebuffer((GX2ColorBuffer*)targets.getFirstTarget().texture->getRenderTargetHandle());

        gx2.setViewport({ 0, 0, pixelWidth, pixelHeight });

        if (state.scissor)
            gx2.setScissor(state.scissorRect);
    }

    TextureBase* Graphics::newTexture(const TextureBase::Settings& settings, const TextureBase::Slices* data)
    {
        return new Texture(this, settings, data);
    }

    FontBase* Graphics::newFont(Rasterizer* data)
    {
        return new Font(data, this->states.back().defaultSamplerState);
    }

    FontBase* Graphics::newDefaultFont(int size, const Rasterizer::Settings& settings)
    {
        auto* module = Module::getInstance<FontModuleBase>(Module::M_FONT);

        if (module == nullptr)
            throw love::Exception("Font module has not been loaded.");

        StrongRef<Rasterizer> r(module->newTrueTypeRasterizer(size, settings), Acquire::NO_RETAIN);
        return this->newFont(r.get());
    }

    ShaderStageBase* Graphics::newShaderStageInternal(ShaderStageType stage, const std::string& filepath)
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "Graphics::newShaderStageInternal() called - stage: %d (%s), filepath: %s\n", 
                    stage, (stage == 0 ? "VERTEX" : "PIXEL"), filepath.c_str());
            fflush(logFile);
            fclose(logFile);
        }
        
        // Special handling for .fs files with CafeGLSL
        if (filepath.size() > 3 && filepath.substr(filepath.size() - 3) == ".fs" && 
            stage == SHADERSTAGE_PIXEL) {
            
#ifdef USE_CAFEGLSL
            if (love::CafeGLSLCompiler::IsAvailable()) {
                FILE* convLogFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (convLogFile) {
                    fprintf(convLogFile, "Graphics::newShaderStageInternal() - .fs file detected with CafeGLSL available, attempting conversion: %s\n", filepath.c_str());
                    fflush(convLogFile);
                    fclose(convLogFile);
                }
                
                // Read the .fs file content
                std::FILE* fsFile = std::fopen(filepath.c_str(), "r");
                if (fsFile) {
                    std::fseek(fsFile, 0, SEEK_END);
                    long fileSize = std::ftell(fsFile);
                    std::rewind(fsFile);
                    
                    std::string fsContent(fileSize, '\0');
                    std::fread(&fsContent[0], 1, fileSize, fsFile);
                    std::fclose(fsFile);
                    
                    // Convert LÖVE2D shader to GLSL
                    std::string glslSource = love::CafeGLSLCompiler::ConvertLoveShaderToGLSL(fsContent);
                    
                    // Compile with CafeGLSL
                    GX2PixelShader* compiledPS = love::CafeGLSLCompiler::CompilePixelShader(glslSource);
                    
                    if (compiledPS) {
                        FILE* successLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                        if (successLog) {
                            fprintf(successLog, "Graphics::newShaderStageInternal() - Successfully converted and compiled .fs shader: %s\n", filepath.c_str());
                            fflush(successLog);
                            fclose(successLog);
                        }
                        
                        // Create a special shader stage that holds the compiled shader directly
                        // For now, fall back to .gsh path since we need to refactor ShaderStage to support this
                    }
                }
            }
#endif
        }
#endif
        return new ShaderStage(stage, filepath);
    }

    ShaderBase* Graphics::newShaderInternal(StrongRef<ShaderStageBase> stages[SHADERSTAGE_MAX_ENUM],
                                            const ShaderBase::CompileOptions& options)
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "Graphics::newShaderInternal() called - about to create new Shader\n");
            fflush(logFile);
            fclose(logFile);
        }
#endif
    // The Shader constructor will throw love::Exception if loadVolatile fails.
    Shader* shader = new Shader(stages, options);
#ifdef __WIIU__
        FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile2) {
            fprintf(logFile2, "Graphics::newShaderInternal() - Shader created successfully: %p\n", shader);
            fflush(logFile2);
            fclose(logFile2);
        }
#endif
        return shader;
    }

    bool Graphics::setMode(int width, int height, int pixelWidth, int pixelHeight, bool backBufferStencil,
                           bool backBufferDepth, int msaa)
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "Graphics::setMode() called with %dx%d (pixel: %dx%d)\n", 
                    width, height, pixelWidth, pixelHeight);
            fprintf(logFile, "Graphics::setMode() - current transform matrix dump:\n");
            fflush(logFile);
            fclose(logFile);
        }
#endif
        
        // Store the pixel dimensions in member variables so getPixelWidth()/getPixelHeight() work correctly
        this->pixelWidth = pixelWidth;
        this->pixelHeight = pixelHeight;
        
        gx2.initialize();

#ifdef __WIIU__
        FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile2) {
            fprintf(logFile2, "Graphics: gx2.initialize() completed\n");
            fflush(logFile2);
            fclose(logFile2);
        }
#endif

        this->created = true;
        this->initCapabilities();

#ifdef __WIIU__
        FILE* logFile3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile3) {
            fprintf(logFile3, "Graphics: initCapabilities() completed\n");
            fflush(logFile3);
            fclose(logFile3);
        }
#endif

#ifdef __WIIU__
        // Early readiness marker before heavy shader creation
        {
            FILE* readyLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (readyLog) {
                static uint64_t ctorStart = 0; // best-effort (may be 0 if not set)
                uint64_t now = OSGetSystemTime();
                fprintf(readyLog, "[GFX] EARLY_READY after initCapabilities (tick=%llu)\n", (unsigned long long)now);
                fflush(readyLog);
                fclose(readyLog);
            }
        }
#endif

        // gx2.setupContext();

        try
        {
            if (this->batchedDrawState.vertexBuffer == nullptr)
            {
#ifdef __WIIU__
                FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (logFile) {
                    fprintf(logFile, "Graphics: About to create index buffer with size %d\n", INIT_INDEX_BUFFER_SIZE);
                    fflush(logFile);
                    fclose(logFile);
                }
#endif
                this->batchedDrawState.indexBuffer  = newIndexBuffer(INIT_INDEX_BUFFER_SIZE);
#ifdef __WIIU__
                FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (logFile2) {
                    fprintf(logFile2, "Graphics: Index buffer created successfully\n");
                    fflush(logFile2);
                    fclose(logFile2);
                }
#endif
                
#ifdef __WIIU__
                FILE* logFile3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (logFile3) {
                    fprintf(logFile3, "Graphics: About to create vertex buffer with size %d\n", INIT_VERTEX_BUFFER_SIZE);
                    fflush(logFile3);
                    fclose(logFile3);
                }
#endif
                this->batchedDrawState.vertexBuffer = newVertexBuffer(INIT_VERTEX_BUFFER_SIZE);
#ifdef __WIIU__
                FILE* logFile4 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (logFile4) {
                    fprintf(logFile4, "Graphics: Vertex buffer created successfully\n");
                    fflush(logFile4);
                    fclose(logFile4);
                }
#endif
            }
        }
        catch (love::Exception&)
        {
            throw;
        }

#ifdef __WIIU__
        FILE* logFile5 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile5) {
            fprintf(logFile5, "Graphics: Buffers created, about to load volatile objects\n");
            fflush(logFile5);
            fclose(logFile5);
        }
#endif

        if (!Volatile::loadAll())
            std::printf("Failed to load all volatile objects.\n");

#ifdef __WIIU__
        FILE* logFile6 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile6) {
            fprintf(logFile6, "Graphics: Volatile objects loaded, about to restore state\n");
            fflush(logFile6);
            fclose(logFile6);
        }
#endif

        this->restoreState(this->states.back());

#ifdef __WIIU__
        FILE* logFile7 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile7) {
            fprintf(logFile7, "Graphics: State restored, now trying to initialize CafeGLSL\n");
            fflush(logFile7);
            fclose(logFile7);
        }

        FILE* logFile7b = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile7b) {
            fprintf(logFile7b, "Graphics: About to create standard shaders (CafeGLSL disabled)\n");
            fflush(logFile7b);
            fclose(logFile7b);
        }
#endif

        for (int index = 0; index < ShaderBase::STANDARD_MAX_ENUM; index++)
        {
            auto type = (Shader::StandardShader)index;

#ifdef __WIIU__
            FILE* logFile8 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile8) {
                fprintf(logFile8, "Graphics: Creating standard shader %d\n", index);
                fflush(logFile8);
                fclose(logFile8);
            }
#endif

            if (!Shader::standardShaders[index])
            {
                std::vector<std::string> stages {};
                Shader::CompileOptions options {};

                stages.push_back(Shader::getDefaultStagePath(type, SHADERSTAGE_VERTEX));
                stages.push_back(Shader::getDefaultStagePath(type, SHADERSTAGE_PIXEL));

#ifdef __WIIU__
                FILE* logFile9 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (logFile9) {
                    fprintf(logFile9, "Graphics: About to create shader %d with stages\n", index);
                    fflush(logFile9);
                    fclose(logFile9);
                }
#endif

                try
                {
                    Shader::standardShaders[type] = this->newShader(stages, options);
#ifdef __WIIU__
                    FILE* logFile10 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                    if (logFile10) {
                        fprintf(logFile10, "Graphics: Shader %d created successfully\n", index);
                        fflush(logFile10);
                        fclose(logFile10);
                    }
#endif
                }
                catch (const std::exception& e)
                {
#ifdef __WIIU__
                    FILE* logFile11 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                    if (logFile11) {
                        fprintf(logFile11, "Graphics: Exception creating shader %d: %s\n", index, e.what());
                        fflush(logFile11);
                        fclose(logFile11);
                    }
#endif
                    throw;
                }
            }
        }

#ifdef __WIIU__
        FILE* logFile12 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile12) {
            fprintf(logFile12, "Graphics: All standard shaders created, about to attach default shader\n");
            fflush(logFile12);
            fclose(logFile12);
        }
#endif

        if (!Shader::current)
            Shader::standardShaders[Shader::STANDARD_DEFAULT]->attach();

#ifdef __WIIU__
        FILE* logFile13 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile13) {
            fprintf(logFile13, "Graphics: Default shader attached, setMode() completed successfully\n");
            fflush(logFile13);
            fclose(logFile13);
        }
        
        // Show that graphics system is ready
        // Note: We cannot call drawLove2DLoading here as it might conflict with GX2
        // Instead we'll log that graphics is ready for the main loop
#endif

        return true;
    }

    void Graphics::unsetMode()
    {
        if (!this->isCreated())
            return;

        this->flushBatchedDraws();
        // gx2.deInitialize();
    }

    void Graphics::setViewport(int x, int y, int width, int height)
    {
        gx2.setViewport({ x, y, width, height });
    }

    void Graphics::draw(const DrawIndexedCommand& command)
    {
#ifdef __WIIU__
        // Skip excessive draw calls to prevent performance issues
        if (WiiUGraphicsOptimizer::shouldSkipDrawCall()) {
            return;
        }
        
        // Only call prepareDraw if really needed
        static int frameNumber = 0;
        if (!WiiUGraphicsOptimizer::shouldSkipPrepareDraw(frameNumber)) {
            gx2.prepareDraw(this);
        }
#else
        gx2.prepareDraw(this);
#endif
        
        gx2.bindTextureToUnit(command.texture, 0);
        // gx2.setCullMode(command.cullMode);

        const auto mode              = GX2::getPrimitiveType(command.primitiveType);
        auto* buffer                 = (GX2RBuffer*)command.indexBuffer->getHandle();
        const auto indexType         = GX2::getIndexType(command.indexType);
        const uint32_t count         = (uint32_t)command.indexCount;
        const uint32_t offset        = (uint32_t)command.indexBufferOffset;
        const uint32_t instanceCount = (uint32_t)command.instanceCount;

        GX2RDrawIndexed(mode, buffer, indexType, count, offset, 0, instanceCount);
        ++this->drawCalls;
#ifdef __WIIU__
    love_gx2IncrementIssuedDraws();
    FILE* drawLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
    if (drawLog) {
        fprintf(drawLog, "[DRAW-DETAIL] Indexed final issuedDraws=%u\n", gx2.getIssuedDrawsThisFrame());
        fclose(drawLog);
    }
#endif
    }

    void Graphics::draw(const DrawCommand& command)
    {
#ifdef __WIIU__
        // Skip excessive draw calls to prevent performance issues
        if (WiiUGraphicsOptimizer::shouldSkipDrawCall()) {
            return;
        }
        
        // Only call prepareDraw if really needed
        static int frameNumber = 0;
        if (!WiiUGraphicsOptimizer::shouldSkipPrepareDraw(frameNumber)) {
            gx2.prepareDraw(this);
        }
#else
        gx2.prepareDraw(this);
#endif

        const auto mode        = GX2::getPrimitiveType(command.primitiveType);
        const auto vertexCount = command.vertexCount;
        const auto vertexStart = command.vertexStart;

        GX2DrawEx(mode, vertexCount, vertexStart, 1);
    ++this->drawCalls;
#ifdef __WIIU__
    love_gx2IncrementIssuedDraws();
    FILE* drawLog2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
    if (drawLog2) {
        fprintf(drawLog2, "[DRAW-DETAIL] NonIndexed final issuedDraws=%u\n", gx2.getIssuedDrawsThisFrame());
        fclose(drawLog2);
    }
#endif
    }
} // namespace love

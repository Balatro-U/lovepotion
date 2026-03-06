#include "driver/display/GX2.hpp"
#include "driver/display/Uniform.hpp"

/* keyboard needs GX2 inited first */
#include "modules/graphics/Shader.hpp"
#include "modules/keyboard/Keyboard.hpp"

#ifdef __WIIU__
#include "WiiUGraphicsOptimizer.hpp"
#endif

#include <gx2/clear.h>
#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/event.h>
#include <gx2/state.h>
#include <gx2/swap.h>

#include <proc_ui/procui.h>
#include <coreinit/screen.h>

#include <malloc.h>

namespace love
{
#define Keyboard() (Module::getInstance<Keyboard>(Module::M_KEYBOARD))

    GX2::GX2() :
        targets {},
        context {},
        inForeground(false),
        consecutivePresentCalls(0),
        commandBuffer(nullptr),
        state(nullptr),
        dirtyProjection(false)
    {}

    GX2::~GX2()
    {
        this->deInitialize();
    }

    void GX2::deInitialize()
    {
        if (this->inForeground)
            this->onForegroundReleased();

        GX2Shutdown();

        delete this->uniform;

        free(this->state);
        this->state = nullptr;

        free(this->commandBuffer);
        this->commandBuffer = nullptr;
    }

    int GX2::onForegroundAcquired()
    {
        this->inForeground = true;

        auto foregroundHeap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_FG);
        auto memOneHeap     = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);

        for (auto& target : this->targets)
        {
            if (!target.allocateScanBuffer(foregroundHeap))
                return -1;

            if (!target.invalidateColorBuffer(memOneHeap))
                return -2;

            if (!target.invalidateDepthBuffer(memOneHeap))
                return -3;
        }

        return 0;
    }

    static uint32_t ProcUIAcquired(void*)
    {
        return gx2.onForegroundAcquired();
    }

    int GX2::onForegroundReleased()
    {
        GX2DrawDone();

        auto foregroundHeap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_FG);
        auto memOneHeap     = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);

        MEMFreeToFrmHeap(foregroundHeap, MEM_FRM_HEAP_FREE_ALL);
        MEMFreeToFrmHeap(memOneHeap, MEM_FRM_HEAP_FREE_ALL);

        this->inForeground = false;

        return 0;
    }

    static uint32_t ProcUIReleased(void*)
    {
        return gx2.onForegroundReleased();
    }

    void GX2::initialize()
    {
        if (this->initialized)
            return;

        this->commandBuffer = memalign(GX2_COMMAND_BUFFER_ALIGNMENT, GX2_COMMAND_BUFFER_SIZE);

        if (!this->commandBuffer)
            throw love::Exception("Failed to allocate GX2 command buffer.");

        // clang-format off
        uint32_t attributes[9] =
        {
            GX2_INIT_CMD_BUF_BASE, (uintptr_t)this->commandBuffer,
            GX2_INIT_CMD_BUF_POOL_SIZE, GX2_COMMAND_BUFFER_SIZE,
            GX2_INIT_ARGC, 0, GX2_INIT_ARGV, 0,
            GX2_INIT_END
        };
        // clang-format on

        GX2Init(attributes);

        this->state = (GX2ContextState*)memalign(GX2_CONTEXT_STATE_ALIGNMENT, sizeof(GX2ContextState));

        if (!this->state)
            throw love::Exception("Failed to allocate GX2 context state.");

        GX2SetupContextStateEx(this->state, false);
        GX2SetContextState(this->state);

        this->createFramebuffers();

        GX2SetDepthOnlyControl(false, false, GX2_COMPARE_FUNC_ALWAYS);
        // GX2SetAlphaTest(false, GX2_COMPARE_FUNC_ALWAYS, 0.0f);

        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, false, true);
        GX2SetSwapInterval(1);

        ProcUIRegisterCallback(PROCUI_CALLBACK_ACQUIRE, ProcUIAcquired, nullptr, 100);
        ProcUIRegisterCallback(PROCUI_CALLBACK_RELEASE, ProcUIReleased, nullptr, 100);

        if (auto result = this->onForegroundAcquired(); result != 0)
            throw love::Exception("Failed to acquire foreground: {:d}", result);

        if (Keyboard())
            Keyboard()->initialize();

        this->context.winding     = GX2_FRONT_FACE_CCW;
        this->context.cullBack    = false;
        this->context.cullFront   = false;
        this->context.depthTest   = false;
        this->context.depthWrite  = true;
        this->context.compareMode = GX2_COMPARE_FUNC_ALWAYS;

        this->uniform             = (Uniform*)memalign(GX2_UNIFORM_BLOCK_ALIGNMENT, sizeof(Uniform));
        this->uniform->modelView  = glm::mat4(1.0f);
        this->uniform->projection = glm::mat4(1.0f);

        this->bindFramebuffer(&this->targets[0].get());

        this->initialized = true;
    }

    void GX2::createFramebuffers()
    {
        const auto info = love::getScreenInfo();

        for (size_t index = 0; index < info.size(); ++index)
            this->targets[index].create(info[index]);
    }

    GX2ColorBuffer& GX2::getInternalBackbuffer()
    {
        return this->targets[love::currentScreen].get();
    }

    GX2DepthBuffer& GX2::getInternalDepthbuffer()
    {
        return this->targets[love::currentScreen].getDepth();
    }

    GX2ColorBuffer* GX2::getFramebuffer()
    {
        return this->context.boundFramebuffer;
    }

    void GX2::destroyFramebuffers()
    {
        for (auto& target : this->targets)
            target.destroy();
    }

    void GX2::ensureInFrame()
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "GX2::ensureInFrame() called, inFrame=%s prevIssued=%u frameIdx? (will set below)\n", this->inFrame ? "true" : "false", this->issuedDrawsThisFrame);
            fflush(logFile);
            fclose(logFile);
        }
#endif
        
        GX2SetContextState(this->state);

        if (!this->inFrame)
        {
#ifdef __WIIU__
            static uint64_t frameCounter = 0;
            frameCounter++;
            FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile2) {
                fprintf(logFile2, "GX2::ensureInFrame() - starting new frame #%llu (reset consecutivePresentCalls, prevIssued=%u)\n", (unsigned long long)frameCounter, this->issuedDrawsThisFrame);
                fflush(logFile2);
                fclose(logFile2);
            }
            // Reset consecutive present calls counter at start of new frame
            this->consecutivePresentCalls = 0;
            
            // Reset draw call optimization counters for new frame
            #ifdef __WIIU__
            WiiUGraphicsOptimizer::startFrame();
            #endif
#endif
            this->inFrame = true;
            // Reset per-frame issued draw counter (added for conditional debug fill)
            this->issuedDrawsThisFrame = 0;
            // Stash current frame index in a static for present() logging
            this->dirtyProjection = this->dirtyProjection; // no-op to silence potential unused warnings
        }
        
#ifdef __WIIU__
        FILE* logFile3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile3) {
            fprintf(logFile3, "GX2::ensureInFrame() completed, inFrame = true\n");
            fflush(logFile3);
            fclose(logFile3);
        }
#endif
    }

    void GX2::copyCurrentScanBuffer()
    {
        Graphics::flushBatchedDrawsGlobal();
        Graphics::advanceStreamBuffersGlobal();

        this->targets[love::currentScreen].copyScanBuffer();

        GX2Flush();
        GX2WaitForFlip();
    }

    void GX2::clear(const Color& color)
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "GX2::clear() called with color: R=%.2f G=%.2f B=%.2f A=%.2f, inFrame = %s\n", 
                   color.r, color.g, color.b, color.a, this->inFrame ? "true" : "false");
            fflush(logFile);
            fclose(logFile);
        }
#endif
        
        if (!this->inFrame)
        {
#ifdef __WIIU__
            FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile2) {
                fprintf(logFile2, "GX2::clear() - not in frame, calling ensureInFrame\n");
                fflush(logFile2);
                fclose(logFile2);
            }
#endif
            this->ensureInFrame();
        }

#ifdef __WIIU__
        FILE* logFile3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile3) {
            fprintf(logFile3, "GX2::clear() - clearing framebuffer\n");
            fflush(logFile3);
            fclose(logFile3);
        }
#endif
        
        GX2ClearColor(this->getFramebuffer(), color.r, color.g, color.b, color.a);
        GX2SetContextState(this->state);
        
#ifdef __WIIU__
        FILE* logFile4 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile4) {
            fprintf(logFile4, "GX2::clear() completed\n");
            fflush(logFile4);
            fclose(logFile4);
        }
#endif
    }

    void GX2::clearDepthStencil(int depth, uint8_t mask, double stencil)
    {
        // GX2ClearDepthStencilEx(&this->getInternalDepthbuffer(), depth, stencil, GX2_CLEAR_FLAGS_BOTH);
        // GX2SetContextState(this->state);
    }

    void GX2::bindFramebuffer(GX2ColorBuffer* target)
    {
        bool bindingModified = false;

        if (this->context.boundFramebuffer != target)
        {
            bindingModified                = true;
            this->context.boundFramebuffer = target;
        }

        if (bindingModified)
        {
            GX2SetColorBuffer(target, GX2_RENDER_TARGET_0);
            this->setMode(target->surface.width, target->surface.height);
        }
    }

    void GX2::setMode(int width, int height)
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "GX2::setMode() called with width=%d height=%d\n", width, height);
            fflush(logFile);
            fclose(logFile);
        }
#endif

        this->setViewport({ 0, 0, width, height });
        this->setScissor({ 0, 0, width, height });

        auto* newUniform = this->targets[love::currentScreen].getUniform();
        std::memcpy(this->uniform, newUniform, sizeof(Uniform));

#ifdef __WIIU__
        FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile2) {
            fprintf(logFile2, "GX2::setMode() completed, context.viewport=(%d,%d %dx%d)\n", 
                    this->context.viewport.x, this->context.viewport.y, this->context.viewport.w, this->context.viewport.h);
            fflush(logFile2);
            fclose(logFile2);
        }
#endif
    }

    void GX2::setSamplerState(TextureBase* texture, const SamplerState& state)
    {
        auto* sampler = (GX2Sampler*)texture->getSamplerHandle();
        GX2InitSampler(sampler, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_XY_FILTER_MODE_LINEAR);

        GX2TexXYFilterMode minFilter;

        if (!GX2::getConstant(state.minFilter, minFilter))
            return;

        GX2TexXYFilterMode magFilter;

        if (!GX2::getConstant(state.magFilter, magFilter))
            return;

        GX2InitSamplerXYFilter(sampler, magFilter, minFilter, GX2_TEX_ANISO_RATIO_NONE);

        GX2TexClampMode wrapU;

        if (!GX2::getConstant(state.wrapU, wrapU))
            return;

        GX2TexClampMode wrapV;

        if (!GX2::getConstant(state.wrapV, wrapV))
            return;

        GX2TexClampMode wrapW;

        if (!GX2::getConstant(state.wrapW, wrapW))
            return;

        GX2InitSamplerClamping(sampler, wrapU, wrapV, wrapW);
        GX2InitSamplerLOD(sampler, state.minLod, state.maxLod, state.lodBias);
    }

    void GX2::prepareDraw(GraphicsBase* graphics)
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            // Snapshot current context state minimal (will expand if needed)
            fprintf(logFile, "GX2::prepareDraw() called issuedDraws=%u inFrame=%d\n", this->issuedDrawsThisFrame, this->inFrame?1:0);
            fflush(logFile);
            fclose(logFile);
        }
#endif
        
        // Ensure we're in frame before any drawing operations
        this->ensureInFrame();
        
        // --- Uniform self-heal: sometimes modelView/projection are zero/garbage (observed in logs) ---
        if (this->uniform) {
            const float* mv = (const float*)&this->uniform->modelView;
            const float* pr = (const float*)&this->uniform->projection;
            bool mvAllZero = true;
            for (int i=0;i<16;i++) if (mv[i] != 0.0f) { mvAllZero = false; break; }
            bool prAllZero = true;
            for (int i=0;i<16;i++) if (pr[i] != 0.0f) { prAllZero = false; break; }
            bool prHuge = false;
            for (int i=0;i<16;i++) { float a = pr[i]; if (a > 1e9f || a < -1e9f) { prHuge = true; break; } }
            if (mvAllZero || prAllZero || prHuge) {
                // Rebuild sane matrices
                int w = this->context.viewport.w ? this->context.viewport.w : 1280;
                int h = this->context.viewport.h ? this->context.viewport.h : 720;
                // Identity modelView
                this->uniform->modelView = glm::mat4(1.0f);
                // Ortho 0..w x 0..h (Y down) similar to glm::ortho(left,right,bottom,top,zNear,zFar)
                // We want top=0, bottom=h so pass bottom=h, top=0
                this->uniform->projection = glm::ortho(0.0f, (float)w, (float)h, 0.0f, -10.0f, 10.0f);
#ifdef __WIIU__
                FILE* healLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (healLog) {
                    fprintf(healLog, "GX2::prepareDraw() UNIFORM REPAIR triggered (mvAllZero=%d prAllZero=%d prHuge=%d) viewport=%dx%d\n", (int)mvAllZero, (int)prAllZero, (int)prHuge, w, h);
                    fclose(healLog);
                }
#endif
            }
        }

        if (Shader::current != nullptr)
        {
            auto* shader = (Shader*)ShaderBase::current;
            shader->updateBuiltinUniforms(graphics, this->uniform);
#ifdef __WIIU__
        // Log a snapshot of the modelView and projection matrices to confirm values look sane
        FILE* matLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (matLog) {
        const float* mv = (const float*)&this->uniform->modelView;
        const float* pr = (const float*)&this->uniform->projection;
        // Row-major print (glm default column-major, but we just show raw floats sequentially)
        fprintf(matLog, "GX2::prepareDraw() uniform modelView[0..7]=[% .3f % .3f % .3f % .3f | % .3f % .3f % .3f % .3f]\n",
            mv[0], mv[1], mv[2], mv[3], mv[4], mv[5], mv[6], mv[7]);
        fprintf(matLog, "GX2::prepareDraw() uniform projection[0..7]=[% .3f % .3f % .3f % .3f | % .3f % .3f % .3f % .3f]\n",
            pr[0], pr[1], pr[2], pr[3], pr[4], pr[5], pr[6], pr[7]);
        // Attempt to transform a captured CPU-space vertex (if provided by higher-level logging)
        extern float g_lastPolyX; extern float g_lastPolyY; extern bool g_haveLastPoly;
        if (g_haveLastPoly) {
            glm::vec4 clip = this->uniform->projection * this->uniform->modelView * glm::vec4(g_lastPolyX, g_lastPolyY, 0.0f, 1.0f);
            float ndcX = (clip.w != 0.0f) ? clip.x / clip.w : 0.0f;
            float ndcY = (clip.w != 0.0f) ? clip.y / clip.w : 0.0f;
            fprintf(matLog, "GX2::prepareDraw() sampleTransform src=(%.2f,%.2f) clip=(%.3f,%.3f,%.3f,%.3f) ndc=(%.3f,%.3f)\n", g_lastPolyX, g_lastPolyY, clip.x, clip.y, clip.z, clip.w, ndcX, ndcY);
        } else {
            fprintf(matLog, "GX2::prepareDraw() sampleTransform not available (no polygon captured)\n");
        }
        fflush(matLog);
        fclose(matLog);
        }
#endif
        }
        
#ifdef __WIIU__
        FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile2) {
            fprintf(logFile2, "GX2::prepareDraw() completed (shader=%p boundFramebuffer=%p)\n", (void*)ShaderBase::current, (void*)this->context.boundFramebuffer);
            fflush(logFile2);
            fclose(logFile2);
        }
#endif
    }

    void GX2::bindTextureToUnit(TextureBase* texture, int unit)
    {
        if (texture == nullptr)
            return;

        auto* handle = (GX2Texture*)texture->getHandle();

        if (handle == nullptr)
            return;

        auto* sampler = (GX2Sampler*)texture->getSamplerHandle();

        if (sampler == nullptr)
            return;

        this->bindTextureToUnit(handle, sampler, unit);
    }

    void GX2::bindTextureToUnit(GX2Texture* texture, GX2Sampler* sampler, int unit)
    {
        auto* shader = (Shader*)ShaderBase::current;
        const ShaderBase::UniformInfo* info = nullptr;
        // Try common sampler names across examples
        static const char* names[] = { "texture0", "uTexture", "uTex", "mainTex", "tex0" };
        for (const char* n : names) {
            info = shader->getUniformInfo(n);
            if (info) break;
        }
        if (!info)
            return;

        GX2SetPixelTexture(texture, unit);
        GX2SetPixelSampler(sampler, info->location);
    }

    void GX2::present()
    {
#ifdef __WIIU__
        // CRITICAL: Detect endless loop and trigger fallback
        static bool fallbackTriggered = false;
        static uint32_t debugFrameCounter = 0; // used for alternating test pattern
        
        this->consecutivePresentCalls++;
        
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "GX2::present() callCount=%d inFrame=%s issuedDrawsThisFrame=%u applyingFallbackTestColor=%s\n", this->consecutivePresentCalls, this->inFrame ? "true" : "false", this->issuedDrawsThisFrame, (this->issuedDrawsThisFrame==0?"yes":"no"));
            fflush(logFile);
            fclose(logFile);
        }
        
        // DISABLED: Fallback detection to prevent black screen
        // If we've had too many consecutive present calls without a reset, trigger fallback
        // if (this->consecutivePresentCalls > 30 && !fallbackTriggered) {
        //     fallbackTriggered = true;
        //     
        //     FILE* fallbackLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        //     if (fallbackLog) {
        //         fprintf(fallbackLog, "=== ENDLESS LOOP DETECTED: TRIGGERING FALLBACK DIAGNOSTIC SCREEN ===\n");
        //         fprintf(fallbackLog, "Consecutive present calls: %d\n", this->consecutivePresentCalls);
        //         fprintf(fallbackLog, "This indicates lua_resume() is hanging in an endless loop\n");
        //         fflush(fallbackLog);
        //         fclose(fallbackLog);
        //     }
        //     
        //     // Force entry into diagnostic fallback mode
        //     this->showFallbackDiagnosticScreen();
        //     return; // Skip normal present logic
        // }
#endif
        
        if (!this->inFrame)
        {
#ifdef __WIIU__
            FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile2) {
                fprintf(logFile2, "GX2::present() - not in frame, calling ensureInFrame\n");
                fflush(logFile2);
                fclose(logFile2);
            }
#endif
            this->ensureInFrame();
        }
        
#ifdef __WIIU__
    // Acquire framebuffer pointer before logging so we can safely print it
    GX2ColorBuffer* fb = this->getFramebuffer();
        FILE* logFile3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile3) {
        // Corrected format string: viewport=(x,y wxh) and ensure all placeholders match arguments
        fprintf(logFile3, "GX2::present(): presenting issuedDraws=%u (will %s fallback clear) fb=%p viewport=(%d,%d %dx%d) scissor=(%d,%d %dx%d)\n", 
            this->issuedDrawsThisFrame, this->issuedDrawsThisFrame==0?"apply":"skip", (void*)fb,
            this->context.viewport.x, this->context.viewport.y, this->context.viewport.w, this->context.viewport.h,
            this->context.scissor.x, this->context.scissor.y, this->context.scissor.w, this->context.scissor.h);
            fflush(logFile3);
            fclose(logFile3);
        }
#endif
        
        // Conditionally apply debug fill ONLY if no draws occurred this frame.
        // This lets us still see real content once drawing works.
    // fb already acquired above (WiiU build). For non-WiiU builds acquire now.
#ifndef __WIIU__
    GX2ColorBuffer* fb = this->getFramebuffer();
#endif
    if (fb && this->issuedDrawsThisFrame == 0) {
            float r = (debugFrameCounter & 1) ? 1.0f : 0.05f;
            float g = (debugFrameCounter & 1) ? 0.05f : 1.0f;
            float b = 0.4f;
            GX2ClearColor(fb, r, g, b, 1.0f);
#ifdef __WIIU__
        FILE* lf = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (lf) { fprintf(lf, "GX2::present(): applied fallback debug clear color frameCounter=%u\n", debugFrameCounter); fclose(lf);} 
#endif
        }

#ifdef __WIIU__
        // Extra diagnostic: if a control file exists, force an alternating bright clear even when draws occurred.
        // Create an empty file at fs:/vol/external01/force_overlay_debug to enable.
        {
            FILE* ctl = fopen("fs:/vol/external01/force_overlay_debug", "r");
            if (ctl) {
                fclose(ctl);
                if (fb) {
                    float r = (debugFrameCounter & 1) ? 0.9f : 0.1f;
                    float g = (debugFrameCounter & 1) ? 0.1f : 0.9f;
                    float b = 0.1f;
                    GX2ClearColor(fb, r, g, b, 1.0f);
                    FILE* overLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                    if (overLog) { fprintf(overLog, "GX2::present(): force_overlay_debug applied AFTER draws (issued=%u)\n", this->issuedDrawsThisFrame); fclose(overLog);} 
                }
            }
        }
#endif
        
        // Present each screen using its matching framebuffer.
        for (size_t index = 0; index < love::getScreenInfo().size(); ++index)
            this->targets[index].copyScanBuffer();
        
        // Swap buffers for both screens
        GX2SwapScanBuffers();
        GX2Flush();
        GX2WaitForVsync();
        
    this->inFrame = false;
    debugFrameCounter++;
        
#ifdef __WIIU__
        FILE* logFile4 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile4) {
            fprintf(logFile4, "GX2::present() completed issuedDraws=%u nextFrameInFrame=false\n", this->issuedDrawsThisFrame);
            fflush(logFile4);
            fclose(logFile4);
        }
#endif
    }

    void GX2::setViewport(const Rect& rect)
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "GX2::setViewport() called with rect=(%d,%d %dx%d)\n", rect.x, rect.y, rect.w, rect.h);
            fflush(logFile);
            fclose(logFile);
        }
#endif

        Rect view = rect;
        // Fix: Check for empty viewport properly (Rect::EMPTY is an array, not comparable directly)
        if (rect.x == -1 && rect.y == -1 && rect.w == -1 && rect.h == -1)
            view = this->targets[love::currentScreen].getViewport();

#ifdef __WIIU__
        FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile2) {
            fprintf(logFile2, "GX2::setViewport() final view=(%d,%d %dx%d) calling GX2SetViewport\n", view.x, view.y, view.w, view.h);
            fflush(logFile2);
            fclose(logFile2);
        }
#endif

        GX2SetViewport(view.x, view.y, view.w, view.h, Framebuffer::Z_NEAR, Framebuffer::Z_FAR);
        this->context.viewport = view;
    }

    void GX2::setScissor(const Rect& rect)
    {
        Rect scissor = rect;
        // Fix: Check for empty scissor properly (Rect::EMPTY is an array, not comparable directly)
        if (rect.x == -1 && rect.y == -1 && rect.w == -1 && rect.h == -1)
            scissor = this->targets[love::currentScreen].getScissor();

        GX2SetScissor(scissor.x, scissor.y, scissor.w, scissor.h);
        this->context.scissor = scissor;
    }

    void GX2::setCullMode(CullMode mode)
    {
        const auto enabled = mode != CullMode::CULL_NONE;

        this->context.cullBack  = (enabled && mode == CullMode::CULL_BACK);
        this->context.cullFront = (enabled && mode == CullMode::CULL_FRONT);

        GX2SetCullOnlyControl(this->context.winding, this->context.cullBack, this->context.cullFront);
    }

    void GX2::setVertexWinding(Winding winding)
    {
        GX2FrontFace windingMode;

        if (!GX2::getConstant(winding, windingMode))
            return;

        GX2SetCullOnlyControl(windingMode, this->context.cullBack, this->context.cullFront);
        this->context.winding = windingMode;
    }

    void GX2::setColorMask(ColorChannelMask mask)
    {
        const auto red   = (GX2_CHANNEL_MASK_R * mask.r);
        const auto green = (GX2_CHANNEL_MASK_G * mask.g);
        const auto blue  = (GX2_CHANNEL_MASK_B * mask.b);
        const auto alpha = (GX2_CHANNEL_MASK_A * mask.a);

        const auto value = GX2ChannelMask(red + green + blue + alpha);
        const auto NONE  = GX2ChannelMask(0);

        GX2SetTargetChannelMasks(value, NONE, NONE, NONE, NONE, NONE, NONE, NONE);
    }

    void GX2::setBlendState(const BlendState& state)
    {
        GX2BlendCombineMode operationRGB;
        if (!GX2::getConstant(state.operationRGB, operationRGB))
            return;

        GX2BlendCombineMode operationA;
        if (!GX2::getConstant(state.operationA, operationA))
            return;

        GX2BlendMode sourceColor;
        if (!GX2::getConstant(state.srcFactorRGB, sourceColor))
            return;

        GX2BlendMode destColor;
        if (!GX2::getConstant(state.dstFactorRGB, destColor))
            return;

        GX2BlendMode sourceAlpha;
        if (!GX2::getConstant(state.srcFactorA, sourceAlpha))
            return;

        GX2BlendMode destAlpha;
        if (!GX2::getConstant(state.dstFactorA, destAlpha))
            return;

        GX2SetBlendControl(GX2_RENDER_TARGET_0, sourceColor, destColor, operationRGB, true, sourceAlpha,
                           destAlpha, operationA);
    }

    void GX2::showFallbackDiagnosticScreen()
    {
#ifdef __WIIU__
        // Force immediate display of diagnostic message using OSScreen
        // This bypasses the normal graphics pipeline which may be stuck
        
        FILE* diagnosticLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (diagnosticLog) {
            fprintf(diagnosticLog, "=== SHOWING FALLBACK DIAGNOSTIC SCREEN ===\n");
            fprintf(diagnosticLog, "Attempting to display diagnostic message using OSScreen\n");
            fflush(diagnosticLog);
            fclose(diagnosticLog);
        }
        
        // Try to initialize screen
        OSScreenInit();
        
        // Get buffer sizes
        size_t tvBufferSize = OSScreenGetBufferSizeEx(SCREEN_TV);
        size_t drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);
        
        // Allocate buffers (simplified approach)
        void* tvBuffer = memalign(0x100, tvBufferSize);
        void* drcBuffer = memalign(0x100, drcBufferSize);
        
        if (tvBuffer && drcBuffer) {
            // Set buffers
            OSScreenSetBufferEx(SCREEN_TV, tvBuffer);
            OSScreenSetBufferEx(SCREEN_DRC, drcBuffer);
            
            // Enable both screens
            OSScreenEnableEx(SCREEN_TV, true);
            OSScreenEnableEx(SCREEN_DRC, true);
            
            // Clear screens to black
            OSScreenClearBufferEx(SCREEN_TV, 0x000000FF);
            OSScreenClearBufferEx(SCREEN_DRC, 0x000000FF);
            
            // Display diagnostic text with more detailed information
            const char* line1 = "LOVE POTION DIAGNOSTIC MODE";
            const char* line2 = "*** ENDLESS GRAPHICS LOOP DETECTED ***";
            const char* line3 = "lua_resume() was called but never returned";
            const char* line4 = "Lua VM stuck in infinite loop";
            const char* line5 = "Possible causes:";
            const char* line6 = "- Infinite loop in love.run() or love.update()";
            const char* line7 = "- Recursive function calls (stack overflow)";
            const char* line8 = "- Blocking operation in main loop";
            const char* line9 = "Check: /vol/external01/wiiu/apps/balatro/simple_debug.log";
            const char* line10 = "Press HOME to exit to Wii U menu";
            
            // Get current system time for display
            OSCalendarTime calendarTime;
            OSTicksToCalendarTime(OSGetTime(), &calendarTime);
            
            char timeStr[64];
            snprintf(timeStr, sizeof(timeStr), "Time: %02d:%02d:%02d | Present calls: %d", 
                     calendarTime.tm_hour, calendarTime.tm_min, calendarTime.tm_sec, this->consecutivePresentCalls);
            
            // Put text on TV (more detailed)
            OSScreenPutFontEx(SCREEN_TV, 5, 2, line1);
            OSScreenPutFontEx(SCREEN_TV, 5, 4, line2);
            OSScreenPutFontEx(SCREEN_TV, 5, 5, line3);
            OSScreenPutFontEx(SCREEN_TV, 5, 6, line4);
            OSScreenPutFontEx(SCREEN_TV, 5, 8, line5);
            OSScreenPutFontEx(SCREEN_TV, 5, 9, line6);
            OSScreenPutFontEx(SCREEN_TV, 5, 10, line7);
            OSScreenPutFontEx(SCREEN_TV, 5, 11, line8);
            OSScreenPutFontEx(SCREEN_TV, 5, 13, line9);
            OSScreenPutFontEx(SCREEN_TV, 5, 14, timeStr);
            OSScreenPutFontEx(SCREEN_TV, 5, 16, line10);
            
            // Put text on GamePad (condensed)
            OSScreenPutFontEx(SCREEN_DRC, 2, 1, "LOVE POTION DIAGNOSTIC");
            OSScreenPutFontEx(SCREEN_DRC, 2, 3, "*** ENDLESS LOOP DETECTED ***");
            OSScreenPutFontEx(SCREEN_DRC, 2, 4, "lua_resume() never returned");
            OSScreenPutFontEx(SCREEN_DRC, 2, 5, "Lua VM stuck in infinite loop");
            OSScreenPutFontEx(SCREEN_DRC, 2, 7, "Possible causes:");
            OSScreenPutFontEx(SCREEN_DRC, 2, 8, "- Infinite loop in love.run()");
            OSScreenPutFontEx(SCREEN_DRC, 2, 9, "- Recursive function calls");
            OSScreenPutFontEx(SCREEN_DRC, 2, 10, "- Blocking operation");
            OSScreenPutFontEx(SCREEN_DRC, 2, 12, "Check: simple_debug.log");
            OSScreenPutFontEx(SCREEN_DRC, 2, 13, timeStr);
            OSScreenPutFontEx(SCREEN_DRC, 2, 15, "Press HOME to exit");
            
            // Flip buffers to display
            OSScreenFlipBuffersEx(SCREEN_TV);
            OSScreenFlipBuffersEx(SCREEN_DRC);
            
            FILE* successLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (successLog) {
                fprintf(successLog, "Fallback diagnostic screen displayed successfully\n");
                fflush(successLog);
                fclose(successLog);
            }
            
            // Keep the diagnostic screen visible and enter a simple loop
            // that doesn't hang like the graphics loop
            while (true) {
                // Simple delay to prevent 100% CPU usage
                for (volatile int i = 0; i < 1000000; i++);
                
                // Refresh the diagnostic display periodically
                OSScreenFlipBuffersEx(SCREEN_TV);
                OSScreenFlipBuffersEx(SCREEN_DRC);
            }
        } else {
            FILE* errorLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (errorLog) {
                fprintf(errorLog, "FAILED to allocate OSScreen buffers for diagnostic display\n");
                fflush(errorLog);
                fclose(errorLog);
            }
        }
#endif
    }

} // namespace love

// Global instance definition
love::GX2 love::gx2;

// Helper for instrumentation without including GX2.hpp in large compilation units
extern "C" void love_gx2IncrementIssuedDraws() {
    love::gx2.incrementIssuedDraws();
}

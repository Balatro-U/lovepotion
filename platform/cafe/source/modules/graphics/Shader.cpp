#include "common/Exception.hpp"
#include "common/config.hpp"
#include "common/screen.hpp"

#include "modules/graphics/Shader.hpp"
#include "modules/graphics/ShaderStage.hpp"

#include <gfd.h>
#include <gx2/mem.h>
#include <whb/gfx.h>
#ifdef USE_CAFEGLSL
#include "common/CafeGLSL.hpp"
#endif

#include <malloc.h>

#define SHADERS_DIR "/vol/content/shaders/"

#define DEFAULT_PRIMITIVE_SHADER (SHADERS_DIR "color.gsh")
#define DEFAULT_TEXTURE_SHADER   (SHADERS_DIR "texture.gsh")
#define DEFAULT_VIDEO_SHADER     (SHADERS_DIR "video.gsh")

namespace love
{
    Shader::Shader(StrongRef<ShaderStageBase> _stages[SHADERSTAGE_MAX_ENUM], const CompileOptions& options) :
        ShaderBase(_stages, options)
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "Shader::Shader() constructor called\n");
            fflush(logFile);
            fclose(logFile);
        }
#endif
        // Load GPU resources now and fail fast if anything is wrong.
        // Previously, a failed load would leave a half-initialized shader object
        // that could later crash when bound. Throwing here lets Lua see a proper
        // error from love.graphics.newShader instead of causing a system error.
        if (!this->loadVolatile())
        {
#ifdef __WIIU__
            FILE* logFileFail = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFileFail) {
                fprintf(logFileFail, "Shader::Shader() - loadVolatile FAILED, throwing Exception\n");
                fflush(logFileFail);
                fclose(logFileFail);
            }
#endif
            // Prefer a descriptive error if warnings were collected on stages
            std::string warn = this->getWarnings();
            if (!warn.empty())
                throw love::Exception("Failed to load shader: %s", warn.c_str());
            throw love::Exception("Failed to load shader (missing or invalid .gsh?)");
        }
#ifdef __WIIU__
        FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile2) {
            fprintf(logFile2, "Shader::Shader() loadVolatile() completed\n");
            fflush(logFile2);
            fclose(logFile2);
        }
#endif
    }

    Shader::~Shader()
    {
        // Ensure GPU resources are released
        unloadVolatile();
    }

    Shader::Shader(const WHBGfxShaderGroup& compiledGroup, const CompileOptions& options)
        : ShaderBase(nullptr, options)
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "Shader::Shader(runtimeCompiled) constructor called\n");
            fflush(logFile);
            fclose(logFile);
        }
#endif
        this->program = compiledGroup;
        this->runtimeCompiled = true;
    // Initialize attribute layout to match engine vertex format
    WHBGfxInitShaderAttribute(&this->program, "aPosition",  0, POSITION_OFFSET, GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&this->program, "aTexCoord",  0, TEXCOORD_OFFSET, GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&this->program, "aColor",     0, COLOR_OFFSET,    GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitFetchShader(&this->program);
    // Reflect
        this->mapActiveUniforms();
    }

    const char* Shader::getDefaultStagePath(StandardShader shader, ShaderStageType stage)
    {
        // For Wii U, .gsh files contain both vertex and pixel shaders
        // Use the same file for both stages - the loading system will extract the correct stage
        switch (shader)
        {
            case STANDARD_DEFAULT:
            default:
                return DEFAULT_PRIMITIVE_SHADER;
            case STANDARD_TEXTURE:
                return DEFAULT_TEXTURE_SHADER;
            case STANDARD_VIDEO:
                return DEFAULT_VIDEO_SHADER;
        }
    }

    void Shader::mapActiveUniforms()
    {
        const auto uniformBlockCount = this->program.vertexShader->uniformBlockCount;

        for (size_t index = 0; index < uniformBlockCount; index++)
        {
            const auto uniform = this->program.vertexShader->uniformBlocks[index];

            this->reflection.uniforms.insert_or_assign(
                uniform.name, new UniformInfo {
                                  .type      = UNIFORM_MATRIX,
                                  .stageMask = ShaderStageMask::SHADERSTAGEMASK_VERTEX,
                                  .active    = true,
                                  .location  = uniform.offset,
                                  .count     = 1,
                                  .name      = uniform.name,
                              });
#ifdef __WIIU__
            FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile) {
                fprintf(logFile, "Shader::mapActiveUniforms() - VERTEX uniform block: '%s' at offset %u\n", uniform.name, uniform.offset);
                fflush(logFile);
                fclose(logFile);
            }
#endif
        }

        const auto samplerCount = this->program.pixelShader->samplerVarCount;

        for (size_t index = 0; index < samplerCount; index++)
        {
            const auto sampler = this->program.pixelShader->samplerVars[index];

            this->reflection.uniforms.insert_or_assign(
                sampler.name, new UniformInfo {
                                  .type      = UNIFORM_SAMPLER,
                                  .stageMask = ShaderStageMask::SHADERSTAGEMASK_PIXEL,
                                  .active    = true,
                                  .location  = sampler.location,
                                  .count     = 1,
                                  .name      = sampler.name,
                              });
#ifdef __WIIU__
            FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile2) {
                fprintf(logFile2, "Shader::mapActiveUniforms() - PIXEL sampler: '%s' at location %u\n", sampler.name, sampler.location);
                fflush(logFile2);
                fclose(logFile2);
            }
#endif
        }
    }

    bool Shader::setShaderStages(WHBGfxShaderGroup* group, std::array<StrongRef<ShaderStageBase>, 2> stages)
    {
        std::memset(group, 0, sizeof(WHBGfxShaderGroup));

        if (this->hasStage(ShaderStageType::SHADERSTAGE_VERTEX))
            group->vertexShader = (GX2VertexShader*)stages[SHADERSTAGE_VERTEX]->getHandle();

        if (this->hasStage(ShaderStageType::SHADERSTAGE_PIXEL))
            group->pixelShader = (GX2PixelShader*)stages[SHADERSTAGE_PIXEL]->getHandle();

        if (!this->program.vertexShader || !this->program.pixelShader)
            return false;

        return true;
    }

    bool Shader::loadVolatile()
    {
#ifdef __WIIU__
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "Shader::loadVolatile() starting\n");
            fflush(logFile);
            fclose(logFile);
        }
#endif
        
        for (const auto& stage : this->stages)
        {
            if (stage.get() != nullptr)
            {
#ifdef __WIIU__
                FILE* logFile2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (logFile2) {
                    fprintf(logFile2, "Shader::loadVolatile() - loading stage %p\n", stage.get());
                    fflush(logFile2);
                    fclose(logFile2);
                }
#endif
                bool stageLoadResult = ((ShaderStage*)stage.get())->loadVolatile();
#ifdef __WIIU__
                FILE* logFile2b = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (logFile2b) {
                    fprintf(logFile2b, "Shader::loadVolatile() - stage %p loadVolatile() returned: %s\n", 
                            stage.get(), stageLoadResult ? "SUCCESS" : "FAILURE");
                    fflush(logFile2b);
                    fclose(logFile2b);
                }
#endif
                if (!stageLoadResult) {
#ifdef __WIIU__
                    FILE* logFile2c = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                    if (logFile2c) {
                        fprintf(logFile2c, "Shader::loadVolatile() - STAGE LOAD FAILED! Aborting shader load.\n");
                        fflush(logFile2c);
                        fclose(logFile2c);
                    }
#endif
                    return false;
                }
            }
        }

        if (!this->setShaderStages(&this->program, this->stages))
        {
#ifdef __WIIU__
            FILE* logFile3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile3) {
                fprintf(logFile3, "Shader::loadVolatile() - setShaderStages() failed\n");
                fflush(logFile3);
                fclose(logFile3);
            }
#endif
            return false;  // Changed from true to false - this should be an error!
        }

#ifdef __WIIU__
        FILE* logFile3b = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile3b) {
            fprintf(logFile3b, "Shader::loadVolatile() - setShaderStages() succeeded, calling mapActiveUniforms()\n");
            fflush(logFile3b);
            fclose(logFile3b);
        }
#endif

        this->mapActiveUniforms();

#ifdef __WIIU__
        FILE* logFile3c = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile3c) {
            fprintf(logFile3c, "Shader::loadVolatile() - mapActiveUniforms() completed, initializing shader attributes\n");
            fflush(logFile3c);
            fclose(logFile3c);
        }
#endif

    // clang-format off
    // Dynamically bind attributes based on what's actually present in the compiled shader.
    // Some shader toolchains may rename attributes (e.g. position -> aPosition / vPosition / inPosition / position0).
    // We'll scan the vertex shader's attribVars and choose the first match from a list of candidates.
    const char* positionCandidates[] = { "aPosition", "position", "vPosition", "inPosition", "inPos", "position0", nullptr };
    const char* texcoordCandidates[] = { "aTexCoord", "texCoord", "vTexCoord", "inTexCoord", "uv0", "texcoord0", nullptr };
    const char* colorCandidates[]    = { "aColor", "vColor", "inColor", "color", "color0", nullptr };

    auto findAttributeName = [&](const char** candidates) -> const char* {
        if (!this->program.vertexShader)
            return nullptr;
        for (int i = 0; i < (int)this->program.vertexShader->attribVarCount; i++) {
            const auto& var = this->program.vertexShader->attribVars[i];
            for (int c = 0; candidates[c]; c++) {
                if (strcmp(var.name, candidates[c]) == 0)
                    return candidates[c];
            }
        }
        return nullptr; // Not found
    };

    const char* posName = findAttributeName(positionCandidates);
    const char* texName = findAttributeName(texcoordCandidates);
    const char* colName = findAttributeName(colorCandidates);

#ifdef __WIIU__
    FILE* attrLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (attrLog) {
        fprintf(attrLog, "Shader::loadVolatile() attribute introspection: attribVarCount=%u\n",
                this->program.vertexShader ? this->program.vertexShader->attribVarCount : 0);
        if (this->program.vertexShader) {
            for (uint32_t i = 0; i < this->program.vertexShader->attribVarCount; i++) {
                const auto& var = this->program.vertexShader->attribVars[i];
                fprintf(attrLog, "  VS Attrib[%u]: name='%s' type=%u location=%u\n", i, var.name, var.type, var.location);
            }
        }
        fprintf(attrLog, "  Chosen attribute names -> position='%s' texcoord='%s' color='%s' (NULL means fallback)\n",
                posName ? posName : "<default aPosition>", texName ? texName : "<default aTexCoord>", colName ? colName : "<default aColor>");
        fflush(attrLog);
        fclose(attrLog);
    }
#endif

    // Fall back to canonical names if not found.
    bool hadInPos = false;
    if (!posName) {
        // Detect if 'inPos' actually existed but wasn't matched earlier (shouldn't happen now but safety)
        if (this->program.vertexShader) {
            for (uint32_t i = 0; i < this->program.vertexShader->attribVarCount; i++) {
                if (strcmp(this->program.vertexShader->attribVars[i].name, "inPos") == 0) {
                    hadInPos = true; break;
                }
            }
        }
        posName = "aPosition";
    }
    if (!texName) texName = "aTexCoord";
    if (!colName) colName = "aColor";

    WHBGfxInitShaderAttribute(&this->program, posName, 0, POSITION_OFFSET, GX2_ATTRIB_FORMAT_FLOAT_32_32);
#ifdef __WIIU__
    if (hadInPos) {
        FILE* warnLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (warnLog) { fprintf(warnLog, "WARNING: Shader::loadVolatile() used fallback aPosition while 'inPos' existed. Potential attribute mismatch earlier.\n"); fclose(warnLog);} 
    }
#endif
    WHBGfxInitShaderAttribute(&this->program, texName, 0, TEXCOORD_OFFSET, GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&this->program, colName, 0, COLOR_OFFSET,    GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    // clang-format on

#ifdef __WIIU__
        FILE* logFile3d = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile3d) {
            fprintf(logFile3d, "Shader::loadVolatile() - shader attributes initialized, calling WHBGfxInitFetchShader()\n");
            fflush(logFile3d);
            fclose(logFile3d);
        }
#endif

        if (!WHBGfxInitFetchShader(&this->program))
        {
#ifdef __WIIU__
            FILE* logFile4 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile4) {
                fprintf(logFile4, "Shader::loadVolatile() - WHBGfxInitFetchShader() failed\n");
                fflush(logFile4);
                fclose(logFile4);
            }
#endif
            return false;
        }

#ifdef __WIIU__
        FILE* logFile4b = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile4b) {
            fprintf(logFile4b, "Shader::loadVolatile() - WHBGfxInitFetchShader() succeeded\n");
            fflush(logFile4b);
            fclose(logFile4b);
        }
#endif

#ifdef __WIIU__
        FILE* logFile5 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile5) {
            fprintf(logFile5, "Shader::loadVolatile() completed successfully\n");
            fflush(logFile5);
            fclose(logFile5);
        }
#endif

        return true;
    }

    void Shader::unloadVolatile()
    {
        // Free fetch/group resources
        WHBGfxFreeShaderGroup(&this->program);

#ifdef USE_CAFEGLSL
        // Additionally free CafeGLSL-compiled shaders explicitly
        if (this->runtimeCompiled)
        {
            if (this->program.vertexShader)
                CafeGLSLCompiler::FreeVertexShader(this->program.vertexShader);
            if (this->program.pixelShader)
                CafeGLSLCompiler::FreePixelShader(this->program.pixelShader);
            this->program.vertexShader = nullptr;
            this->program.pixelShader = nullptr;
        }
#endif

        for (auto& it : this->reflection.uniforms)
            delete it.second;
    }

    std::string Shader::getWarnings() const
    {
        std::string warnings {};
        std::string_view stageString;

        for (const auto& stage : this->stages)
        {
            if (stage.get() == nullptr)
                continue;

            const std::string& _warnings = stage->getWarnings();
            if (!_warnings.empty() && ShaderStage::getConstant(stage->getStageType(), stageString))
                warnings += std::format("{} shader:\n{}", stageString, _warnings);
        }

        return warnings;
    }

    void Shader::updateBuiltinUniforms(GraphicsBase* graphics, Uniform* uniform)
    {
        if (current != this)
            return;

        const ShaderBase::UniformInfo* uniformBlock = this->getUniformInfo("Transformation");
        if (!uniformBlock) {
            static const char* altNames[] = { "uMVP", "uTransform", "uProjection", "uModelView" };
            for (const char* name : altNames) {
                uniformBlock = this->getUniformInfo(name);
                if (uniformBlock) break;
            }
        }
        if (!uniformBlock) {
            for (const auto& it : this->reflection.uniforms) {
                const auto* info = it.second;
                if (info && info->active && info->type == UNIFORM_MATRIX &&
                    (info->stageMask & ShaderStageMask::SHADERSTAGEMASK_VERTEX)) {
                    uniformBlock = info;
                    break;
                }
            }
        }
        if (!uniformBlock)
            return;

        GX2Invalidate(INVALIDATE_UNIFORM_BLOCK, uniform, UNIFORM_SIZE);
        GX2SetVertexUniformBlock(uniformBlock->location, UNIFORM_SIZE, uniform);
    }

    ptrdiff_t Shader::getHandle() const
    {
        return 0;
    }

    void Shader::attach()
    {
        // Safety: if this shader is not valid, gracefully fallback to default
        // instead of binding null pointers which can crash the system.
        if (!this->program.vertexShader || !this->program.pixelShader)
        {
#ifdef __WIIU__
            FILE* logFileInvalid = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFileInvalid) {
                fprintf(logFileInvalid, "Shader::attach() - INVALID shader program detected (vs=%p, ps=%p). Fallback to STANDARD_DEFAULT.\n",
                        this->program.vertexShader, this->program.pixelShader);
                fflush(logFileInvalid);
                fclose(logFileInvalid);
            }
#endif
            if (standardShaders[STANDARD_DEFAULT])
            {
                standardShaders[STANDARD_DEFAULT]->attach();
                return;
            }
            // If default shader is not available for some reason, do nothing.
            return;
        }

        if (current != this)
        {
#ifdef __WIIU__
            static int attachCount = 0;
            attachCount++;
            
            FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile) {
                fprintf(logFile, "Shader::attach() #%d: Switching to shader %p (was %p)\n", 
                        attachCount, this, current);
                        
                // Log which standard shader this is
                const char* shaderTypeName = "unknown";
                for (int i = 0; i < STANDARD_MAX_ENUM; i++) {
                    if (this == standardShaders[i]) {
                        switch(i) {
                            case STANDARD_DEFAULT: shaderTypeName = "STANDARD_DEFAULT"; break;
                            case STANDARD_TEXTURE: shaderTypeName = "STANDARD_TEXTURE"; break;
                            case STANDARD_VIDEO: shaderTypeName = "STANDARD_VIDEO"; break;
                        }
                        break;
                    }
                }
                fprintf(logFile, "  Shader type: %s\n", shaderTypeName);
                fprintf(logFile, "  Vertex shader: %p, Pixel shader: %p\n", 
                        this->program.vertexShader, this->program.pixelShader);
                
                fflush(logFile);
                fclose(logFile);
            }
#endif

            Graphics::flushBatchedDrawsGlobal();

            GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);

            GX2SetFetchShader(&this->program.fetchShader);
            GX2SetVertexShader(this->program.vertexShader);
            GX2SetPixelShader(this->program.pixelShader);

#ifdef __WIIU__
            // Experimental: If a special file flag exists, we later will attempt a flat color mode.
            // (Implemented elsewhere by swapping pixel shader if needed.) For now just log hook point.
            static bool loggedOnce = false;
            if (!loggedOnce) {
                FILE* flatLog = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (flatLog) {
                    fprintf(flatLog, "Shader::attach() - normal shaders bound (vs=%p ps=%p)\n", this->program.vertexShader, this->program.pixelShader);
                    fclose(flatLog);
                }
                loggedOnce = true;
            }
#endif

            current = this;
            shaderSwitches++;
        }
#ifdef __WIIU__
        else {
            static int skipCount = 0;
            skipCount++;
            if (skipCount <= 5 || skipCount % 100 == 0) {
                FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
                if (logFile) {
                    fprintf(logFile, "Shader::attach() skipped: already current (skip #%d)\n", skipCount);
                    fflush(logFile);
                    fclose(logFile);
                }
            }
        }
#endif
    }
} // namespace love

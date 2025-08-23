#include "common/Exception.hpp"
#include "common/config.hpp"
#include "common/screen.hpp"

#include "modules/graphics/Shader.hpp"
#include "modules/graphics/ShaderStage.hpp"

#include <gfd.h>
#include <gx2/mem.h>
#include <whb/gfx.h>

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
#if defined(__WIIU__) && (__DEBUG__)
        FILE* logFile = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "Shader::Shader() constructor called\n");
            fflush(logFile);
            fclose(logFile);
        }
#endif
        this->loadVolatile();
#if defined(__WIIU__) && (__DEBUG__)
        FILE* logFile2 = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile2) {
            fprintf(logFile2, "Shader::Shader() loadVolatile() completed\n");
            fflush(logFile2);
            fclose(logFile2);
        }
#endif
    }

    Shader::~Shader()
    {
        this->unloadVolatile();
    }

    const char* Shader::getDefaultStagePath(StandardShader shader, ShaderStageType stage)
    {
        LOVE_UNUSED(stage);

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
#if defined(__WIIU__) && (__DEBUG__)
        FILE* logFile = fopen("fs:/vol/external01/simple_debug.log", "a");
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
#if defined(__WIIU__) && (__DEBUG__)
                FILE* logFile2 = fopen("fs:/vol/external01/simple_debug.log", "a");
                if (logFile2) {
                    fprintf(logFile2, "Shader::loadVolatile() - loading stage %p\n", stage.get());
                    fflush(logFile2);
                    fclose(logFile2);
                }
#endif
                bool stageLoadResult = ((ShaderStage*)stage.get())->loadVolatile();
#if defined(__WIIU__) && (__DEBUG__)
                FILE* logFile2b = fopen("fs:/vol/external01/simple_debug.log", "a");
                if (logFile2b) {
                    fprintf(logFile2b, "Shader::loadVolatile() - stage %p loadVolatile() returned: %s\n", 
                            stage.get(), stageLoadResult ? "SUCCESS" : "FAILURE");
                    fflush(logFile2b);
                    fclose(logFile2b);
                }
#endif
                if (!stageLoadResult) {
#if defined(__WIIU__) && (__DEBUG__)
                    FILE* logFile2c = fopen("fs:/vol/external01/simple_debug.log", "a");
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
            FILE* logFile3 = fopen("fs:/vol/external01/simple_debug.log", "a");
            if (logFile3) {
                fprintf(logFile3, "Shader::loadVolatile() - setShaderStages() failed\n");
                fflush(logFile3);
                fclose(logFile3);
            }
#endif
            return false;  // Changed from true to false - this should be an error!
        }

#ifdef __WIIU__
        FILE* logFile3b = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile3b) {
            fprintf(logFile3b, "Shader::loadVolatile() - setShaderStages() succeeded, calling mapActiveUniforms()\n");
            fflush(logFile3b);
            fclose(logFile3b);
        }
#endif

        this->mapActiveUniforms();

#ifdef __WIIU__
        FILE* logFile3c = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile3c) {
            fprintf(logFile3c, "Shader::loadVolatile() - mapActiveUniforms() completed, initializing shader attributes\n");
            fflush(logFile3c);
            fclose(logFile3c);
        }
#endif

        // clang-format off
        WHBGfxInitShaderAttribute(&this->program, "inPos",      0, POSITION_OFFSET, GX2_ATTRIB_FORMAT_FLOAT_32_32);
        WHBGfxInitShaderAttribute(&this->program, "inTexCoord", 0, TEXCOORD_OFFSET, GX2_ATTRIB_FORMAT_FLOAT_32_32);
        WHBGfxInitShaderAttribute(&this->program, "inColor",    0, COLOR_OFFSET,    GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
        // clang-format on

#ifdef __WIIU__
        FILE* logFile3d = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile3d) {
            fprintf(logFile3d, "Shader::loadVolatile() - shader attributes initialized, calling WHBGfxInitFetchShader()\n");
            fflush(logFile3d);
            fclose(logFile3d);
        }
#endif

        if (!WHBGfxInitFetchShader(&this->program))
        {
#ifdef __WIIU__
            FILE* logFile4 = fopen("fs:/vol/external01/simple_debug.log", "a");
            if (logFile4) {
                fprintf(logFile4, "Shader::loadVolatile() - WHBGfxInitFetchShader() failed\n");
                fflush(logFile4);
                fclose(logFile4);
            }
#endif
            return false;
        }

#ifdef __WIIU__
        FILE* logFile4b = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile4b) {
            fprintf(logFile4b, "Shader::loadVolatile() - WHBGfxInitFetchShader() succeeded\n");
            fflush(logFile4b);
            fclose(logFile4b);
        }
#endif

#ifdef __WIIU__
        FILE* logFile5 = fopen("fs:/vol/external01/simple_debug.log", "a");
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
        WHBGfxFreeShaderGroup(&this->program);

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

        auto& transform = graphics->getTransform();
        // uniform->update(transform);

        auto* uniformBlock = this->getUniformInfo("Transformation");

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
        if (current != this)
        {
#ifdef __WIIU__
            static int attachCount = 0;
            attachCount++;
            
            FILE* logFile = fopen("fs:/vol/external01/simple_debug.log", "a");
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

            current = this;
            shaderSwitches++;
        }
#ifdef __WIIU__
        else {
            static int skipCount = 0;
            skipCount++;
            if (skipCount <= 5 || skipCount % 100 == 0) {
                FILE* logFile = fopen("fs:/vol/external01/simple_debug.log", "a");
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

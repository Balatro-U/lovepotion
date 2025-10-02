#include "common/Exception.hpp"

#include "modules/graphics/Shader.tcc"

namespace love
{
    Type ShaderBase::type("Shader", &Object::type);

    ShaderBase* ShaderBase::current                            = nullptr;
    ShaderBase* ShaderBase::standardShaders[STANDARD_MAX_ENUM] = { nullptr };

    int ShaderBase::shaderSwitches = 0;

    ShaderBase::ShaderBase(StrongRef<ShaderStageBase> _stages[], const CompileOptions& options) :
        stages(),
        debugName(options.debugName)
    {
        for (int i = 0; i < SHADERSTAGE_MAX_ENUM; i++)
            this->stages[i] = _stages[i];
    }

    ShaderBase::~ShaderBase()
    {
        for (int index = 0; index < STANDARD_MAX_ENUM; index++)
        {
            if (this == standardShaders[index])
                standardShaders[index] = nullptr;
        }

        if (current == this)
            this->attachDefault(STANDARD_DEFAULT);
    }

    bool ShaderBase::hasStage(ShaderStageType stage)
    {
        return this->stages[stage] != nullptr;
    }

    void ShaderBase::attachDefault(StandardShader type)
    {
#ifdef __WIIU__
        static int attachDefaultCount = 0;
        attachDefaultCount++;
        
        FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile) {
            const char* typeName = "unknown";
            switch(type) {
                case STANDARD_DEFAULT: typeName = "STANDARD_DEFAULT"; break;
                case STANDARD_TEXTURE: typeName = "STANDARD_TEXTURE"; break;
                case STANDARD_VIDEO: typeName = "STANDARD_VIDEO"; break;
            }
            fprintf(logFile, "ShaderBase::attachDefault() #%d: type=%s\n", attachDefaultCount, typeName);
            fflush(logFile);
            fclose(logFile);
        }
#endif

        auto* defaultShader = standardShaders[type];

        if (defaultShader == nullptr)
        {
#ifdef __WIIU__
            FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile) {
                fprintf(logFile, "  WARNING: standardShaders[%d] is NULL, shaders not ready yet\n", type);
                fflush(logFile);
                fclose(logFile);
            }
#endif
            // Don't set current to nullptr, just return early
            // Shaders will be attached when they're ready
            return;
        }

        if (current != defaultShader)
        {
#ifdef __WIIU__
            FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile) {
                fprintf(logFile, "  Calling attach() on shader %p\n", defaultShader);
                fflush(logFile);
                fclose(logFile);
            }
#endif
            defaultShader->attach();
        }
#ifdef __WIIU__
        else {
            FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile) {
                fprintf(logFile, "  Shader already current, no switch needed\n");
                fflush(logFile);
                fclose(logFile);
            }
        }
#endif
    }

    const ShaderBase::UniformInfo* ShaderBase::getUniformInfo(const std::string& name) const
    {
        const auto it = this->reflection.uniforms.find(name);
        return it != this->reflection.uniforms.end() ? it->second : nullptr;
    }

    bool ShaderBase::hasUniform(const std::string& name) const
    {
        const auto it = this->reflection.uniforms.find(name);
        return it != this->reflection.uniforms.end() && it->second->active;
    }

    bool ShaderBase::isDefaultActive()
    {
        for (int index = 0; index < STANDARD_MAX_ENUM; index++)
        {
            if (current == standardShaders[index])
                return true;
        }

        return false;
    }
} // namespace love

#include "common/Exception.hpp"

#include "modules/graphics/ShaderStage.hpp"

#include <memory>

namespace love
{
    ShaderStage::ShaderStage(ShaderStageType stage, const std::string& filepath) :
        ShaderStageBase(stage, filepath)
    {
        this->loadVolatile();
    }

    ShaderStage::~ShaderStage()
    {
        this->unloadVolatile();
    }

    bool ShaderStage::loadVolatile()
    {
#if defined(__WIIU__) && (__DEBUG__)
        FILE* logFile = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile) {
            fprintf(logFile, "ShaderStage::loadVolatile() starting - file: %s\n", this->filepath.c_str());
            fflush(logFile);
            fclose(logFile);
        }
#endif

        std::FILE* file = std::fopen(this->filepath.c_str(), "rb");

        if (!file)
        {
#if defined(__WIIU__) && (__DEBUG__)
            FILE* logFile2 = fopen("fs:/vol/external01/simple_debug.log", "a");
            if (logFile2) {
                fprintf(logFile2, "ShaderStage::loadVolatile() - Failed to open file: %s\n", this->filepath.c_str());
                fflush(logFile2);
                fclose(logFile2);
            }
#endif
            this->warnings.append(std::format("Failed to open file {:s}", this->filepath.c_str()));
            return false;
        }

#if defined(__WIIU__) && (__DEBUG__)
        FILE* logFile3 = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile3) {
            fprintf(logFile3, "ShaderStage::loadVolatile() - file opened successfully\n");
            fflush(logFile3);
            fclose(logFile3);
        }
#endif

        std::fseek(file, 0, SEEK_END);
        long size = std::ftell(file);
        std::rewind(file);

#if defined(__WIIU__) && (__DEBUG__)
        FILE* logFile4 = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile4) {
            fprintf(logFile4, "ShaderStage::loadVolatile() - file size: %ld bytes\n", size);
            fflush(logFile4);
            fclose(logFile4);
        }
#endif

        if (size <= 0)
        {
#if defined(__WIIU__) && (__DEBUG__)
            FILE* logFile5 = fopen("fs:/vol/external01/simple_debug.log", "a");
            if (logFile5) {
                fprintf(logFile5, "ShaderStage::loadVolatile() - Invalid file size: %ld\n", size);
                fflush(logFile5);
                fclose(logFile5);
            }
#endif
            this->warnings.append("Invalid file size.");
            std::fclose(file);
            return false;
        }

#if defined(__WIIU__) && (__DEBUG__)
        FILE* logFile6 = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile6) {
            fprintf(logFile6, "ShaderStage::loadVolatile() - about to resize code vector to %ld bytes\n", size);
            fflush(logFile6);
            fclose(logFile6);
        }
#endif

        try
        {
            this->code.resize(size);
#if defined(__WIIU__) && (__DEBUG__)
            FILE* logFile7 = fopen("fs:/vol/external01/simple_debug.log", "a");
            if (logFile7) {
                fprintf(logFile7, "ShaderStage::loadVolatile() - code vector resized successfully\n");
                fflush(logFile7);
                fclose(logFile7);
            }
#endif
        }
        catch (std::bad_alloc&)
        {
#if defined(__WIIU__) && (__DEBUG__)
            FILE* logFile8 = fopen("fs:/vol/external01/simple_debug.log", "a");
            if (logFile8) {
                fprintf(logFile8, "ShaderStage::loadVolatile() - OUT OF MEMORY during resize!\n");
                fflush(logFile8);
                fclose(logFile8);
            }
#endif
            std::fclose(file);
            this->warnings.append(E_OUT_OF_MEMORY);
            return false;
        }

        size_t read = std::fread(this->code.data(), size, 1, file);

#if defined(__WIIU__) && (__DEBUG__)
        FILE* logFile9 = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile9) {
            fprintf(logFile9, "ShaderStage::loadVolatile() - fread completed, read %zu items (expected 1)\n", read);
            fflush(logFile9);
            fclose(logFile9);
        }
#endif

        if (read < 1)
        {
#if defined(__WIIU__) && (__DEBUG__)
            FILE* logFile10 = fopen("fs:/vol/external01/simple_debug.log", "a");
            if (logFile10) {
                fprintf(logFile10, "ShaderStage::loadVolatile() - FREAD FAILED! read=%zu, expected=1\n", read);
                fflush(logFile10);
                fclose(logFile10);
            }
#endif
            this->warnings.append(std::format("Failed to read file '{}'.", this->filepath.c_str()));
            std::fclose(file);
            return false;
        }

        std::fclose(file);

#if defined(__WIIU__) && (__DEBUG__)
        FILE* logFile11 = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile11) {
            fprintf(logFile11, "ShaderStage::loadVolatile() - file closed, about to check shader stage type\n");
            fflush(logFile11);
            fclose(logFile11);
        }
#endif

        if (this->getStageType() == SHADERSTAGE_VERTEX)
        {
#if defined(__WIIU__) && (__DEBUG__)
            FILE* logFile12 = fopen("fs:/vol/external01/simple_debug.log", "a");
            if (logFile12) {
                fprintf(logFile12, "ShaderStage::loadVolatile() - Loading VERTEX shader, about to call WHBGfxLoadGFDVertexShader\n");
                fflush(logFile12);
                fclose(logFile12);
            }
#endif
            this->vertex = WHBGfxLoadGFDVertexShader(0, this->code.data());

#if defined(__WIIU__) && (__DEBUG__)
            FILE* logFile13 = fopen("fs:/vol/external01/simple_debug.log", "a");
            if (logFile13) {
                fprintf(logFile13, "ShaderStage::loadVolatile() - WHBGfxLoadGFDVertexShader returned: %p\n", this->vertex);
                fflush(logFile13);
                fclose(logFile13);
            }
#endif

            if (!this->vertex)
            {
#if defined(__WIIU__) && (__DEBUG__)
                FILE* logFile14 = fopen("fs:/vol/external01/simple_debug.log", "a");
                if (logFile14) {
                    fprintf(logFile14, "ShaderStage::loadVolatile() - VERTEX SHADER LOAD FAILED!\n");
                    fflush(logFile14);
                    fclose(logFile14);
                }
#endif
                this->warnings.append("Failed to load Vertex Shader.");
                return false;
            }
        }

        if (this->getStageType() == SHADERSTAGE_PIXEL)
        {
#if defined(__WIIU__) && (__DEBUG__)
            FILE* logFile15 = fopen("fs:/vol/external01/simple_debug.log", "a");
            if (logFile15) {
                fprintf(logFile15, "ShaderStage::loadVolatile() - Loading PIXEL shader, about to call WHBGfxLoadGFDPixelShader\n");
                fflush(logFile15);
                fclose(logFile15);
            }
#endif
            this->pixel = WHBGfxLoadGFDPixelShader(0, this->code.data());

#if defined(__WIIU__) && (__DEBUG__)
            FILE* logFile16 = fopen("fs:/vol/external01/simple_debug.log", "a");
            if (logFile16) {
                fprintf(logFile16, "ShaderStage::loadVolatile() - WHBGfxLoadGFDPixelShader returned: %p\n", this->pixel);
                fflush(logFile16);
                fclose(logFile16);
            }
#endif

            if (!this->pixel)
            {
#if defined(__WIIU__) && (__DEBUG__)
                FILE* logFile17 = fopen("fs:/vol/external01/simple_debug.log", "a");
                if (logFile17) {
                    fprintf(logFile17, "ShaderStage::loadVolatile() - PIXEL SHADER LOAD FAILED!\n");
                    fflush(logFile17);
                    fclose(logFile17);
                }
#endif
                this->warnings.append("Failed to load Pixel Shader.");
                return false;
            }
        }

#if defined(__WIIU__) && (__DEBUG__)
        FILE* logFile18 = fopen("fs:/vol/external01/simple_debug.log", "a");
        if (logFile18) {
            fprintf(logFile18, "ShaderStage::loadVolatile() - COMPLETED SUCCESSFULLY\n");
            fflush(logFile18);
            fclose(logFile18);
        }
#endif

        return true;
    }

    void ShaderStage::unloadVolatile()
    {}

    ptrdiff_t ShaderStage::getHandle() const
    {
        if (this->getStageType() == SHADERSTAGE_VERTEX)
            return (ptrdiff_t)this->vertex;

        return (ptrdiff_t)this->pixel;
    }
} // namespace love

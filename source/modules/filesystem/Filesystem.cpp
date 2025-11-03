#include "modules/filesystem/Filesystem.tcc"
#include "patch/PatchEngine.hpp"
#include <sys/stat.h>

// Portable fallbacks in case platform headers don't define these macros
#ifndef S_ISREG
# ifdef _S_IFREG
#  define S_ISREG(m) (((m) & _S_IFREG) == _S_IFREG)
# elif defined(S_IFREG)
#  define S_ISREG(m) (((m) & S_IFREG) == S_IFREG)
# else
#  define S_ISREG(m) (0)
# endif
#endif
#ifndef S_ISDIR
# ifdef _S_IFDIR
#  define S_ISDIR(m) (((m) & _S_IFDIR) == _S_IFDIR)
# elif defined(S_IFDIR)
#  define S_ISDIR(m) (((m) & S_IFDIR) == S_IFDIR)
# else
#  define S_ISDIR(m) (0)
# endif
#endif
#ifndef S_ISLNK
# ifdef _S_IFLNK
#  define S_ISLNK(m) (((m) & _S_IFLNK) == _S_IFLNK)
# elif defined(S_IFLNK)
#  define S_ISLNK(m) (((m) & S_IFLNK) == S_IFLNK)
# else
#  define S_ISLNK(m) (0)
# endif
#endif

namespace love
{
    bool FilesystemBase::isRealDirectory(const std::string& path) const
    {
        FileType type = FILETYPE_MAX_ENUM;
        if (!this->getRealPathType(path, type))
            return false;

        return type == FILETYPE_DIRECTORY;
    }

    bool FilesystemBase::getRealPathType(const std::string& path, FileType& type) const
    {
        struct stat buffer;
        if (stat(path.c_str(), &buffer) != 0)
            return false;

        if (S_ISREG(buffer.st_mode))
            type = FILETYPE_FILE;
        else if (S_ISDIR(buffer.st_mode))
            type = FILETYPE_DIRECTORY;
        else if (S_ISLNK(buffer.st_mode))
            type = FILETYPE_SYMLINK;
        else
            type = FILETYPE_OTHER;

        return true;
    }

    bool FilesystemBase::createRealDirectory(const std::string& path)
    {
        FileType type = FILETYPE_MAX_ENUM;
        if (this->getRealPathType(path, type))
            return type == FILETYPE_DIRECTORY;

        std::vector<std::string> createPaths = { path };

        while (true)
        {
            std::string subPath {};
            if (!getContainingDirectory(createPaths[0], subPath))
                break;

            if (this->isRealDirectory(subPath))
                break;

            createPaths.insert(createPaths.begin(), subPath);
        }

        for (const std::string& createPath : createPaths)
        {
            if (!createDirectoryRaw(createPath))
                return false;
        }

        return true;
    }

    FileData* FilesystemBase::newFileData(const void* data, size_t size, const std::string& filename) const
    {
        // If the file is a Lua script, allow the patch engine to override it
        std::string ext;
        auto pos = filename.rfind('.');
        if (pos != std::string::npos)
            ext = filename.substr(pos + 1);

        if (ext == "lua")
        {
            std::string original((const char*)data, size);
            std::string patched = PatchEngine::apply(filename, original);
            FileData* fileData = new FileData(patched.size(), filename);
            std::memcpy(fileData->getData(), patched.data(), patched.size());
            return fileData;
        }

        FileData* fileData = new FileData(size, filename);
        std::memcpy(fileData->getData(), data, size);

        return fileData;
    }
} // namespace love

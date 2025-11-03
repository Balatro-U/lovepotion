#pragma once

#include <string>

namespace love {

class PatchEngine
{
public:
    // Apply patches (prepending, override, appending). Returns patched content.
    static std::string apply(const std::string& path, const std::string& original);
};

} // namespace love

#include "patch/PatchEngine.hpp"
#include "patch/ModManager.hpp"

#include <fstream>
#include <sstream>
#include <regex>
#include "patch/Logger.hpp"

static std::string applyMatchIndent(const std::string& payload, const std::string& indent)
{
    std::istringstream ss(payload);
    std::string line;
    std::ostringstream out;
    bool first = true;
    while (std::getline(ss, line))
    {
        if (!first)
            out << "\n";
        out << indent << line;
        first = false;
    }
    return out.str();
}

namespace love {

std::string PatchEngine::apply(const std::string& path, const std::string& original)
{
    auto& mm = ModManager::get();
    mm.scan();

    // Initialize logger to overlay dir if available
    try {
        std::string overlay = mm.getOverlayDir();
        if (!overlay.empty())
            Logger::init(overlay);
    } catch (...) {}

    Logger::info(std::string("PatchEngine: applying patches for: ") + path);

    // Collect patches for this target and sort by priority
    std::vector<Patch> applicable;
    for (const auto& p : mm.getAllPatches())
    {
        if (p.target == path)
            applicable.push_back(p);
    }

    std::sort(applicable.begin(), applicable.end(), [](const Patch& a, const Patch& b){ return a.priority < b.priority; });

    std::string result = original;

    for (const auto& p : applicable)
    {
        Logger::info(std::string("Applying patch type=") + std::to_string(p.type) + " target=" + p.target);
        if (p.type == Patch::OVERRIDE)
        {
            std::ifstream ifs(p.source_file, std::ios::binary);
            if (ifs)
            {
                std::ostringstream ss;
                ss << ifs.rdbuf();
                result = ss.str();
                Logger::info(std::string("Override applied for: ") + p.target + " from " + p.source_file);
            }
        }
        else if (p.type == Patch::COPY)
        {
            for (const auto& s : p.sources)
            {
                std::ifstream ifs(s, std::ios::binary);
                if (ifs)
                {
                    std::ostringstream ss;
                    ss << ifs.rdbuf();
                    if (p.position == "append")
                        result += ss.str();
                    else if (p.position == "prepend")
                        result = ss.str() + result;
                }
            }
        }
        else if (p.type == Patch::PATTERN || p.type == Patch::REGEX)
        {
            size_t times = (p.times < 0) ? SIZE_MAX : (size_t)p.times;

            if (p.type == Patch::REGEX)
            {
                try
                {
                    std::regex re(p.pattern);
                    std::string out;
                    std::sregex_iterator it(result.begin(), result.end(), re);
                    std::sregex_iterator end;
                    size_t replaced = 0;
                    size_t lastPos = 0;
                    for (; it != end && replaced < times; ++it)
                    {
                        auto match = *it;
                        out.append(result.substr(lastPos, match.position() - lastPos));

                        std::string insert = p.payload;
                        if (p.match_indent)
                        {
                            // detect indent of the matched line
                            size_t lineStart = result.rfind('\n', match.position());
                            std::string indent;
                            if (lineStart != std::string::npos)
                                indent = result.substr(lineStart+1, match.position() - lineStart - 1);
                            insert = applyMatchIndent(p.payload, indent);
                        }

                        out.append(insert);
                        lastPos = match.position() + match.length();
                        ++replaced;
                    }
                    out.append(result.substr(lastPos));
                    result.swap(out);
                }
                catch (std::regex_error& e)
                {
                    Logger::error(std::string("Regex error for pattern: ") + p.pattern + " error: " + e.what());
                }
            }
            else
            {
                // simple substring pattern
                size_t start = 0;
                while (times-- > 0)
                {
                    size_t pos = result.find(p.pattern, start);
                    if (pos == std::string::npos)
                        break;

                    std::string toInsert = p.payload;
                    if (p.match_indent)
                    {
                        // find line start
                        size_t lineStart = result.rfind('\n', pos);
                        std::string indent;
                        if (lineStart != std::string::npos)
                            indent = result.substr(lineStart+1, pos - lineStart - 1);
                        toInsert = applyMatchIndent(p.payload, indent);
                    }

                    if (p.position == "after")
                    {
                        pos += p.pattern.size();
                        result.insert(pos, toInsert);
                        start = pos + toInsert.size();
                    }
                    else if (p.position == "before")
                    {
                        result.insert(pos, toInsert);
                        start = pos + toInsert.size() + p.pattern.size();
                    }
                    else if (p.position == "at")
                    {
                        result.erase(pos, p.pattern.size());
                        result.insert(pos, toInsert);
                        start = pos + toInsert.size();
                    }
                    else
                    {
                        break;
                    }
                }
                Logger::info(std::string("Pattern/regex patch applied for: ") + p.target);
            }
        }
    }

    return result;
}

} // namespace love

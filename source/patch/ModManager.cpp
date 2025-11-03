#include "patch/ModManager.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include "patch/Logger.hpp"
#include <physfs.h>

// Very small TOML-ish parser for the subset we need (not a full TOML implementation)
static std::string readFileString(const std::string& path)
{
    std::ifstream ifs(path);
    if (!ifs)
        return {};
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

namespace love {

ModManager& ModManager::get()
{
    static ModManager inst;
    return inst;
}

ModManager::ModManager()
{
    // Initialize logger to a sensible fallback (local Mods folder). Real overlay may be set later.
    Logger::init("Mods");
}

void ModManager::scan()
{
    if (scanned)
        return;

    // Default mod directory on Wii U SD card
    std::string modsdir = "sdcard:/lovely/mods"; // primary location
    // fallback to local Mods for development
    std::string fallback = "Mods";

    try
    {
        if (!std::filesystem::exists(modsdir))
        {
            // try fallback
            if (std::filesystem::exists(fallback))
            {
                Logger::info("Mods directory not found at sdcard, using fallback: " + fallback);
                modsdir = fallback;
            }
            else
            {
                Logger::info("No mods directory found (sdcard or fallback). Skipping mod scan.");
                scanned = true;
                return;
            }
        }

        if (!std::filesystem::exists(modsdir) && std::filesystem::exists(fallback))
            modsdir = fallback;

        for (auto const& dirEntry : std::filesystem::directory_iterator(modsdir))
        {
            if (!dirEntry.is_directory())
                continue;

            // For simplicity we support a structure: Mods/ModName/lovely/*
            auto lovelydir = dirEntry.path() / "lovely";
            if (!std::filesystem::exists(lovelydir))
                continue;

            // Mount this mod's lovely directory into PhysFS so modules and copies are visible
            if (PHYSFS_isInit())
            {
                // append to front so mods override game files
                PHYSFS_mount(lovelydir.string().c_str(), nullptr, 1);
            }

            Logger::info(std::string("Mounted mod lovely dir: ") + lovelydir.string());

            for (auto const& file : std::filesystem::recursive_directory_iterator(lovelydir))
            {
                if (!file.is_regular_file())
                    continue;

                auto rel = std::filesystem::relative(file.path(), lovelydir).generic_string();
                std::string target = rel;

                // If file is a toml manifest, parse patches
                if (file.path().filename() == "lovely.toml")
                {
                    Logger::info(std::string("Parsing lovely.toml for mod: ") + dirEntry.path().string());
                    parseLovelyToml(file.path().string(), lovelydir);
                    continue;
                }

                // Otherwise register as an override of the relative path
                Patch p;
                p.type = Patch::OVERRIDE;
                p.target = rel;
                p.source_file = file.path().string();
                patches.push_back(std::move(p));
            }
        }
    }
    catch (std::exception& e)
    {
        Logger::error(std::string("ModManager scan error: ") + e.what());
    }

    scanned = true;
}

void ModManager::parseLovelyToml(const std::string& tomlPath, const std::filesystem::path& baseDir)
{
    // super minimal parsing: look for "[[patches]]" blocks and extract target, pattern, payload, position, copy/module
    std::string content = readFileString(tomlPath);
    if (content.empty())
        return;

    std::istringstream ss(content);
    std::string line;
    Patch current;
    bool inPatch = false;
    while (std::getline(ss, line))
    {
        // trim
        auto trim = [](std::string s) {
            size_t a = s.find_first_not_of(" \t\r\n");
            size_t b = s.find_last_not_of(" \t\r\n");
            if (a==std::string::npos) return std::string();
            return s.substr(a, b-a+1);
        };
        std::string t = trim(line);
        if (t.rfind("[[patches]]", 0) == 0)
        {
            if (inPatch)
            {
                patches.push_back(current);
                current = Patch();
            }
            inPatch = true;
            continue;
        }

        if (!inPatch)
            continue;

        // handle multiline payloads starting with ''' or """
        if (t.rfind("payload", 0) == 0 && (t.find("'''") != std::string::npos || t.find("\"\"\"") != std::string::npos))
        {
            // find the delimiter
            size_t delim_pos = t.find("'''");
            std::string delim = "'''";
            if (delim_pos == std::string::npos)
            {
                delim_pos = t.find("\"\"\"");
                delim = "\"\"\"";
            }
            // read until closing delimiter
            std::string payload;
            if (delim_pos != std::string::npos)
            {
                // consume remainder of line after delimiter
                std::string rest = t.substr(delim_pos + delim.size());
                if (!rest.empty())
                    payload += rest + "\n";
                while (std::getline(ss, line))
                {
                    if (line.rfind(delim, 0) == 0 || line.find(delim) != std::string::npos)
                    {
                        // end
                        break;
                    }
                    payload += line + "\n";
                }
            }
            current.payload = payload;
            current.type = Patch::PATTERN;
            continue;
        }

        auto eq = t.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = trim(t.substr(0, eq));
        std::string val = trim(t.substr(eq+1));
        // strip quotes
        if (!val.empty() && val.front()=='"' && val.back()=='"')
            val = val.substr(1, val.size()-2);

        if (key == "target") current.target = val;
        else if (key == "pattern") { current.pattern = val; current.type = Patch::PATTERN; }
        else if (key == "regex") { current.pattern = val; current.type = Patch::REGEX; }
        else if (key == "position") current.position = val;
        else if (key == "payload") { current.payload = val; current.type = Patch::PATTERN; }
        else if (key == "times") current.times = std::stoi(val);
        else if (key == "match_indent") current.match_indent = (val=="true");
        else if (key == "priority") current.priority = std::stoi(val);
        else if (key == "source") // support single-file copy source
        {
            // allow comma-separated or single value
            if (!val.empty() && val.front() == '[')
            {
                // parse array like ["a","b"]
                std::string s = val.substr(1, val.size()-2);
                std::istringstream arr(s);
                std::string item;
                while (std::getline(arr, item, ','))
                {
                    auto v = trim(item);
                    if (!v.empty() && v.front()=='"' && v.back()=='"')
                        v = v.substr(1, v.size()-2);
                    std::filesystem::path p = baseDir / v;
                    current.sources.push_back(p.string());
                }
            }
            else
            {
                std::filesystem::path p = baseDir / val;
                current.sources.push_back(p.string());
            }
            current.type = Patch::COPY;
        }
        else if (key == "module")
        {
            // support module = "name.lua" or module.name = "..."
            current.type = Patch::MODULE;
            current.module_name = val;
        }
    }

    if (inPatch)
        patches.push_back(current);
}

const std::vector<Patch>& ModManager::getPatchesFor(const std::string& path) const
{
    // Return a vector of patches matching the path by filtering into a static container.
    static std::vector<Patch> results;
    results.clear();
    for (const auto& p : patches)
    {
        if (p.target == path)
            results.push_back(p);
    }
    return results;
}

std::vector<std::string> ModManager::getPrepend(const std::string& path) const
{
    auto it = prepends.find(path);
    if (it == prepends.end())
        return {};
    return it->second;
}

std::vector<std::string> ModManager::getAppend(const std::string& path) const
{
    auto it = appends.find(path);
    if (it == appends.end())
        return {};
    return it->second;
}

bool ModManager::hasOverride(const std::string& path) const
{
    return overrides.find(path) != overrides.end();
}

std::string ModManager::getOverride(const std::string& path) const
{
    auto it = overrides.find(path);
    if (it == overrides.end())
        return std::string();
    return it->second;
}

} // namespace love

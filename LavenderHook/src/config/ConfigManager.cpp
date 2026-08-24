#include "ConfigManager.h"
#include <fstream>
#include <cstdlib>
#include <windows.h>

namespace LavenderHook::Config {

    static std::string g_game_title;

    void SetCurrentGameTitle(const std::string& title)
    {
        g_game_title = SanitizeFileName(title);
    }

    std::string SanitizeFileName(const std::string& name)
    {
        std::string safe = name;
        for (auto& c : safe)
        {
            if (c == '\\' || c == '/' || c == ':' || c == '*' ||
                c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                c = '_';
        }
        if (safe.empty())
            safe = "Unknown";
        return safe;
    }

    std::string GetBaseDir()
    {
        char* app = nullptr;
        size_t len = 0;
        std::string dir = ".";

        if (_dupenv_s(&app, &len, "APPDATA") == 0 && app)
        {
            dir = app;
            free(app);
        }

        dir += "\\LavenderHook";
        CreateDirectoryA(dir.c_str(), nullptr);

        if (!g_game_title.empty())
        {
            dir += "\\" + g_game_title;
            CreateDirectoryA(dir.c_str(), nullptr);
        }

        // Verify directory exists
        DWORD attr = GetFileAttributesA(dir.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            // Directory doesn't exist, try to create it
            CreateDirectoryA(dir.c_str(), nullptr);
        }

        return dir;
    }

    static std::unordered_map<std::string, Store> g_stores;

    Store& Store::Instance()
    {
        static Store& defaultStore = Instance("default.ini");
        return defaultStore;
    }

    Store& Store::Instance(const std::string& file)
    {
        return g_stores[file];
    }

    void Store::Load(const std::string& file)
    {
        m_path = GetBaseDir() + "\\" + file;
        m_ints.clear();
        m_dirty = false;

        std::ifstream f(m_path);
        if (!f)
            return;

        std::string line;
        while (std::getline(f, line))
        {
            auto eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            m_ints[line.substr(0, eq)] =
                std::atoi(line.substr(eq + 1).c_str());
        }
    }

    void Store::Save()
    {
        if (!m_dirty || m_path.empty())
            return;

        std::ofstream f(m_path, std::ios::trunc);
        if (!f)
            return;

        for (auto& kv : m_ints)
            f << kv.first << "=" << kv.second << "\n";

        m_dirty = false;
    }

    // Read-only
    int Store::GetInt(const std::string& key, int def) const
    {
        auto it = m_ints.find(key);
        return it != m_ints.end() ? it->second : def;
    }

    // Read + insert default if missing
    int Store::EnsureInt(const std::string& key, int def)
    {
        auto it = m_ints.find(key);
        if (it != m_ints.end())
            return it->second;

        m_ints[key] = def;
        m_dirty = true;
        return def;
    }

    void Store::SetInt(const std::string& key, int value)
    {
        auto it = m_ints.find(key);
        if (it == m_ints.end() || it->second != value)
        {
            m_ints[key] = value;
            m_dirty = true;
        }
    }

} // namespace LavenderHook::Config

#pragma once
#include <string>
#include <unordered_map>

namespace LavenderHook::Config {

    class Store {
    public:
        // Legacy / default config
        static Store& Instance();

        // Per-file config
        static Store& Instance(const std::string& file);

        // Load / save
        void Load(const std::string& file);
        void Save();

        // Accessors
        int  GetInt(const std::string& key, int def = 0) const;
        int  EnsureInt(const std::string& key, int def);
        void SetInt(const std::string& key, int value);

    private:
        std::string m_path;
        std::unordered_map<std::string, int> m_ints;
        bool m_dirty = false;
    };

    // Per-game config directory support
    void SetCurrentGameTitle(const std::string& title);
    std::string GetBaseDir();
    std::string SanitizeFileName(const std::string& name);

} // namespace LavenderHook::Config

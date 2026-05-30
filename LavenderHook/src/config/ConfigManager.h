#pragma once
#include <string>
#include <unordered_map>

namespace LavenderHook::Config {

    // Returns the config base directory (%APPDATA%\LavenderHook), creating it.
    std::string GetBaseDir();

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

} // namespace LavenderHook::Config

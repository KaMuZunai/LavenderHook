#pragma once
#include <string>
#include <vector>

namespace LavenderHook::UI {

    struct FunctionEntry {
        std::string name;
        bool*       pEnabled = nullptr;
    };

    class FunctionRegistry {
    public:
        static FunctionRegistry& Instance();

        void Register(const std::string& name, bool* pEnabled);
        const std::vector<FunctionEntry>& GetAll() const;
        bool* Find(const std::string& name) const;

    private:
        std::vector<FunctionEntry> m_entries;
    };

} // namespace LavenderHook::UI

#include "FunctionRegistry.h"

namespace LavenderHook::UI {

    FunctionRegistry& FunctionRegistry::Instance()
    {
        static FunctionRegistry inst;
        return inst;
    }

    void FunctionRegistry::Register(const std::string& name, bool* pEnabled)
    {
        for (auto& e : m_entries) {
            if (e.name == name) {
                e.pEnabled = pEnabled;
                return;
            }
        }
        m_entries.push_back({ name, pEnabled });
    }

    const std::vector<FunctionEntry>& FunctionRegistry::GetAll() const
    {
        return m_entries;
    }

    bool* FunctionRegistry::Find(const std::string& name) const
    {
        for (const auto& e : m_entries)
            if (e.name == name) return e.pEnabled;
        return nullptr;
    }

} // namespace LavenderHook::UI

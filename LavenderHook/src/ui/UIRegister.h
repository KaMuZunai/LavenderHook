#pragma once
#include <vector>
#include <functional>

void RegisterUIWindows();

struct UIWindowEntry
{
    std::function<void()> Update;
    std::function<void()> Render;
    std::function<bool()> ShouldDraw;
};

class UIRegistry
{
public:
    static UIRegistry& Get()
    {
        static UIRegistry instance;
        return instance;
    }

    void Register(const UIWindowEntry& entry)
    {
        m_windows.emplace_back(entry);
    }

    void Clear()
    {
        m_windows.clear();
    }

    void UIRegistry::Update()
    {
        for (auto& w : m_windows)
        {
            if (w.Update)
                w.Update();
        }
    }

    void UIRegistry::Render()
    {
        for (auto& w : m_windows)
        {
            if (!w.ShouldDraw || w.ShouldDraw())
            {
                if (w.Render)
                    w.Render();
            }
        }
    }

private:
    std::vector<UIWindowEntry> m_windows;
};

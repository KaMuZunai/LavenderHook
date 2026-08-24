#pragma once

#include "../../imgui/imgui.h"

namespace LavenderHook::UI
{
    class LavenderBackgroundDim
    {
    public:
        void Tick(bool wantVisible);
        void Render();

        void SetDimAlpha(float a) { m_dimAlpha = a; }
        float Alpha() const { return m_fade; }

    private:
        float m_fade = 0.0f;
        float m_target = 0.0f;
        float m_speed = 10.0f;
        float m_dimAlpha = 0.3f;
    };
}

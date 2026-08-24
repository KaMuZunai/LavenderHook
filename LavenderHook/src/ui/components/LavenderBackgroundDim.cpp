#include "LavenderBackgroundDim.h"

#include <cmath>

namespace LavenderHook::UI
{
    void LavenderBackgroundDim::Tick(bool wantVisible)
    {
        m_target = wantVisible ? 1.0f : 0.0f;

        const float dt = ImGui::GetIO().DeltaTime;
        m_fade += (m_target - m_fade) * m_speed * dt;

        if (std::fabs(m_fade) < 0.001f) m_fade = 0.0f;
        if (std::fabs(m_fade - 1.0f) < 0.001f) m_fade = 1.0f;
    }

    void LavenderBackgroundDim::Render()
    {
        if (m_fade <= 0.0f && m_target <= 0.0f)
            return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const ImVec2 p0(0.f, 0.f);
        const ImVec2 p1 = ImGui::GetIO().DisplaySize;
        dl->AddRectFilled(p0, p1, ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, m_dimAlpha * m_fade)));
    }
}

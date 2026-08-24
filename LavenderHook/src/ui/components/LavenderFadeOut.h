#pragma once
#include "../../imgui/imgui.h"
#include <cmath>

namespace LavenderHook::UI
{
    class LavenderFadeOut
    {
    public:
        void Tick(bool wantVisible)
        {
            SetVisible(wantVisible);
            Update();
        }

        void SetVisible(bool v)
        {
            m_target = v ? 1.0f : 0.0f;
        }

        void Update()
        {
            float dt = ImGui::GetIO().DeltaTime;
            float diff = m_target - m_alpha;
            if (fabsf(diff) < 0.001f)
            {
                m_alpha = m_target;
                return;
            }

            float boost = 1.0f;
            if (diff < 0.0f)
            {
                float a = m_alpha;
                if (a <= 0.5f)
                    boost = 3.0f;
                else if (a < 0.65f)
                    boost = 3.0f - 2.0f * (a - 0.5f) / 0.15f;
            }

            m_alpha += diff * m_speed * dt * boost;

            if (fabsf(m_alpha) < 0.001f) m_alpha = 0.0f;
            if (fabsf(m_alpha - 1.0f) < 0.001f) m_alpha = 1.0f;
        }

        float Alpha() const { return m_alpha; }

        bool ShouldRender() const
        {
            return m_alpha > 0.0f || m_target > 0.0f;
        }

        bool IsFullyVisible() const { return m_alpha >= 1.0f; }
        bool IsFullyHidden()  const { return m_alpha <= 0.0f; }

        void SetSpeed(float s) { m_speed = s; }

    private:
        float m_alpha = 0.0f;
        float m_target = 0.0f;
        float m_speed = 4.0f;
    };
}

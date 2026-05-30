#include "ParagonLevelWindow.h"
#include "../../misc/Globals.h"
#include "../../misc/xpsystem/XpManager.h"
#include "../../misc/xpsystem/XpTables.h"
#include "../../imgui/imgui.h"
#include "../../ui/components/LavenderFadeOut.h"

#include <cmath>
#include <string>

static constexpr float kPLW_TwoPi  = 6.28318530717958647692f;
static constexpr float kPLW_HalfPi = 1.57079632679489661923f;
static constexpr float kPLW_Pi     = 3.14159265358979323846f;

extern ImVec4 DARK_RED;

// Arc bar animation states
enum class ArcBarState { Idle, FillingToFull, Flashing, Resetting };

namespace LavenderHook::UI::Windows {

    namespace {
        static LavenderHook::UI::LavenderFadeOut s_fade;

        static ArcBarState s_barState      = ArcBarState::Idle;
        static float       s_displayProg   = 0.f;  // value currently drawn [0..1]
        static int         s_lastLevel     = -1;   // level-up detection
        static float       s_animSrc       = 0.f;  // interpolation start
        static float       s_animTarget    = 0.f;  // interpolation end
        static float       s_animTimer     = 0.f;  // time within current phase
        static float       s_flashTimer    = 0.f;  // time within flash phase
        static float       s_newTargetProg = 0.f;  // real progress after level-up
    }

    void ParagonLevelWindow::Render(bool wantVisible)
    {
        s_fade.Tick(wantVisible);
        if (!s_fade.ShouldRender()) return;

        const float alpha = s_fade.Alpha();

        // Geometry
        const float displayH  = ImGui::GetIO().DisplaySize.y;
        const float SIZE          = displayH * 0.045f;
        const float outlineR      = SIZE * 0.48f;              // gray border
        const float ringThick     = SIZE * 0.14f;              // thick XP ring
        const float ringOuterEdge = outlineR - SIZE * 0.03f;  // outer edge of the ring
        const float ringR         = ringOuterEdge - ringThick * 0.5f;
        const float innerR        = ringOuterEdge - ringThick; // black inner fill radius
        const float fontScale     = (SIZE / 120.0f) * 2.6f;   // scales with SIZE

        // Window setup
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        ImGui::SetNextWindowSize(ImVec2(SIZE, SIZE), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(20.0f, 100.0f), ImGuiCond_FirstUseEver);

        constexpr ImGuiWindowFlags kFlags =
            ImGuiWindowFlags_NoTitleBar           |
            ImGuiWindowFlags_NoResize             |
            ImGuiWindowFlags_NoScrollbar          |
            ImGuiWindowFlags_NoScrollWithMouse    |
            ImGuiWindowFlags_NoCollapse           |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoFocusOnAppearing   |
            ImGuiWindowFlags_NoNav;

        if (!ImGui::Begin("##ParagonLevel", nullptr, kFlags))
        {
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            return;
        }

        // Draw
        const auto Col = [&](int r, int g, int b, int a) -> ImU32 {
            return IM_COL32(r, g, b, static_cast<int>(a * alpha));
        };

        ImDrawList*  dl  = ImGui::GetWindowDrawList();
        const ImVec2 wp  = ImGui::GetWindowPos();
        const ImVec2 ctr = ImVec2(wp.x + SIZE * 0.5f, wp.y + SIZE * 0.5f);

        // Black base circle
        dl->AddCircleFilled(ctr, outlineR, Col(0, 0, 0, 230), 64);

        // Gray outer outline
        dl->AddCircle(ctr, outlineR, Col(110, 110, 110, 220), 64, 1.5f);

        // Dark track ring
        dl->AddCircle(ctr, ringR, Col(30, 30, 30, 255), 64, ringThick);

        // XP data
        const int   level     = LavenderHook::XpManager::GetLevel();
        const float currentXp = LavenderHook::XpManager::GetCurrentXp();
        const float threshold = LavenderHook::XpTables::GetXpThreshold(level);
        const float realProg  = (threshold > 0.f) ? (currentXp / threshold) : 0.f;
        const float dt        = ImGui::GetIO().DeltaTime;

        constexpr float kFillDuration  = 0.33f;
        constexpr float kFlashDuration = 0.45f;

        // Animation state machine 
        if (s_lastLevel < 0) {
            s_lastLevel   = level;
            s_displayProg = realProg;
        }

        if (level > s_lastLevel) {
            s_lastLevel      = level;
            s_newTargetProg  = realProg;
            s_animSrc        = s_displayProg;
            s_animTarget     = 1.0f;
            s_animTimer      = 0.f;
            s_barState       = ArcBarState::FillingToFull;
        }

        switch (s_barState) {

            case ArcBarState::Idle: {
                const float delta = realProg - s_displayProg;
                if (delta > 0.0005f) {
                    s_displayProg += (delta / kFillDuration) * dt;
                    if (s_displayProg > realProg) s_displayProg = realProg;
                } else {
                    s_displayProg = realProg;
                }
                break;
            }

            case ArcBarState::FillingToFull: {
                s_animTimer += dt;
                float t = s_animTimer / kFillDuration;
                if (t >= 1.0f) {
                    s_displayProg = 1.0f;
                    s_flashTimer  = 0.f;
                    s_barState    = ArcBarState::Flashing;
                } else {
                    t = t * t * (3.0f - 2.0f * t);
                    s_displayProg = s_animSrc + (s_animTarget - s_animSrc) * t;
                }
                break;
            }

            case ArcBarState::Flashing: {
                s_flashTimer += dt;
                if (s_flashTimer >= kFlashDuration) {
                    s_displayProg = 0.f;
                    s_animSrc     = 0.f;
                    s_animTarget  = s_newTargetProg;
                    s_animTimer   = 0.f;
                    s_barState    = ArcBarState::Resetting;
                }
                break;
            }

            case ArcBarState::Resetting: {
                s_animTimer += dt;
                float t = s_animTimer / kFillDuration;
                if (t >= 1.0f) {
                    s_displayProg = s_animTarget;
                    s_barState    = ArcBarState::Idle;
                } else {
                    t = t * t * (3.0f - 2.0f * t);
                    s_displayProg = s_animSrc + (s_animTarget - s_animSrc) * t;
                }
                break;
            }
        }

        const float drawProg = (s_displayProg < 0.f) ? 0.f : (s_displayProg > 1.f ? 1.f : s_displayProg);

        // Compute arc colour
        ImVec4 arcColorV(DARK_RED.x, DARK_RED.y, DARK_RED.z, 1.0f);
        if (s_barState == ArcBarState::Flashing) {
            const float w = sinf((s_flashTimer / kFlashDuration) * kPLW_Pi); // 0->1->0
            arcColorV.x = DARK_RED.x + w * (1.0f - DARK_RED.x);
            arcColorV.y = DARK_RED.y + w * (1.0f - DARK_RED.y);
            arcColorV.z = DARK_RED.z + w * (1.0f - DARK_RED.z);
        }

        const ImU32 arcCol = IM_COL32(
            static_cast<int>(arcColorV.x * 255.f),
            static_cast<int>(arcColorV.y * 255.f),
            static_cast<int>(arcColorV.z * 255.f),
            static_cast<int>(255.f * alpha)
        );

        if (drawProg > 0.001f) {
            dl->PathArcTo(ctr, ringR, -kPLW_HalfPi, -kPLW_HalfPi + kPLW_TwoPi * drawProg, 64);
            dl->PathStroke(arcCol, false, ringThick);
        }

        // Black inner circle
        dl->AddCircleFilled(ctr, innerR, Col(0, 0, 0, 230), 64);

        // Level number 
        const std::string lvlStr  = std::to_string(level);
        ImFont* const     font    = ImGui::GetFont();
        const float       fSize   = font->LegacySize * fontScale;
        const ImVec2      textSz  = font->CalcTextSizeA(fSize, FLT_MAX, 0.0f, lvlStr.c_str());
        const float       tx      = wp.x + (SIZE - textSz.x) * 0.5f;
        const float       ty      = wp.y + (SIZE - textSz.y) * 0.5f;
        const ImU32       tCol    = IM_COL32(255, 255, 255, static_cast<int>(255.f * alpha));

        const ImU32  borderCol = IM_COL32(0, 0, 0, static_cast<int>(255.f * alpha));
        const float  bOff      = 1.0f;
        const float  borderOff = 2.0f;
        const ImVec2 borderPasses[] = {
            {  borderOff,  0.f       }, { -borderOff,  0.f       },
            {  0.f,        borderOff }, {  0.f,       -borderOff },
            {  borderOff,  borderOff }, { -borderOff,  borderOff },
            {  borderOff, -borderOff }, { -borderOff, -borderOff }
        };
        for (const auto& off : borderPasses)
            dl->AddText(font, fSize, ImVec2(tx + off.x, ty + off.y), borderCol, lvlStr.c_str());

        const ImVec2 passes[] = {
            { 0.f,   0.f  },
            { bOff,  0.f  }, { -bOff, 0.f  },
            { 0.f,   bOff }, {  0.f, -bOff }
        };
        for (const auto& off : passes)
            dl->AddText(font, fSize, ImVec2(tx + off.x, ty + off.y), tCol, lvlStr.c_str());

        // Cleanup
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

} // namespace LavenderHook::UI::Windows

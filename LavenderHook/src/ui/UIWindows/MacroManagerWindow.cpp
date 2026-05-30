#include "MacroManagerWindow.h"
#include "MacroWindowsShared.h"
#include "../../assets/UITextures.h"
#include "../../misc/Globals.h"
#include "../../sound/SoundPlayer.h"
#include "../../ui/ActionsOverlay.h"
#include "../../ui/components/LavenderFadeOut.h"
#include "../../ui/components/LavenderWindowHeader.h"
#include "../../ui/components/LavenderUI.h"
#include "../../imgui/imgui.h"
#include "../../imgui/imgui_internal.h"
#include <cmath>
#include <vector>

extern ImVec4 MAIN_RED;

static float S() { return LavenderHook::Globals::menu_scale; }

namespace {
    constexpr float kWindowWidth = 320.0f;
    constexpr float kExpandedWindowHeight = 520.0f;
    constexpr float kCollapsedWindowHeight = 38.0f;
    constexpr float kDropdownRowSpacing = 2.0f;
    constexpr float kDropdownClosedBuffer = 2.0f;
    constexpr float kDropdownArrowInset = 6.0f;
    constexpr float kDropdownArrowScale = 0.40f;

    static float EaseInOut(float t)
    {
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return t * t * (3.0f - 2.0f * t);
    }
}

namespace LavenderHook::UI::Windows {

    struct MacroRowState {
        float colorAnim = 0.0f;
        float arrowAnim = 0.0f;
        float dropdownAnim = 0.0f;
        float layoutHeight = 0.0f;
        float dropdownFade = 0.0f;
        bool dropdownOpen = false;
        bool dropdownOpenNext = false;
    };

    static LavenderHook::UI::LavenderFadeOut g_fade;
    static bool g_managerHeaderOpen = true;
    static float g_managerHeaderAnim = 1.0f;
    static float g_managerArrowAnim = 0.0f;
    static std::vector<MacroRowState> g_macroRowStates;

    void MacroManagerWindow::Init()
    {
        InitMacros();
    }

    static ImU32 LerpColor(ImU32 a, ImU32 b, float t)
    {
        ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
        ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);
        ImVec4 c(
            ca.x + (cb.x - ca.x) * t,
            ca.y + (cb.y - ca.y) * t,
            ca.z + (cb.z - ca.z) * t,
            ca.w + (cb.w - ca.w) * t
        );
        return ImGui::ColorConvertFloat4ToU32(c);
    }

    static float AnimateTowards(float current, float target, float speed)
    {
        return current + (target - current) * ImGui::GetIO().DeltaTime * speed;
    }

    void MacroManagerWindow::UpdateActions()
    {
        if (g_macroRowStates.size() != g_macros.size())
            g_macroRowStates.assign(g_macros.size(), MacroRowState());

        for (int i = 0; i < (int)g_macros.size(); ++i)
        {
            auto& macro = *g_macros[i];
            if (macro.hotkey.keyVK != &macro.hotkeyVK)
                macro.hotkey.keyVK = &macro.hotkeyVK;

            bool before = macro.enabled;
            macro.hotkey.UpdateToggle(macro.enabled);
            if (macro.enabled != before)
            {
                LavenderHook::UI::Actions::SetActive(macro.name, macro.enabled);
                g_selectedMacro = i;
                MarkMacrosDirty();
            }
        }

        SaveDirtyMacros();
        UpdateMacroExecution();
    }

    void MacroManagerWindow::SwapRowStates(int a, int b)
    {
        if (a < 0 || b < 0 || a >= (int)g_macroRowStates.size() || b >= (int)g_macroRowStates.size())
            return;
        std::swap(g_macroRowStates[a], g_macroRowStates[b]);
    }

    void MacroManagerWindow::Render(bool wantVisible)
    {
        Init();
        g_fade.Tick(wantVisible);
        if (!g_fade.ShouldRender())
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_fade.Alpha());

        float s = S();

        ImGui::SetNextWindowSize(ImVec2(kWindowWidth * s, kCollapsedWindowHeight * s), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(kWindowWidth * s, kCollapsedWindowHeight * s), ImVec2(kWindowWidth * s, FLT_MAX));

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse;

        if (!ImGui::Begin("Macro Manager##MacroManager", nullptr, flags))
        {
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }

        LavenderHook::UI::Lavender::RenderWindowHeader(
            "Macro Manager",
            g_dotsIcoTex,
            g_dropLeftTex,
            ImGui::GetWindowWidth(),
            g_fade.Alpha(),
            g_managerHeaderOpen,
            g_managerHeaderAnim,
            g_managerArrowAnim);

        const float contentStartY = ImGui::GetCursorPosY();

        s = S();

        float target = g_managerHeaderOpen ? 1.0f : 0.0f;
        g_managerHeaderAnim += (target - g_managerHeaderAnim) * ImGui::GetIO().DeltaTime * 8.0f;
        g_managerHeaderAnim = g_managerHeaderAnim < 0.0f ? 0.0f : (g_managerHeaderAnim > 1.0f ? 1.0f : g_managerHeaderAnim);

        if (g_managerHeaderAnim > 0.001f)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_fade.Alpha() * EaseInOut(g_managerHeaderAnim));

            bool deleted = false;
            for (int i = 0; i < (int)g_macros.size() && !deleted; ++i)
            {
                auto& macro = *g_macros[i];
                if (macro.hidden)
                    continue;

                auto& row = g_macroRowStates[i];
                const float arrowWidth = 18.0f * s;
                const float height = ImGui::GetFrameHeight();
                const float width = ImGui::GetContentRegionAvail().x;
                ImVec2 pos = ImGui::GetCursorScreenPos();

                ImGui::PushID(i);
                ImGui::InvisibleButton("##macro_btn", ImVec2(width, height));
                bool clicked = ImGui::IsItemClicked();
                bool hovered = ImGui::IsItemHovered();
                bool held = ImGui::IsItemActive();
                bool active = macro.enabled;
                bool overArrow = ImGui::GetIO().MousePos.x >= (pos.x + width - arrowWidth - kDropdownArrowInset);

                float colorTarget = held ? 1.0f : active ? (hovered ? 0.55f : 0.45f) : hovered ? 0.6f : 0.0f;
                row.colorAnim = AnimateTowards(row.colorAnim, colorTarget, 10.0f);
                float t = EaseInOut(row.colorAnim);

                row.dropdownOpenNext = row.dropdownOpen;
                if (clicked)
                {
                    if (overArrow)
                        row.dropdownOpenNext = !row.dropdownOpen;
                    else
                    {
                        bool before = macro.enabled;
                        macro.enabled = !macro.enabled;
                        LavenderHook::UI::Actions::SetActive(macro.name, macro.enabled);
                        if (before != macro.enabled)
                        {
                            LavenderHook::Audio::PlayToggleSound(macro.enabled);
                            MarkMacrosDirty();
                        }
                        g_selectedMacro = i;
                    }
                }

                float dropdownTarget = row.dropdownOpenNext ? 1.0f : 0.0f;
                row.dropdownAnim += (dropdownTarget - row.dropdownAnim) * ImGui::GetIO().DeltaTime * 7.0f;
                row.dropdownAnim = row.dropdownAnim < 0.0f ? 0.0f : (row.dropdownAnim > 1.0f ? 1.0f : row.dropdownAnim);
                row.arrowAnim = AnimateTowards(row.arrowAnim, row.dropdownOpenNext ? 1.0f : 0.0f, 7.0f);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                const bool polished = LavenderHook::Globals::use_polished_overlay;
                float borderSize = ImGui::GetStyle().FrameBorderSize;

                if (polished) {
                    float rr = ImGui::GetStyle().FrameRounding;
                    // Drop shadow + frosted fill + top sheen + accent border
                    LavenderHook::UI::Lavender::PolishedPanel(
                        dl, pos, ImVec2(pos.x + width, pos.y + height),
                        rr, g_fade.Alpha(), false, height * 0.5f);

                    // Animated colored highlight when active/held
                    if (t > 0.01f) {
                        dl->AddRectFilled(pos,
                            ImVec2(pos.x + width, pos.y + height),
                            ImGui::GetColorU32(ImVec4(
                                MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.60f * t)),
                            rr);
                    }

                    // arrow divider (subtle accent)
                    float dividerX = pos.x + width - arrowWidth - kDropdownArrowInset;
                    dl->AddLine(ImVec2(dividerX, pos.y + 4.0f),
                                ImVec2(dividerX, pos.y + height - 4.0f),
                                LavenderHook::UI::Lavender::PolishedAccent(0.35f * g_fade.Alpha()), 1.0f);
                }
                else {
                    ImU32 baseCol = ImGui::GetColorU32(ImGuiCol_FrameBg);
                    ImU32 hoverCol = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);
                    ImU32 activeCol = ImGui::GetColorU32(ImGuiCol_ButtonActive);
                    ImU32 bg = LerpColor(baseCol, active ? activeCol : hoverCol, t);

                    ImGui::RenderFrame(pos, ImVec2(pos.x + width, pos.y + height), bg, true, ImGui::GetStyle().FrameRounding);

                    if (borderSize > 0.0f)
                    {
                        dl->AddRect(pos, ImVec2(pos.x + width, pos.y + height), ImGui::GetColorU32(ImGuiCol_Border), ImGui::GetStyle().FrameRounding, 0, borderSize);
                        float dividerX = pos.x + width - arrowWidth - kDropdownArrowInset;
                        dl->AddLine(ImVec2(dividerX, pos.y + 4.0f), ImVec2(dividerX, pos.y + height - 4.0f), ImGui::GetColorU32(ImGuiCol_Border), borderSize);
                    }
                }

                dl->AddText(ImVec2(pos.x + 8.0f * s, pos.y + (height - ImGui::GetFontSize()) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), macro.name.c_str());

                if (g_dropLeftTex) {
                    float iconSize = height * 0.40f;
                    ImVec2 center(pos.x + width - arrowWidth * 0.5f - kDropdownArrowInset, pos.y + height * 0.5f);
                    constexpr float kPi = 3.14159265358979323846f;
                    float angle = -row.arrowAnim * kPi * 0.5f;
                    float s = sinf(angle);
                    float c = cosf(angle);
                    ImVec2 half(iconSize * 0.5f, iconSize * 0.5f);
                    ImVec2 corners[4] = {{-half.x, -half.y}, {half.x, -half.y}, {half.x, half.y}, {-half.x, half.y}};
                    for (int j = 0; j < 4; ++j)
                        corners[j] = ImVec2(center.x + corners[j].x * c - corners[j].y * s, center.y + corners[j].x * s + corners[j].y * c);
                    dl->AddImageQuad(g_dropLeftTex, corners[0], corners[1], corners[2], corners[3], ImVec2(0,0), ImVec2(1,0), ImVec2(1,1), ImVec2(0,1), ImGui::GetColorU32(ImVec4(1,1,1,g_fade.Alpha())));
                }

                row.dropdownOpen = row.dropdownOpenNext;

                if (ImGui::BeginPopupContextItem("macro_context"))
                {
                    if (ImGui::MenuItem(" Delete Macro"))
                    {
                        DeleteMacro(i);
                        deleted = true;
                    }
                    ImGui::EndPopup();
                }

                const float rowH = ImGui::GetFrameHeight();
                const float rowHInt = std::round(rowH);
                const float spacingInt = std::round(kDropdownRowSpacing * s);
                int rows = 1;
                float full = rows * rowHInt + (rows > 0 ? (rows - 1) * spacingInt : 0.0f);
                float tDropdown = EaseInOut(row.dropdownAnim);
                float animated = tDropdown * full;
                float delta = animated - row.layoutHeight;
                float alpha = 0.0f;
                if (delta < 0.0f)
                {
                    alpha = ImClamp(ImGui::GetIO().DeltaTime * 80.0f, 0.0f, 0.99f);
                }
                else
                {
                    float speed = std::max(12.0f, 60.0f / std::max(1.0f, full));
                    alpha = ImClamp(ImGui::GetIO().DeltaTime * speed, 0.0f, 0.9f);
                }
                row.layoutHeight += delta * alpha;

                float childHeightRounded = (!row.dropdownOpen) ? std::ceil(row.layoutHeight) : std::round(row.layoutHeight);
                childHeightRounded = std::min(childHeightRounded, full);
                float childContainerHeight = std::max(childHeightRounded, kDropdownClosedBuffer);
                float itemSpacing = childContainerHeight > 0.5f ? 0.0f : kDropdownClosedBuffer;

                    float savedCursorX = ImGui::GetCursorPosX();
                    ImGui::Indent(12.0f * s);
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, row.dropdownAnim * g_fade.Alpha() * EaseInOut(g_managerHeaderAnim));
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

                    ImGui::BeginChild("##dropdown_child", ImVec2(0.0f, childContainerHeight), false,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav);
                    ImGui::SetScrollY(0.0f);

                    if (macro.hotkey.keyVK != &macro.hotkeyVK)
                        macro.hotkey.keyVK = &macro.hotkeyVK;

                    float labelX = ImGui::GetCursorPosX();
                    float controlX = labelX + (70.0f + 10.0f) * s;
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("Hotkey:");
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(controlX);
                    if (macro.hotkey.Render(ImVec2(120 * s, ImGui::GetFrameHeight())))
                        MarkMacrosDirty();

                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                    ImGui::Unindent(12.0f * s);
                    ImGui::SetCursorPosX(savedCursorX);
                    ImGui::PopID();
                    ImGui::Dummy(ImVec2(0.0f, itemSpacing));
            }

            if (ImGui::Button("Open Editor", ImVec2(-1, 0)))
            {
                if (g_selectedMacro < 0 && !g_macros.empty())
                    g_selectedMacro = 0;
                g_editorOpen = true;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::PopStyleVar();
        }

        {
            float contentEndY = ImGui::GetCursorPosY();
            float actualContent = (contentEndY - contentStartY) * EaseInOut(g_managerHeaderAnim);

            static float s_contentAnim = 0.0f;
            if (s_contentAnim == 0.0f)
                s_contentAnim = actualContent;
            s_contentAnim += (actualContent - s_contentAnim) * ImGui::GetIO().DeltaTime * 20.0f;
            if (fabsf(s_contentAnim - actualContent) < 0.5f)
                s_contentAnim = actualContent;

            float bottomPad = ImGui::GetStyle().WindowPadding.y + 4.0f;
            ImGui::SetWindowSize(ImVec2(kWindowWidth * s, kCollapsedWindowHeight * s + s_contentAnim + bottomPad));
        }

        ImVec2 sbPos = ImGui::GetWindowPos();
        ImVec2 sbSize = ImGui::GetWindowSize();
        ImGui::End();
        if (LavenderHook::Globals::use_polished_overlay)
            LavenderHook::UI::Lavender::DrawWindowShadow(sbPos, sbSize, g_fade.Alpha());
        ImGui::PopStyleVar();
    }
}

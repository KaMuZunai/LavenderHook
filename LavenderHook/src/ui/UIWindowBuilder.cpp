#include "UIWindowBuilder.h"
#include "components/LavenderUI.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_internal.h"
#include "../assets/UITextures.h"
#include "UIWindows/SettingsWindow.h"
#include "../misc/Globals.h"
#include "GUI.h"
#include "../sound/SoundPlayer.h"
#include <cmath>

constexpr float kDropdownArrowInset = 6.0f;   // left
constexpr float kDropdownArrowScale = 0.40f;
constexpr float kDropdownRowSpacing = 2.0f;

static float Scale(float v) { return v * LavenderHook::Globals::menu_scale; }

using namespace LavenderHook::UI;
using namespace LavenderHook::UI::Lavender;

namespace {
    constexpr float kToggleWidth = 170.0f;
    constexpr float kHotkeyWidth = 70.0f;
    constexpr float kButtonWidth = 240.0f;

    constexpr float kBaseHeight = 8.0f;
    constexpr float kRowHeight = 47.0f;
    constexpr float kRowSpacing = 8.0f;

    constexpr float kDropdownClosedBuffer = 2.0f;
    constexpr float kDropdownExpansionOffset = 5.0f;

    constexpr float kTimingLabelWidth = 70.0f;
    constexpr float kTimingSliderWidth = 140.0f;

    constexpr float kTimingControlX =
        kTimingLabelWidth + 10.0f; // padding between label and control
}



UIWindowBuilder& UIWindowBuilder::AddItemDescription(const char* description)
{
    if (m_items.empty())
        return *this;

    UIItem& it = m_items.back();
    it.description = description;
    return *this;
}

UIWindowBuilder::UIWindowBuilder(const char* title)
    : m_title(title)
{
}


UIWindowBuilder& UIWindowBuilder::SetWidth(float w)
{
    m_width = w;
    return *this;
}

UIWindowBuilder& UIWindowBuilder::AddToggle(
    const char* label,
    bool* value,
    int* hotkeyVK)
{
    UIItem item{};
    item.type = UIItemType::Toggle;
    item.label = label;
    item.toggle = value;
    item.hotkeyVK = hotkeyVK;

    if (hotkeyVK) {
        item.hotkeyIndex = (int)m_hotkeys.size();
        m_hotkeys.emplace_back();
        m_hotkeys.back().keyVK = hotkeyVK;
    }

    m_items.push_back(item);
    return *this;
}

UIWindowBuilder& UIWindowBuilder::AddDropdownButton(
    const char* label,
    int* hotkeyVK)
{
    if (m_items.empty())
        return *this;

    UIItem& it = m_items.back();
    if (it.type != UIItemType::ToggleDropdown)
        return *this;

    UITiming t;
    t.label = label;
    t.isHotkey = true;
    t.hotkeyVK = hotkeyVK;
    if (hotkeyVK) {
        t.hotkey.keyVK = hotkeyVK;
    }

    it.timings.push_back(t);
    return *this;
}

UIWindowBuilder& UIWindowBuilder::AddToggleDropdown(
    const char* label,
    bool* value,
    int* hotkeyVK)
{
    UIItem item{};
    item.type = UIItemType::ToggleDropdown;
    item.label = label;
    item.toggle = value;
    item.hotkeyVK = hotkeyVK;

    if (hotkeyVK) {
        item.hotkeyIndex = (int)m_hotkeys.size();
        m_hotkeys.emplace_back();
        m_hotkeys.back().keyVK = hotkeyVK;
    }

    m_items.push_back(item);
    return *this;
}

UIWindowBuilder& UIWindowBuilder::AddDropdownCheckbox(
    const char* label,
    bool* value)
{
    if (m_items.empty())
        return *this;

    UIItem& it = m_items.back();
    if (it.type != UIItemType::ToggleDropdown)
        return *this;

    UITiming t;
    t.label = label;
    t.isCheckbox = true;
    t.checkboxValue = value;

    it.timings.push_back(t);
    return *this;
}

UIWindowBuilder& UIWindowBuilder::AddDropdownIntInput(
    const char* label,
    int* value,
    int min,
    int max)
{
    if (m_items.empty())
        return *this;

    UIItem& it = m_items.back();
    if (it.type != UIItemType::ToggleDropdown)
        return *this;

    it.timings.push_back({ label, value, min, max, false, true });
    return *this;
}

UIWindowBuilder& UIWindowBuilder::AddDropdownTiming(
    const char* label,
    int* valueMs,
    int minMs,
    int maxMs)
{
    if (m_items.empty())
        return *this;

    UIItem& it = m_items.back();
    if (it.type != UIItemType::ToggleDropdown)
        return *this;

    it.timings.push_back({ label, valueMs, minMs, maxMs, false });
    return *this;
}

UIWindowBuilder& UIWindowBuilder::AddDropdownTimingSeconds(
    const char* label,
    int* valueMs,
    int minMs,
    int maxMs)
{
    if (m_items.empty())
        return *this;

    UIItem& it = m_items.back();
    if (it.type != UIItemType::ToggleDropdown)
        return *this;

    it.timings.push_back({ label, valueMs, minMs, maxMs, true });
    return *this;
}

UIWindowBuilder& UIWindowBuilder::AddDropdownSliderFloat(
    const char* label,
    float* value,
    float min,
    float max,
    const char* fmt)
{
    if (m_items.empty())
        return *this;

    UIItem& it = m_items.back();
    if (it.type != UIItemType::ToggleDropdown)
        return *this;

    UITiming t{};
    t.label = label;
    t.sliderFloat = value;
    t.sliderMin = min;
    t.sliderMax = max;
    t.sliderFmt = fmt;
    it.timings.push_back(t);
    return *this;
}

UIWindowBuilder& UIWindowBuilder::AddSlider(
    const char* label,
    int* value,
    int min,
    int max)
{
    UIItem item{};
    item.type = UIItemType::SliderInt;
    item.label = label;
    item.sliderInt = value;
    item.min = min;
    item.max = max;

    m_items.push_back(item);
    return *this;
}

UIWindowBuilder& UIWindowBuilder::AddSliderFloat(
    const char* label,
    float* value,
    float min,
    float max)
{
    UIItem item{};
    item.type = UIItemType::SliderFloat;
    item.label = label;
    item.sliderFloat = value;
    item.minF = min;
    item.maxF = max;

    m_items.push_back(item);
    return *this;
}

UIWindowBuilder& UIWindowBuilder::AddButton(
    const char* label,
    std::function<void()> onClick)
{
    UIItem item{};
    item.type = UIItemType::Button;
    item.label = label;
    item.onClick = std::move(onClick);

    m_items.push_back(item);
    return *this;
}

static float EaseInOut(float t)
{
    t = ImClamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

UIWindowBuilder& UIWindowBuilder::SetHeaderIcon(ImTextureID icon)
{
    m_headerIcon = icon;
    return *this;
}


float UIWindowBuilder::ComputeHeight() const
{
    float s = Scale(1.0f);
    float height = kBaseHeight * s;

    // Polished theme has larger ItemSpacing.y (6 vs 4) and FramePadding, adding ~8px per item
    float polishedExtra = LavenderHook::Globals::use_polished_overlay ? 8.0f * s : 0.0f;

    for (const auto& it : m_items)
    {
                height += kRowHeight * s + polishedExtra;

        if (it.type == UIItemType::ToggleDropdown)
        {
            int rows = 0; 
            if (it.hotkeyIndex >= 0)
                rows += 1;
            rows += (int)it.timings.size();

            float rowH = ImGui::GetFrameHeight();
            float rowHInt = std::round(rowH);
            float spacingInt = std::round(kDropdownRowSpacing * s);
            float full = rows * rowHInt + (rows > 0 ? (rows - 1) * spacingInt : 0.0f);
            float t = EaseInOut(it.dropdownAnim);
            float fullInt = std::round(full); 
            float animated = t * fullInt;
            auto& nit = const_cast<UIItem&>(it);
            float delta = animated - nit.layoutHeight;
            float alpha = 0.0f;
            if (delta < 0.0f) {
                float closeSpeed = 80.0f;
                alpha = ImClamp(ImGui::GetIO().DeltaTime * closeSpeed, 0.0f, 0.99f);
            }
            else {
                float baseSpeed = 12.0f;
                float minSpeedForSmall = 60.0f;
                float speed = std::max(baseSpeed, minSpeedForSmall / std::max(1.0f, fullInt));
                alpha = ImClamp(ImGui::GetIO().DeltaTime * speed, 0.0f, 0.9f);
            }
            nit.layoutHeight += delta * alpha;

            float layoutRounded = (!nit.dropdownOpen) ? std::ceil(nit.layoutHeight) : std::round(nit.layoutHeight);
            layoutRounded = std::min(layoutRounded, fullInt);
            float closedBufInt = std::round(kDropdownClosedBuffer * s);
            if (nit.dropdownAnim > 0.001f || nit.dropdownOpen) {
                // flat negative pixel offset
                float reduced = layoutRounded - kDropdownExpansionOffset * s;
                if (reduced < closedBufInt)
                    reduced = closedBufInt;
                if (reduced < 0.0f)
                    reduced = 0.0f;
                height += reduced;
            }

        }

    }

    return height;
}

static ImU32 LerpColor(ImU32 a, ImU32 b, float t)
{
    ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
    ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);

    ImVec4 c(
        ImLerp(ca.x, cb.x, t),
        ImLerp(ca.y, cb.y, t),
        ImLerp(ca.z, cb.z, t),
        ImLerp(ca.w, cb.w, t)
    );

    return ImGui::ColorConvertFloat4ToU32(c);
}

static float AnimateTowards(float current, float target, float speed)
{
    return current + (target - current) * ImGui::GetIO().DeltaTime * speed;
}


void UIWindowBuilder::Render(bool wantVisible)
{
    m_fade.Tick(wantVisible);
    if (!m_fade.ShouldRender())
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_fade.Alpha());

    // header expand / collapse animation
    float target = m_headerOpen ? 1.0f : 0.0f;
    m_headerAnim += (target - m_headerAnim) *
        ImGui::GetIO().DeltaTime * 8.0f;
    m_headerAnim = ImClamp(m_headerAnim, 0.0f, 1.0f);

    float eased = EaseInOut(m_headerAnim);

    for (auto& it : m_items) {
        if (it.type == UIItemType::Toggle && it.toggle) {
            float pressedTarget = *it.toggle ? 1.0f : 0.0f;
            it.colorAnim += (pressedTarget - it.colorAnim) * ImGui::GetIO().DeltaTime * 10.0f;
            it.colorAnim = ImClamp(it.colorAnim, 0.0f, 1.0f);
        }

        if (it.type != UIItemType::ToggleDropdown)
            continue;

        float dropdownTarget = it.dropdownOpen ? 1.0f : 0.0f;
        // expand/collapse speed
        it.dropdownAnim += (dropdownTarget - it.dropdownAnim) * ImGui::GetIO().DeltaTime * 7.0f;
        it.dropdownAnim = ImClamp(it.dropdownAnim, 0.0f, 1.0f);

        float arrowTarget = it.dropdownOpen ? 1.0f : 0.0f;
        // arrow rotation speed
        it.arrowAnim += (arrowTarget - it.arrowAnim) * ImGui::GetIO().DeltaTime * 7.0f;
        it.arrowAnim = ImClamp(it.arrowAnim, 0.0f, 1.0f);
    }

    float s = Scale(1.0f);
    float collapsedHeight = (32.0f + 6.0f) * s;

    // Always call ComputeHeight to update mutable layoutHeight values (used by dropdowns)
    float computedHeight = ComputeHeight();

    float estimateHeight = m_lastContentHeight > 0.0f
        ? m_lastContentHeight
        : computedHeight;

    float animatedHeight =
        collapsedHeight + (estimateHeight - collapsedHeight) * eased;

    ImGui::SetNextWindowSize(
        ImVec2(m_width * s, animatedHeight),
        ImGuiCond_Always
    );


    const bool polished = LavenderHook::Globals::use_polished_overlay;

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse;

    if (!ImGui::Begin(m_title, nullptr, flags)) {
        ImGui::End();
        return;
    }
    float contentStartY = ImGui::GetCursorPosY();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    // header
    const float headerHeight = 32.0f * s;
    const float arrowWidth = 26.0f * s;
    const float iconSize = 18.0f * s;
    const float padding = 8.0f * s;

    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 title = ImGui::GetColorU32(ImGuiCol_TitleBg);
    ImU32 frame = ImGui::GetColorU32(ImGuiCol_FrameBg);

    ImU32 mixed = LerpColor(title, frame, 0.55f);

    float r = ImGui::GetStyle().WindowRounding;

    if (polished) {
        // top sheen
        dl->AddRectFilled(
            ImVec2(winPos.x, winPos.y),
            ImVec2(winPos.x + winSize.x, winPos.y + headerHeight),
            IM_COL32(255, 255, 255, (int)(9.f * m_fade.Alpha())),
            r, ImDrawFlags_RoundCornersTop);

        // accent divider under the header (window bg is frosted from theme)
        dl->AddLine(
            ImVec2(winPos.x + 1.0f, winPos.y + headerHeight),
            ImVec2(winPos.x + winSize.x - 1.0f, winPos.y + headerHeight),
            PolishedAccent(0.55f * m_fade.Alpha()), 1.0f);
    }
    else {
        // simple flat header bar
        dl->AddRectFilled(
            ImVec2(winPos.x, winPos.y),
            ImVec2(winPos.x + winSize.x, winPos.y + headerHeight),
            mixed,
            r,
            ImDrawFlags_RoundCornersTop
        );
    }

    // interaction zone
    ImGui::SetCursorScreenPos(winPos);
    ImGui::InvisibleButton("##header", ImVec2(winSize.x, headerHeight));

    ImGui::PopStyleVar();
    ImGui::SetCursorPosY(headerHeight);

    bool clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();
    bool overArrow =
        ImGui::GetIO().MousePos.x >= (winPos.x + winSize.x - arrowWidth);

    if (clicked && overArrow)
        m_headerOpen = !m_headerOpen;

    // drag window
    if (ImGui::IsItemActive() && !overArrow) {
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 d = ImGui::GetIO().MouseDelta;
        ImGui::SetWindowPos(ImVec2(pos.x + d.x, pos.y + d.y));
    }

    // left icon
    float leftPad = padding;
    if (m_headerIcon != 0) {
        ImVec2 p0(
            winPos.x + padding,
            winPos.y + (headerHeight - iconSize) * 0.5f
        );
        ImVec2 p1(p0.x + iconSize, p0.y + iconSize);

        ImVec4 iconTint = ImVec4(
            MAIN_RED.x,
            MAIN_RED.y,
            MAIN_RED.z,
            m_fade.Alpha()
        );

        const float bgInset = 1.0f;          // how much smaller than icon
        const float bgRounding = 4.0f;

        ImVec2 bg0(
            p0.x + bgInset,
            p0.y + bgInset
        );
        ImVec2 bg1(
            p1.x - bgInset,
            p1.y - bgInset
        );

        // white background plate
        dl->AddRectFilled(
            bg0,
            bg1,
            ImGui::GetColorU32(ImVec4(1, 1, 1, 0.95f * m_fade.Alpha())),
            bgRounding
        );


        dl->AddImage(
            m_headerIcon,
            p0,
            p1,
            ImVec2(0, 0),
            ImVec2(1, 1),
            ImGui::GetColorU32(iconTint)
        );
        leftPad += iconSize + padding;
    }

    // centered title
    ImVec2 textSize = ImGui::CalcTextSize(m_title);
    ImVec2 textPos(
        winPos.x + (winSize.x - textSize.x) * 0.5f,
        winPos.y + (headerHeight - textSize.y) * 0.5f
    );

    ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);

    if (polished) {
        // soft shadow + crisp text, matching the frosted overlay
        ImU32 sh = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.55f * m_fade.Alpha()));
        dl->AddText(ImVec2(textPos.x + 1, textPos.y + 1), sh, m_title);
        dl->AddText(textPos, col, m_title);
    }
    else {
        // draw text multiple times with tiny offsets
        dl->AddText(ImVec2(textPos.x + 1, textPos.y), col, m_title);
        dl->AddText(ImVec2(textPos.x - 1, textPos.y), col, m_title);
        dl->AddText(ImVec2(textPos.x, textPos.y + 1), col, m_title);
        dl->AddText(ImVec2(textPos.x, textPos.y - 1), col, m_title);
        dl->AddText(textPos, col, m_title);
    }


    // arrow anim
    float arrowTarget = m_headerOpen ? 1.0f : 0.0f;
    m_headerArrowAnim += (arrowTarget - m_headerArrowAnim) *
        ImGui::GetIO().DeltaTime * 10.0f;
    m_headerArrowAnim = ImClamp(m_headerArrowAnim, 0.0f, 1.0f);

    if (g_dropLeftTex) {
        float icon = headerHeight * 0.55f;
        constexpr float kHeaderArrowInset = 8.0f; // offset

        ImVec2 c(
            winPos.x + winSize.x - arrowWidth * 0.5f - kHeaderArrowInset,
            winPos.y + headerHeight * 0.5f
        );


        float a = -m_headerArrowAnim * IM_PI * 0.5f;
        float s = sinf(a), c2 = cosf(a);
        ImVec2 h(icon * 0.5f, icon * 0.5f);

        ImVec2 v[4] = {
            {-h.x,-h.y},{h.x,-h.y},{h.x,h.y},{-h.x,h.y}
        };

        for (auto& p : v)
            p = ImVec2(
                c.x + p.x * c2 - p.y * s,
                c.y + p.x * s + p.y * c2
            );

        dl->AddImageQuad(
            g_dropLeftTex,
            v[0], v[1], v[2], v[3],
            ImVec2(0, 0), ImVec2(1, 0),
            ImVec2(1, 1), ImVec2(0, 1),
            ImGui::GetColorU32(ImVec4(1, 1, 1, m_fade.Alpha()))
        );
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));


    {
        float headerAlpha = EaseInOut(m_headerAnim);
        float finalAlpha = headerAlpha * m_fade.Alpha();

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, finalAlpha);

        if (finalAlpha > 0.001f)
        {
            for (auto& it : m_items) {
                float itemSpacing = kRowSpacing * s;
                switch (it.type) {

                case UIItemType::Toggle:
                {
                    int hkBackup = it.hotkeyVK ? *it.hotkeyVK : 0;

                    bool before = it.toggle ? *it.toggle : false;
                    ToggleButton(it.label, it.toggle, ImVec2(kToggleWidth * s, 0));
                    bool after = it.toggle ? *it.toggle : false;
                    if (before != after) LavenderHook::Audio::PlayToggleSound(after);
                    ImGui::SameLine();

                    bool hkChanged = false;
                    if (it.hotkeyIndex >= 0) {
                        hkChanged = m_hotkeys[it.hotkeyIndex]
                            .Render(ImVec2(kHotkeyWidth * s, ImGui::GetFrameHeight()));
                        // Polished: top sheen on hotkey button
                        if (polished) {
                            ImVec2 rMin = ImGui::GetItemRectMin();
                            ImVec2 rMax = ImGui::GetItemRectMax();
                            dl->AddRectFilled(rMin, ImVec2(rMax.x, rMin.y + (rMax.y - rMin.y) * 0.50f),
                                IM_COL32(255, 255, 255, (int)(9.f * finalAlpha)),
                                ImGui::GetStyle().FrameRounding, ImDrawFlags_RoundCornersTop);
                        }
                    }

                    if (!hkChanged && it.hotkeyVK)
                        *it.hotkeyVK = hkBackup;

                    // Polished: light up toggle when active, smoothly
                    if (polished && it.toggle) {
                        float press = EaseInOut(it.colorAnim);
                        if (press > 0.01f) {
                            ImVec2 rMin = ImGui::GetItemRectMin();
                            ImVec2 rMax = ImGui::GetItemRectMax();
                            dl->AddRectFilled(rMin, rMax,
                                ImGui::GetColorU32(ImVec4(
                                    MAIN_RED.x, MAIN_RED.y, MAIN_RED.z,
                                    0.60f * press)),
                                ImGui::GetStyle().FrameRounding);
                        }
                    }
                }
                break;

                case UIItemType::ToggleDropdown:
                {
                    ImGui::PushID(&it);

                    const float arrowWidth = 18.0f * s;
                    const float height = ImGui::GetFrameHeight();
                    const float width = ImGui::GetContentRegionAvail().x;

                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    ImGui::InvisibleButton("##toggle_dd", ImVec2(width, height));
                    bool clicked = ImGui::IsItemClicked();

                    bool hovered = ImGui::IsItemHovered();
                    bool held = ImGui::IsItemActive();
                    bool active = *it.toggle;

                    float colorTarget =
                        held ? 1.0f :
                        active ? (hovered ? 0.55f : 0.45f) :
                        hovered ? 0.6f :
                        0.0f;


                    it.colorAnim = AnimateTowards(it.colorAnim, colorTarget, 10.0f);
                    float t = EaseInOut(it.colorAnim);


                    bool overArrow =
                        ImGui::GetIO().MousePos.x >=
                        (pos.x + width - arrowWidth - kDropdownArrowInset);

                    it.dropdownOpenNext = it.dropdownOpen;

                    if (clicked) {
                        if (overArrow) it.dropdownOpenNext = !it.dropdownOpen;
                        else {
                            bool before = it.toggle ? *it.toggle : false;
                            *it.toggle = !*it.toggle;
                            bool after = it.toggle ? *it.toggle : false;
                            if (before != after) LavenderHook::Audio::PlayToggleSound(after);
                        }
                    }

                    ImDrawList* dl = ImGui::GetWindowDrawList();

                    if (polished) {
                        float rr = ImGui::GetStyle().FrameRounding;
                        // Drop shadow + frosted fill + top sheen + accent border
                        PolishedPanel(dl, pos,
                            ImVec2(pos.x + width, pos.y + height),
                            rr, finalAlpha, false, height * 0.5f);

                        // Colored highlight when active/held, smoothly animated
                        if (t > 0.01f) {
                            dl->AddRectFilled(pos,
                                ImVec2(pos.x + width, pos.y + height),
                                ImGui::GetColorU32(ImVec4(
                                    MAIN_RED.x, MAIN_RED.y, MAIN_RED.z,
                                    0.60f * t)),
                                rr);
                        }
                    }
                    else {
                        ImU32 baseCol = ImGui::GetColorU32(ImGuiCol_FrameBg);
                        ImU32 hoverCol = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);
                        ImU32 activeCol = ImGui::GetColorU32(ImGuiCol_ButtonActive);

                        ImU32 highlightCol = active ? activeCol : hoverCol;
                        ImU32 bg = LerpColor(baseCol, highlightCol, t);

                        ImGui::RenderFrame(pos,
                            ImVec2(pos.x + width, pos.y + height),
                            bg, true, ImGui::GetStyle().FrameRounding);
                    }

                    if (WINDOW_BORDER_SIZE > 0.0f && !polished)
                    {
                        dl->AddRect(
                            pos,
                            ImVec2(pos.x + width, pos.y + height),
                            ImGui::GetColorU32(ImGuiCol_Border),
                            ImGui::GetStyle().FrameRounding,
                            0,
                            WINDOW_BORDER_SIZE
                        );

                        float dividerX = pos.x + width - arrowWidth - kDropdownArrowInset;

                        dl->AddLine(
                            ImVec2(dividerX, pos.y + 4.0f),
                            ImVec2(dividerX, pos.y + height - 4.0f),
                            ImGui::GetColorU32(ImGuiCol_Border),
                            WINDOW_BORDER_SIZE
                        );
                    }

                    {
                        ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);
                        if (polished && t > 0.01f) {
                            float dim = 1.0f - 0.30f * t;
                            ImVec4 tc = ImGui::ColorConvertU32ToFloat4(textCol);
                            tc.x *= dim; tc.y *= dim; tc.z *= dim;
                            textCol = ImGui::GetColorU32(tc);
                        }
                        dl->AddText(
                            ImVec2(pos.x + 8.0f * s,
                                pos.y + (height - ImGui::GetFontSize()) * 0.5f),
                            textCol, it.label);
                    }


                    if (g_dropLeftTex) {
                        float iconSize = height * kDropdownArrowScale;

                        ImVec2 center(
                            pos.x + width - arrowWidth * 0.5f - kDropdownArrowInset,
                            pos.y + height * 0.5f
                        );

                        float angle = -it.arrowAnim * IM_PI * 0.5f;

                        ImVec2 half(iconSize * 0.5f, iconSize * 0.5f);
                        ImVec2 corners[4] = {
                            {-half.x, -half.y},
                            { half.x, -half.y},
                            { half.x,  half.y},
                            {-half.x,  half.y}
                        };

                        float s = sinf(angle);
                        float c = cosf(angle);

                        for (int i = 0; i < 4; i++) {
                            float x = corners[i].x;
                            float y = corners[i].y;
                            corners[i] = ImVec2(
                                center.x + x * c - y * s,
                                center.y + x * s + y * c
                            );
                        }

                        dl->AddImageQuad(
                            g_dropLeftTex,
                            corners[0], corners[1],
                            corners[2], corners[3],
                            ImVec2(0, 0), ImVec2(1, 0),
                            ImVec2(1, 1), ImVec2(0, 1),
                            ImGui::GetColorU32(ImVec4(1, 1, 1, m_fade.Alpha()))
                        );
                    }

                    it.dropdownOpen = it.dropdownOpenNext;

                    int rows =
                        (it.hotkeyIndex >= 0 ? 1 : 0) +
                        (int)it.timings.size();

                    const float rowFadeTime = 0.14f;
                    const float rowStagger = 0.07f;
                    const float fadeOutTime = 0.08f;

                    if (it.dropdownOpen)
                        it.dropdownFade += ImGui::GetIO().DeltaTime;
                    else
                        it.dropdownFade -= ImGui::GetIO().DeltaTime * (rowFadeTime / fadeOutTime);

                    it.dropdownFade = ImClamp(
                        it.dropdownFade,
                        0.0f,
                        rowFadeTime + (rows - 1) * rowStagger
                    );

                    float layoutT = EaseInOut(it.dropdownAnim);

                    float totalSpan = rowFadeTime + (rows - 1) * rowStagger;
                    if (it.dropdownOpen) {
                        float desired = layoutT * totalSpan;
                        if (it.dropdownFade < desired)
                            it.dropdownFade = desired;
                    }

                    constexpr float kHideThreshold = 0.06f;
                    bool visible = layoutT > kHideThreshold;

                    float rowH = ImGui::GetFrameHeight();
                    float rowHInt = std::round(rowH);
                    float spacingInt = std::round(kDropdownRowSpacing * s);
                    float fullHeight = rows * rowHInt + (rows > 0 ? (rows - 1) * spacingInt : 0.0f);
                    float childHeight = it.layoutHeight;
                    float childHeightRounded = (!it.dropdownOpen) ? std::ceil(childHeight) : std::round(childHeight);
                    childHeightRounded = std::min(childHeightRounded, fullHeight);
                    float closedBufInt = std::round(kDropdownClosedBuffer * s);

                    ImGui::Indent(12.0f * s);

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

                    float childContainerHeight = std::max(childHeightRounded, closedBufInt);
                    ImGui::BeginChild(
                        "##dropdown_child",
                        ImVec2(0.0f, childContainerHeight),
                        false,
                        ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse |
                        ImGuiWindowFlags_NoNav
                    );
                    ImGui::SetScrollY(0.0f);

                    int visualIndex = 0;
                    int totalRows = rows;

                    auto RowAlpha = [&](int index) -> float
                        {
                            if (it.dropdownOpen) {
                                float start = index * rowStagger;
                                float t = (it.dropdownFade - start) / rowFadeTime;

                                if (it.dropdownFade < 0.001f) {
                                    float totalSpan = rowFadeTime + (rows - 1) * rowStagger;
                                    float proxy = layoutT * totalSpan;
                                    t = (proxy - start) / rowFadeTime;
                                }
                                return EaseInOut(ImClamp(t, 0.0f, 1.0f));
                            }
                            else {
                                float t = it.dropdownFade / rowFadeTime;
                                return EaseInOut(ImClamp(t, 0.0f, 1.0f));
                            }
                        };

                    const float labelX = ImGui::GetCursorPosX();
                    const float controlX = labelX + kTimingControlX * s;

                    if (it.hotkeyIndex >= 0) {
                        float a = RowAlpha(visualIndex++) * finalAlpha;
                        if (a > 0.0f) {
                            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, a);
                            float rowStartY = ImGui::GetCursorPosY();
                            ImGui::AlignTextToFramePadding();
                            ImGui::TextUnformatted("Hotkey:");
                            ImGui::SameLine();
                            ImGui::SetCursorPosX(controlX);
                            m_hotkeys[it.hotkeyIndex]
                                .Render(ImVec2(kHotkeyWidth * s, ImGui::GetFrameHeight()));
                            ImGui::PopStyleVar();
                            ImGui::SetCursorPosY(rowStartY + rowHInt);
                            // add spacing between rows but not after last row
                            if (visualIndex < totalRows)
                                ImGui::Dummy(ImVec2(0.0f, spacingInt));
                        }
                    }

                    for (auto& tr : it.timings) {
                        float a = RowAlpha(visualIndex++) * finalAlpha;
                        if (a > 0.0f) {
                            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, a);
                            float rowStartY = ImGui::GetCursorPosY();

                            if (!tr.isCheckbox) {
                                ImGui::AlignTextToFramePadding();
                                ImGui::TextUnformatted(tr.label);

                                ImGui::SameLine();
                                ImGui::SetCursorPosX(controlX);
                            }

                            if (tr.sliderFloat) {
                                ImGui::SetNextItemWidth(kTimingSliderWidth * s);
                                ImGui::SliderFloat(
                                    ("##" + std::string(tr.label)).c_str(),
                                    tr.sliderFloat,
                                    tr.sliderMin,
                                    tr.sliderMax,
                                    tr.sliderFmt
                                );
                                float wheel = ImGui::GetIO().MouseWheel;
                                if (wheel != 0.0f && ImGui::IsItemHovered())
                                {
                                    *tr.sliderFloat += wheel * 0.01f;
                                }
                                *tr.sliderFloat = ImClamp(*tr.sliderFloat, tr.sliderMin, tr.sliderMax);
                            }
                            else if (tr.isHotkey) {
                                if (tr.hotkeyVK) {
                                    // make sure hotkey instance is bound to the correct storage
                                    tr.hotkey.keyVK = tr.hotkeyVK;
                                    bool hkChanged = tr.hotkey.Render(ImVec2(kHotkeyWidth * s, ImGui::GetFrameHeight()));
                                    if (hkChanged)
                                        *tr.hotkeyVK = *tr.hotkey.keyVK;
                                }
                            }
                            else if (tr.isCheckbox) {
                                ImGui::AlignTextToFramePadding();
                                ImGui::TextUnformatted(tr.label);
                                ImGui::SameLine();
                                ImGui::SetCursorPosX(controlX);
                                ImGui::Checkbox(("##" + std::string(tr.label)).c_str(), tr.checkboxValue);
                            }
                            else {
                                ImGui::SetNextItemWidth(kTimingSliderWidth * s);
                                if (tr.asIntInput) {
                                    ImGui::InputInt(
                                        ("##" + std::string(tr.label)).c_str(),
                                        tr.valueMs
                                    );
                                    *tr.valueMs = ImClamp(*tr.valueMs, tr.minMs, tr.maxMs);
                                }
                                else if (tr.asSeconds) {
                                    float sec = *tr.valueMs / 1000.0f;
                                    if (ImGui::SliderFloat(
                                        ("##" + std::string(tr.label)).c_str(),
                                        &sec,
                                        tr.minMs / 1000.0f,
                                        tr.maxMs / 1000.0f,
                                        "%.1fs"))
                                    {
                                        sec = std::round(sec * 10.0f) / 10.0f;
                                        *tr.valueMs = (int)(sec * 1000.0f);
                                    }
                                    float wheel = ImGui::GetIO().MouseWheel;
                                    if (wheel != 0.0f && ImGui::IsItemHovered()) {
                                        sec += wheel * 0.1f;
                                        sec = std::round(sec * 10.0f) / 10.0f;
                                        sec = ImClamp(sec, tr.minMs / 1000.0f, tr.maxMs / 1000.0f);
                                        *tr.valueMs = (int)(sec * 1000.0f);
                                    }
                                }
                                else {
                                    ImGui::SliderInt(
                                        ("##" + std::string(tr.label)).c_str(),
                                        tr.valueMs,
                                        tr.minMs,
                                        tr.maxMs,
                                        "%dms"
                                    );
                                    float wheel = ImGui::GetIO().MouseWheel;
                                    if (wheel != 0.0f && ImGui::IsItemHovered()) {
                                        *tr.valueMs += (int)(wheel > 0 ? 10 : -10);
                                        *tr.valueMs = ImClamp(*tr.valueMs, tr.minMs, tr.maxMs);
                                    }
                                }
                            }

                            ImGui::PopStyleVar();
                            ImGui::SetCursorPosY(rowStartY + rowHInt);
                            // add spacing between rows
                            if (visualIndex < totalRows)
                                ImGui::Dummy(ImVec2(0.0f, spacingInt));
                        }
                    }

                    ImGui::EndChild();
                    if (childContainerHeight > 0.5f)
                        itemSpacing = 0.0f;
                    else
                        itemSpacing = kDropdownClosedBuffer;
                    ImGui::PopStyleVar();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                    ImGui::Unindent(12.0f * s);

                    // Tooltip
                    bool rightDown = ImGui::IsMouseDown(1);
                    ImVec2 mouse = ImGui::GetIO().MousePos;
                    float expandedHeight = childHeightRounded;
                    // area: from pos.y to pos.y + height + expandedHeight
                    bool mouseInArea = mouse.x >= pos.x && mouse.x <= pos.x + width && mouse.y >= pos.y && mouse.y <= pos.y + height + expandedHeight;
                    bool wantTooltip = (ImGui::IsItemActive() || mouseInArea) && rightDown && it.description;

                    float tooltipSpeed = 6.0f;
                    it.tooltipFade += (wantTooltip ? 1.0f : -1.0f) * ImGui::GetIO().DeltaTime * tooltipSpeed;
                    it.tooltipFade = ImClamp(it.tooltipFade, 0.0f, 1.0f);

                    if (it.tooltipFade > 0.001f)
                    {
                        ImVec2 mouse = ImGui::GetIO().MousePos;
                        ImVec2 textSize = ImGui::CalcTextSize(it.description);
                        ImVec2 padding = ImGui::GetStyle().WindowPadding;
                        ImVec2 boxMin(mouse.x + 12, mouse.y + 12);
                        ImVec2 boxMax(boxMin.x + textSize.x + padding.x * 2, boxMin.y + textSize.y + padding.y * 2);

                        ImU32 bg = ImGui::GetColorU32(ImGuiCol_PopupBg);
                        ImU32 fg = ImGui::GetColorU32(ImGuiCol_Text);

                        ImDrawList* dl = ImGui::GetForegroundDrawList();
                        float a = it.tooltipFade;

                        ImVec2 display = ImGui::GetIO().DisplaySize;
                        ImVec2 size(textSize.x + padding.x * 2, textSize.y + padding.y * 2);
                        const float edgePad = 8.0f;

                        ImVec2 preferBoxMin(boxMin);
                        ImVec2 preferBoxMax(boxMin.x + size.x, boxMin.y + size.y);

                        if (preferBoxMax.x > display.x - edgePad)
                        {
                            preferBoxMin.x = mouse.x - 12 - size.x;
                            preferBoxMax.x = preferBoxMin.x + size.x;
                        }
                        if (preferBoxMax.y > display.y - edgePad)
                        {
                            preferBoxMin.y = mouse.y - 12 - size.y;
                            preferBoxMax.y = preferBoxMin.y + size.y;
                        }

                        // make sure inside display
                        preferBoxMin.x = ImClamp(preferBoxMin.x, edgePad, display.x - edgePad - size.x);
                        preferBoxMin.y = ImClamp(preferBoxMin.y, edgePad, display.y - edgePad - size.y);
                        preferBoxMax = ImVec2(preferBoxMin.x + size.x, preferBoxMin.y + size.y);

                        // draw shadow (polished only)
                        if (polished) {
                            dl->AddRectFilled(
                                ImVec2(preferBoxMin.x - 4.f * s, preferBoxMin.y + 2.f * s),
                                ImVec2(preferBoxMax.x, preferBoxMax.y + 5.f * s),
                                IM_COL32(0, 0, 0, (int)(85.f * a)), 6.0f);
                        }

                        // draw background with alpha
                        ImVec4 bgf = ImGui::ColorConvertU32ToFloat4(bg);
                        bgf.w *= a;
                        dl->AddRectFilled(preferBoxMin, preferBoxMax, ImGui::GetColorU32(bgf), 6.0f);

                        // draw border (polished only)
                        if (polished) {
                            ImU32 borderCol = ImGui::GetColorU32(ImGuiCol_Border);
                            ImVec4 bf = ImGui::ColorConvertU32ToFloat4(borderCol);
                            bf.w *= a * 0.7f;
                            dl->AddRect(preferBoxMin, preferBoxMax, ImGui::GetColorU32(bf), 6.0f, 0, 1.0f);
                        }

                        ImVec2 textPos(preferBoxMin.x + padding.x, preferBoxMin.y + padding.y);
                        ImVec4 fgf = ImGui::ColorConvertU32ToFloat4(fg);
                        fgf.w *= a;
                        dl->AddText(textPos, ImGui::GetColorU32(fgf), it.description);
                    }

                    ImGui::PopID();
                    break;
                }

                case UIItemType::SliderInt:
                    SliderInt(it.label, it.sliderInt, it.min, it.max);
                    if (polished) {
                        ImVec2 rMin = ImGui::GetItemRectMin();
                        ImVec2 rMax = ImGui::GetItemRectMax();
                        dl->AddRectFilled(rMin, ImVec2(rMax.x, rMin.y + (rMax.y - rMin.y) * 0.50f),
                            IM_COL32(255, 255, 255, (int)(9.f * finalAlpha)),
                            ImGui::GetStyle().FrameRounding, ImDrawFlags_RoundCornersTop);
                    }
                    {
                        float wheel = ImGui::GetIO().MouseWheel;
                        if (wheel != 0.0f && ImGui::IsItemHovered()) {
                            *it.sliderInt += (int)(wheel > 0 ? 1 : -1);
                            *it.sliderInt = ImClamp(*it.sliderInt, it.min, it.max);
                        }
                    }
                    break;

                case UIItemType::SliderFloat:
                    SliderFloat(it.label, it.sliderFloat, it.minF, it.maxF, "%.2f");
                    if (polished) {
                        ImVec2 rMin = ImGui::GetItemRectMin();
                        ImVec2 rMax = ImGui::GetItemRectMax();
                        dl->AddRectFilled(rMin, ImVec2(rMax.x, rMin.y + (rMax.y - rMin.y) * 0.50f),
                            IM_COL32(255, 255, 255, (int)(9.f * finalAlpha)),
                            ImGui::GetStyle().FrameRounding, ImDrawFlags_RoundCornersTop);
                    }
                    {
                        float wheel = ImGui::GetIO().MouseWheel;
                        if (wheel != 0.0f && ImGui::IsItemHovered()) {
                            *it.sliderFloat += wheel * (it.maxF - it.minF) * 0.01f;
                            *it.sliderFloat = ImClamp(*it.sliderFloat, it.minF, it.maxF);
                        }
                    }
                    break;

                case UIItemType::Button:
                {
                    bool btnHeld = false; // track via ImGui
                    if (Button(it.label, ImVec2(kButtonWidth * s, 0)) && it.onClick)
                        it.onClick();
                    if (polished && ImGui::IsItemActive()) {
                        ImVec2 rMin = ImGui::GetItemRectMin();
                        ImVec2 rMax = ImGui::GetItemRectMax();
                        dl->AddRectFilled(rMin, rMax,
                            ImGui::GetColorU32(ImVec4(
                                MAIN_RED.x, MAIN_RED.y, MAIN_RED.z,
                                0.60f * finalAlpha)),
                            ImGui::GetStyle().FrameRounding);
                    }
                    break;
                }
                }
                ImGui::Dummy(ImVec2(0, itemSpacing));
            }
        }
        ImGui::PopStyleVar();
    }
    float contentEndY = ImGui::GetCursorPosY();
    float actualContent = ((contentEndY - contentStartY) + kBaseHeight * s) * eased;
    if (m_lastContentHeight == 0.0f)
        m_lastContentHeight = actualContent;
    m_lastContentHeight += (actualContent - m_lastContentHeight) * ImGui::GetIO().DeltaTime * 20.0f;
    if (fabsf(m_lastContentHeight - actualContent) < 0.5f)
        m_lastContentHeight = actualContent;

    ImVec2 sbPos = ImGui::GetWindowPos();
    ImVec2 sbSize = ImGui::GetWindowSize();
    ImGui::End();
    if (polished)
        Lavender::DrawWindowShadow(sbPos, sbSize, m_fade.Alpha());
    ImGui::PopStyleVar();
}

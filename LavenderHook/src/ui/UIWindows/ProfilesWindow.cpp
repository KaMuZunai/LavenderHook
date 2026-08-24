#include "ProfilesWindow.h"
#include "../../misc/Globals.h"
#include "../../config/ConfigManager.h"
#include "../../imgui/imgui.h"
#include "../components/LavenderFadeOut.h"
#include "../components/LavenderWindowHeader.h"
#include "../components/LavenderUI.h"
#include "../components/LavenderHotkey.h"
#include "../functions/FunctionRegistry.h"
#include "../../assets/UITextures.h"
#include "../../sound/SoundPlayer.h"

#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <cstring>

extern ImVec4 MAIN_RED;
extern ImVec4 MID_RED;

namespace LavenderHook::UI::Windows {

namespace {

using LavenderHook::UI::Lavender::Hotkey;

struct Profile {
    std::string name;
    int hotkeyVK = 0;
    Hotkey hotkey = {};
    std::vector<std::string> enabledFunctions;
    bool active = false;
    float tooltipFade = 0.f;
    float renTooltipFade = 0.f;
    float delTooltipFade = 0.f;
};

static std::vector<Profile> s_profiles;
static bool s_loaded = false;

static LavenderHook::UI::LavenderFadeOut s_fade;
static bool  s_headerOpen = true;
static float s_headerAnim = 1.0f;
static float s_arrowAnim = 1.0f;
static float s_contentAnim = 0.0f;

static int  s_renamingIdx = -1;
static char s_renameBuffer[128] = {};
static bool s_renameFocus = false;

static bool  s_addingNew = false;
static char  s_addNameBuffer[128] = {};
static bool  s_addFocus = false;
static float s_addTooltipFade = 0.f;
static int   s_confirmDeleteIdx = -1;

static std::string GetProfilesPath()
{
    return Config::GetBaseDir() + "\\Profiles.ini";
}

static void SaveCurrentFunctionsToProfile(Profile& p)
{
    p.enabledFunctions.clear();
    auto& reg = FunctionRegistry::Instance();
    for (const auto& e : reg.GetAll()) {
        if (e.pEnabled && *e.pEnabled)
            p.enabledFunctions.push_back(e.name);
    }
}

static void ApplyProfileFunctions(const Profile& p, bool enable)
{
    auto& reg = FunctionRegistry::Instance();
    for (const auto& name : p.enabledFunctions) {
        bool* ptr = reg.Find(name);
        if (ptr) *ptr = enable;
    }
}

static bool IsProfileActive(const Profile& p)
{
    auto& reg = FunctionRegistry::Instance();
    for (const auto& name : p.enabledFunctions) {
        bool* ptr = reg.Find(name);
        if (!ptr || !*ptr) return false;
    }
    return !p.enabledFunctions.empty();
}

static void ToggleProfile(Profile& p)
{
    bool enable = !IsProfileActive(p);
    ApplyProfileFunctions(p, enable);
    LavenderHook::Audio::PlayToggleSound(enable);
}

static void SaveProfiles()
{
    std::ofstream f(GetProfilesPath(), std::ios::trunc);
    if (!f) return;

    f << "[Meta]\n";
    f << "count=" << s_profiles.size() << "\n\n";

    for (size_t i = 0; i < s_profiles.size(); ++i) {
        const auto& p = s_profiles[i];
        f << "[Profile_" << i << "]\n";
        f << "name=" << p.name << "\n";
        f << "hotkey=" << p.hotkeyVK << "\n";
        f << "fn_count=" << p.enabledFunctions.size() << "\n";
        for (size_t j = 0; j < p.enabledFunctions.size(); ++j)
            f << "fn_" << j << "=" << p.enabledFunctions[j] << "\n";
        f << "\n";
    }
}

static void LoadProfiles()
{
    s_profiles.clear();

    std::ifstream f(GetProfilesPath());
    if (!f) return;

    std::unordered_map<std::string,
        std::unordered_map<std::string, std::string>> sections;

    std::string currentSection;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if (line.front() == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.size() - 2);
        } else {
            auto eq = line.find('=');
            if (eq != std::string::npos)
                sections[currentSection][line.substr(0, eq)] = line.substr(eq + 1);
        }
    }

    auto getString = [&](const std::string& sec, const std::string& key,
                         const std::string& def = "") -> std::string {
        auto sit = sections.find(sec);
        if (sit == sections.end()) return def;
        auto kit = sit->second.find(key);
        return kit != sit->second.end() ? kit->second : def;
    };

    auto getInt = [&](const std::string& sec, const std::string& key,
                      int def = 0) -> int {
        const std::string s = getString(sec, key, "");
        return s.empty() ? def : std::atoi(s.c_str());
    };

    const int count = getInt("Meta", "count", 0);
    for (int i = 0; i < count; ++i) {
        const std::string sec = "Profile_" + std::to_string(i);
        Profile p;
        p.name = getString(sec, "name", "Profile " + std::to_string(i + 1));
        p.hotkeyVK = getInt(sec, "hotkey", 0);
        const int fnCount = getInt(sec, "fn_count", 0);
        for (int j = 0; j < fnCount; ++j) {
            std::string fn = getString(sec, "fn_" + std::to_string(j), "");
            if (!fn.empty())
                p.enabledFunctions.push_back(fn);
        }
        s_profiles.push_back(std::move(p));
    }
}

static float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

static void DrawTooltip(const char* text, float alpha)
{
    if (alpha <= 0.001f) return;

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const ImVec2 tSize = ImGui::CalcTextSize(text);
    const ImVec2 padding = ImGui::GetStyle().WindowPadding;
    const ImVec2 size(tSize.x + padding.x * 2.f, tSize.y + padding.y * 2.f);

    ImVec2 boxMin(mouse.x + 12.f, mouse.y + 12.f);
    ImVec2 boxMax(boxMin.x + size.x, boxMin.y + size.y);

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float edgePad = 8.f;

    if (boxMax.x > display.x - edgePad) { boxMin.x = mouse.x - 12.f - size.x; boxMax.x = boxMin.x + size.x; }
    if (boxMax.y > display.y - edgePad) { boxMin.y = mouse.y - 12.f - size.y; boxMax.y = boxMin.y + size.y; }

    boxMin.x = boxMin.x < edgePad ? edgePad : (boxMin.x > display.x - edgePad - size.x ? display.x - edgePad - size.x : boxMin.x);
    boxMin.y = boxMin.y < edgePad ? edgePad : (boxMin.y > display.y - edgePad - size.y ? display.y - edgePad - size.y : boxMin.y);
    boxMax = ImVec2(boxMin.x + size.x, boxMin.y + size.y);

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    ImVec4 bgf = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_PopupBg));
    bgf.w *= alpha;
    dl->AddRectFilled(boxMin, boxMax, ImGui::GetColorU32(bgf), 6.f);

    ImVec4 fgf = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Text));
    fgf.w *= alpha;
    dl->AddText(ImVec2(boxMin.x + padding.x, boxMin.y + padding.y), ImGui::GetColorU32(fgf), text);
}

static bool AnimatedButton(
    const char* label,
    const char* animKey,
    const ImVec2& size,
    const ImVec4& colBase,
    const ImVec4& colHov,
    const ImVec4& colAct,
    float speed = 10.f,
    float activeLevel = 0.f)
{
    ImGuiStorage* store = ImGui::GetStateStorage();
    const ImGuiID animID = ImGui::GetID(animKey);
    float anim = store->GetFloat(animID, 0.f);

    if (LavenderHook::Globals::use_polished_overlay)
    {
        ImGui::PushID(animKey);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 btnSize = ImVec2(
            size.x > 0.f ? size.x : ImGui::GetContentRegionAvail().x,
            size.y > 0.f ? size.y : ImGui::GetFrameHeight());
        ImGui::InvisibleButton("##ab", btnSize);
        const bool pressed = ImGui::IsItemClicked();

        const float target = ImGui::IsItemActive() ? 1.f
            : ImGui::IsItemHovered() ? 0.6f : 0.f;
        anim = Clamp01(anim + (target - anim) * ImGui::GetIO().DeltaTime * speed);
        store->SetFloat(animID, anim);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p1 = ImVec2(pos.x + btnSize.x, pos.y + btnSize.y);
        float rr = ImGui::GetStyle().FrameRounding;

        float styleAlpha = ImGui::GetStyle().Alpha;
        LavenderHook::UI::Lavender::PolishedPanel(
            dl, pos, p1, rr, styleAlpha, false, btnSize.y * 0.5f);

        float hlAmt = anim > activeLevel ? anim : activeLevel;
        if (hlAmt > 0.01f) {
            ImVec4 hl = colHov;
            hl.w *= styleAlpha * hlAmt;
            dl->AddRectFilled(pos, p1, ImGui::GetColorU32(hl), rr);
        }

        const char* hash = strstr(label, "##");
        const char* textEnd = hash ? hash : label + strlen(label);
        ImVec2 ts = ImGui::CalcTextSize(label, textEnd);
        ImVec4 txtCol = ImGui::GetStyle().Colors[ImGuiCol_Text];
        txtCol.w *= styleAlpha;
        dl->AddText(ImVec2(pos.x + (btnSize.x - ts.x) * 0.5f,
            pos.y + (btnSize.y - ts.y) * 0.5f),
            ImGui::GetColorU32(txtCol), label, textEnd);

        ImGui::PopID();
        return pressed;
    }

    const ImVec4 blended{
        colBase.x + (colHov.x - colBase.x) * anim,
        colBase.y + (colHov.y - colBase.y) * anim,
        colBase.z + (colHov.z - colBase.z) * anim,
        colBase.w + (colHov.w - colBase.w) * anim
    };

    ImGui::PushStyleColor(ImGuiCol_Button, blended);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, blended);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, blended);

    const bool pressed = ImGui::Button(label, size);

    const float target = ImGui::IsItemActive() ? 1.f
        : ImGui::IsItemHovered() ? 0.6f : 0.f;
    store->SetFloat(animID, Clamp01(anim + (target - anim) * ImGui::GetIO().DeltaTime * speed));

    ImGui::PopStyleColor(3);
    return pressed;
}

} // anonymous namespace

void RenderProfilesWindow(bool wantVisible)
{
    if (!s_loaded) {
        LoadProfiles();
        s_loaded = true;
    }

    s_fade.Tick(wantVisible);
    if (!s_fade.ShouldRender()) return;

    const float alpha = s_fade.Alpha();

    {
        const float target = s_headerOpen ? 1.f : 0.f;
        s_headerAnim += (target - s_headerAnim) * ImGui::GetIO().DeltaTime * 8.f;
        s_headerAnim = Clamp01(s_headerAnim);
    }

    float s = LavenderHook::Globals::menu_scale;
    const float kWidth = 290.f * s;
    const float kRowH = 32.f * s;
    const float kSpacing = 4.f * s;
    const float kHeaderH = 36.f * s;

    ImGui::SetNextWindowSize(ImVec2(kWidth, kHeaderH + 100.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(740.f * s, 60.f * s), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse;

    if (!ImGui::Begin("##ProfilesWin", nullptr, kFlags))
    {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    LavenderHook::UI::Lavender::RenderWindowHeader(
        "Profiles",
        g_sparkleIcoTex,
        g_dropLeftTex,
        kWidth, alpha,
        s_headerOpen, s_headerAnim, s_arrowAnim);

    const float contentStartY = ImGui::GetCursorPosY();

    if (s_headerAnim > 0.001f)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha * s_headerAnim);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f * s, kSpacing));

        const float innerW = kWidth - ImGui::GetStyle().WindowPadding.x * 2.f;
        const float xBtnW = 22.f * s;
        const float renBtnW = 22.f * s;
        const float hkBtnW = 50.f * s;
        const float btnGap = 4.f * s;
        const float nameBtnW = innerW - hkBtnW - btnGap - renBtnW - btnGap - xBtnW - btnGap;

        bool triggerConfirm = false;

        for (int i = 0; i < static_cast<int>(s_profiles.size()); ++i)
        {
            auto& p = s_profiles[i];
            ImGui::PushID(i);

            p.hotkey.keyVK = &p.hotkeyVK;
            {
                bool wasActive = IsProfileActive(p);
                bool newActive = wasActive;
                p.hotkey.UpdateToggle(newActive);
                if (newActive != wasActive)
                    ToggleProfile(p);
            }

            if (s_renamingIdx == i)
            {
                if (s_renameFocus) {
                    ImGui::SetKeyboardFocusHere();
                    s_renameFocus = false;
                }

                ImGui::SetNextItemWidth(nameBtnW);
                if (ImGui::InputText("##ren_in", s_renameBuffer, sizeof(s_renameBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue |
                    ImGuiInputTextFlags_AutoSelectAll))
                {
                    if (s_renameBuffer[0] != '\0')
                        p.name = s_renameBuffer;
                    s_renamingIdx = -1;
                    SaveProfiles();
                }

                if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                    s_renamingIdx = -1;
            }
            else
            {
                const bool   active  = IsProfileActive(p);
                const ImVec4 colBase = active
                    ? ImVec4(MAIN_RED.x,        MAIN_RED.y,        MAIN_RED.z,        0.70f)
                    : ImVec4(0.12f,             0.12f,             0.12f,             0.90f);
                const ImVec4 colHov  = active
                    ? ImVec4(MAIN_RED.x * 0.80f, MAIN_RED.y * 0.80f, MAIN_RED.z * 0.80f, 0.65f)
                    : ImVec4(MAIN_RED.x,          MAIN_RED.y,          MAIN_RED.z,          0.55f);

                if (AnimatedButton(p.name.c_str(), "##pa", ImVec2(nameBtnW, kRowH),
                    colBase, colHov, ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.90f),
                    10.f, active ? 0.85f : 0.f))
                    ToggleProfile(p);

                // Tooltip on right click hold
                {
                    const ImVec2 bMin  = ImGui::GetItemRectMin();
                    const ImVec2 bMax  = ImGui::GetItemRectMax();
                    const ImVec2 mouse = ImGui::GetIO().MousePos;
                    const bool   want  = mouse.x >= bMin.x && mouse.x <= bMax.x
                                      && mouse.y >= bMin.y && mouse.y <= bMax.y
                                      && ImGui::IsMouseDown(ImGuiMouseButton_Right);

                    p.tooltipFade += (want ? 1.f : -1.f) * ImGui::GetIO().DeltaTime * 6.f;
                    p.tooltipFade  = Clamp01(p.tooltipFade);

                    if (p.tooltipFade > 0.001f)
                    {
                        std::string tip = "Profile \"" + p.name + "\" includes:\n";
                        if (p.enabledFunctions.empty())
                            tip += "  (none saved)";
                        else
                        {
                            tip += "Functions:\n";
                            for (const auto& fn : p.enabledFunctions)
                                tip += "  " + fn + "\n";
                        }
                        DrawTooltip(tip.c_str(), p.tooltipFade);
                    }
                }
            }

            // Hotkey button
            ImGui::SameLine();
            p.hotkey.Render(ImVec2(hkBtnW, kRowH));

            // Rename button
            ImGui::SameLine();
            if (AnimatedButton("R##ren_btn", "##ra", ImVec2(renBtnW, kRowH),
                ImVec4(0.10f, 0.10f, 0.10f, 0.85f),
                ImVec4(0.25f, 0.25f, 0.25f, 0.90f),
                ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.70f)))
            {
                if (s_renamingIdx == i)
                {
                    if (s_renameBuffer[0] != '\0')
                        p.name = s_renameBuffer;
                    s_renamingIdx = -1;
                    SaveProfiles();
                }
                else
                {
                    s_renamingIdx = i;
                    s_renameFocus = true;
                    strncpy_s(s_renameBuffer, sizeof(s_renameBuffer), p.name.c_str(), _TRUNCATE);
                }
            }
            {
                const ImVec2 bMin  = ImGui::GetItemRectMin();
                const ImVec2 bMax  = ImGui::GetItemRectMax();
                const ImVec2 mouse = ImGui::GetIO().MousePos;
                const bool   want  = mouse.x >= bMin.x && mouse.x <= bMax.x
                                  && mouse.y >= bMin.y && mouse.y <= bMax.y
                                  && ImGui::IsMouseDown(ImGuiMouseButton_Right);
                p.renTooltipFade += (want ? 1.f : -1.f) * ImGui::GetIO().DeltaTime * 6.f;
                p.renTooltipFade  = Clamp01(p.renTooltipFade);
                if (p.renTooltipFade > 0.001f)
                    DrawTooltip("Rename", p.renTooltipFade);
            }

            // Delete button
            ImGui::SameLine();
            if (AnimatedButton("X##del", "##da", ImVec2(xBtnW, kRowH),
                ImVec4(0.45f, 0.07f, 0.07f, 0.80f),
                ImVec4(0.62f, 0.10f, 0.10f, 0.95f),
                ImVec4(0.75f, 0.13f, 0.13f, 1.00f)))
            {
                s_confirmDeleteIdx = i;
                triggerConfirm = true;
            }
            {
                const ImVec2 bMin  = ImGui::GetItemRectMin();
                const ImVec2 bMax  = ImGui::GetItemRectMax();
                const ImVec2 mouse = ImGui::GetIO().MousePos;
                const bool   want  = mouse.x >= bMin.x && mouse.x <= bMax.x
                                  && mouse.y >= bMin.y && mouse.y <= bMax.y
                                  && ImGui::IsMouseDown(ImGuiMouseButton_Right);
                p.delTooltipFade += (want ? 1.f : -1.f) * ImGui::GetIO().DeltaTime * 6.f;
                p.delTooltipFade  = Clamp01(p.delTooltipFade);
                if (p.delTooltipFade > 0.001f)
                    DrawTooltip("Delete", p.delTooltipFade);
            }

            ImGui::PopID();
        }

        // Delete confirmation
        if (triggerConfirm)
            ImGui::OpenPopup("Delete Profile?##del_confirm");

        if (ImGui::BeginPopupModal("Delete Profile?##del_confirm", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            if (s_confirmDeleteIdx >= 0 && s_confirmDeleteIdx < (int)s_profiles.size())
                ImGui::Text("  \"%s\"", s_profiles[s_confirmDeleteIdx].name.c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float popBtnW = 80.f * s;
            const float popBtnGap = 8.f * s;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                (ImGui::GetContentRegionAvail().x - popBtnW * 2.f - popBtnGap) * 0.5f);

            if (ImGui::Button("Delete##conf_yes", ImVec2(popBtnW, 0)))
            {
                if (s_confirmDeleteIdx >= 0 && s_confirmDeleteIdx < (int)s_profiles.size())
                {
                    s_profiles.erase(s_profiles.begin() + s_confirmDeleteIdx);
                    if (s_renamingIdx == s_confirmDeleteIdx)
                        s_renamingIdx = -1;
                    else if (s_renamingIdx > s_confirmDeleteIdx)
                        --s_renamingIdx;
                    SaveProfiles();
                }
                s_confirmDeleteIdx = -1;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine(0.f, popBtnGap);
            if (ImGui::Button("Cancel##conf_no", ImVec2(popBtnW, 0)) ||
                ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                s_confirmDeleteIdx = -1;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::Spacing();

        // Add Profile
        if (s_addingNew)
        {
            if (s_addFocus) {
                ImGui::SetKeyboardFocusHere();
                s_addFocus = false;
            }

            const float saveBtnW = 52.f * s;
            ImGui::SetNextItemWidth(innerW - saveBtnW - btnGap);
            const bool confirm = ImGui::InputText("##add_in", s_addNameBuffer, sizeof(s_addNameBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue |
                ImGuiInputTextFlags_AutoSelectAll);

            ImGui::SameLine();
            if (AnimatedButton("Save##add_sv", "##save_btn", ImVec2(saveBtnW, kRowH),
                ImVec4(0.10f, 0.35f, 0.10f, 0.85f),
                ImVec4(0.15f, 0.50f, 0.15f, 0.95f),
                ImVec4(0.20f, 0.65f, 0.20f, 1.00f)))
            {
                Profile p;
                p.name = (s_addNameBuffer[0] != '\0')
                    ? std::string(s_addNameBuffer)
                    : "Profile " + std::to_string(s_profiles.size() + 1);
                SaveCurrentFunctionsToProfile(p);
                s_profiles.push_back(std::move(p));
                SaveProfiles();
                s_addingNew = false;
                s_addNameBuffer[0] = '\0';
            }

            if (confirm)
            {
                Profile p;
                p.name = (s_addNameBuffer[0] != '\0')
                    ? std::string(s_addNameBuffer)
                    : "Profile " + std::to_string(s_profiles.size() + 1);
                SaveCurrentFunctionsToProfile(p);
                s_profiles.push_back(std::move(p));
                SaveProfiles();
                s_addingNew = false;
                s_addNameBuffer[0] = '\0';
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                s_addingNew = false;
                s_addNameBuffer[0] = '\0';
            }
        }
        else
        {
            if (AnimatedButton("+ Add Profile", "##add_btn", ImVec2(innerW, kRowH),
                ImVec4(0.10f, 0.10f, 0.10f, 0.85f),
                ImVec4(MAIN_RED.x * 0.50f, MAIN_RED.y * 0.50f, MAIN_RED.z * 0.50f, 0.70f),
                ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.85f)))
            {
                s_addingNew = true;
                s_addFocus = true;
                s_addNameBuffer[0] = '\0';
            }
            {
                const ImVec2 bMin  = ImGui::GetItemRectMin();
                const ImVec2 bMax  = ImGui::GetItemRectMax();
                const ImVec2 mouse = ImGui::GetIO().MousePos;
                const bool   want  = mouse.x >= bMin.x && mouse.x <= bMax.x
                                  && mouse.y >= bMin.y && mouse.y <= bMax.y
                                  && ImGui::IsMouseDown(ImGuiMouseButton_Right);
                s_addTooltipFade += (want ? 1.f : -1.f) * ImGui::GetIO().DeltaTime * 6.f;
                s_addTooltipFade  = Clamp01(s_addTooltipFade);
                if (s_addTooltipFade > 0.001f)
                    DrawTooltip("Saves all currently active functions as a new profile", s_addTooltipFade);
            }
        }

        ImGui::PopStyleVar(2);
    }

    {
        float contentEndY = ImGui::GetCursorPosY();
        float actualContent = (contentEndY - contentStartY) * s_headerAnim;

        if (s_contentAnim == 0.0f)
            s_contentAnim = actualContent;
        s_contentAnim += (actualContent - s_contentAnim) * ImGui::GetIO().DeltaTime * 20.0f;
        if (fabsf(s_contentAnim - actualContent) < 0.5f)
            s_contentAnim = actualContent;

        float bottomPad = ImGui::GetStyle().WindowPadding.y + 4.0f;
        ImGui::SetWindowSize(ImVec2(kWidth, kHeaderH + s_contentAnim + bottomPad));
    }

    ImVec2 sbPos = ImGui::GetWindowPos();
    ImVec2 sbSize = ImGui::GetWindowSize();
    ImGui::End();
    if (LavenderHook::Globals::use_polished_overlay)
        LavenderHook::UI::Lavender::DrawWindowShadow(sbPos, sbSize, alpha);
    ImGui::PopStyleVar();
}

} // namespace LavenderHook::UI::Windows

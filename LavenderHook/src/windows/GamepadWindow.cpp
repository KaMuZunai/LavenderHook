#include "GamepadWindow.h"
#include "../misc/Globals.h"
#include "../input/VirtualGamepad.h"
#include "../input/VkTable.h"
#include "../imgui/imgui.h"
#include "../ui/components/LavenderFadeOut.h"
#include "../ui/components/LavenderWindowHeader.h"
#include "../assets/UITextures.h"
#include <algorithm>

#include <cmath>
#include <cstring>
#include <string>
#include <shellapi.h>

// Theme colors shared across the whole menu
extern ImVec4 MAIN_RED;
extern ImVec4 MID_RED;

namespace LavenderHook::UI::Windows {

namespace {
    static LavenderHook::UI::LavenderFadeOut s_fade;
    static bool  s_headerOpen = true;
    static float s_headerAnim = 1.0f;
    static float s_arrowAnim  = 1.0f;
    static bool  s_wasVisible = false;

    // Shift-latch
    static bool  s_latch[16] = {};
    static bool  s_prevShift = false;
    static float s_btnHov[16] = {};
    static float s_btnAct[16] = {};

    // Analog stick state
    static float s_lsx = 0.f, s_lsy = 0.f; // left  stick
    static float s_rsx = 0.f, s_rsy = 0.f; // right stick

    bool g_vigemPopupTrigger = false;
    bool g_vigemConnectPending = false;
}

void GamepadWindow::ShowVigemPopup()
{
    g_vigemPopupTrigger = true;
}

static float GpClamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

// Button helper
static bool GpBtn(const char* id, const char* label, ImVec2 sz,
                  bool& latched, bool shiftHeld, int idx)
{
    const float dt    = ImGui::GetIO().DeltaTime;
    const float speed = 8.0f;

    const ImVec4 baseCol (0.05f, 0.05f, 0.05f, 0.92f);
    const ImVec4 hovCol  (MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.75f);
    const ImVec4 actCol  (MID_RED.x,  MID_RED.y,  MID_RED.z,  0.95f);
    const ImVec4 latchCol(MID_RED.x,  MID_RED.y,  MID_RED.z,  0.55f);

    const bool lit = latched && shiftHeld;

    auto Lerp4 = [](const ImVec4& a, const ImVec4& b, float t) {
        return ImVec4(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t,
                      a.z+(b.z-a.z)*t, a.w+(b.w-a.w)*t);
    };

    const ImVec4 btnCol = lit ? latchCol
                              : Lerp4(Lerp4(baseCol, hovCol, s_btnHov[idx]),
                                      actCol, s_btnAct[idx]);

    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Button,        btnCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  btnCol);
    ImGui::Button(label, sz);

    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();

    if (shiftHeld && ImGui::IsItemClicked()) latched = true;

    ImGui::PopStyleColor(3);
    ImGui::PopID();

    const float k = GpClamp01(dt * speed);
    s_btnHov[idx] += ((hovered || active ? 1.0f : 0.0f) - s_btnHov[idx]) * k;
    s_btnAct[idx] += ((active            ? 1.0f : 0.0f) - s_btnAct[idx]) * k;

    return active || lit;
}

// Analog stick pad
static bool StickPad(const char* id, float& nx, float& ny,
                     float size, bool shiftHeld, float drawAlpha,
                     bool& latched)
{
    const ImVec2 sp = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(size, size));
    const bool active = ImGui::IsItemActive();

    if (active)
    {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float  cx    = sp.x + size * 0.5f;
        const float  cy    = sp.y + size * 0.5f;
        float dx = (mouse.x - cx) / (size * 0.5f);
        float dy = -(mouse.y - cy) / (size * 0.5f);
        const float len = sqrtf(dx * dx + dy * dy);
        if (len > 1.f) { dx /= len; dy /= len; }
        nx = dx; ny = dy;
    }
    else if (!shiftHeld)
    {
        nx = 0.f; ny = 0.f;
    }

    if (ImGui::IsItemClicked(1))
        latched = !latched;

    bool pressed = latched;

    // Draw 
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    const ImVec2 br(sp.x + size, sp.y + size);
    const float  rad = size * 0.5f - 2.f;
    const ImVec2 ctr(sp.x + size * 0.5f, sp.y + size * 0.5f);

    auto C = [&](int r, int g, int b, float a) -> ImU32 {
        return IM_COL32(r, g, b, static_cast<int>(a * drawAlpha * 255.f + 0.5f));
    };

    dl->AddRectFilled(sp, br,  C( 8,  8,  8, 0.93f), 4.f);
    dl->AddRect      (sp, br,  C(60, 60, 60, 0.80f), 4.f, 0, 1.5f);
    dl->AddCircle    (ctr, rad, C(50, 50, 50, 0.70f), 48, 1.2f);
    dl->AddLine({ctr.x - rad, ctr.y}, {ctr.x + rad, ctr.y}, C(35, 35, 35, 0.60f));
    dl->AddLine({ctr.x, ctr.y - rad}, {ctr.x, ctr.y + rad}, C(35, 35, 35, 0.60f));

    const float dotX = ctr.x + nx * rad;
    const float dotY = ctr.y - ny * rad;
    const float dotR = size * 0.12f;

    ImVec4 dv;
    if      (latched)              dv = ImVec4(MID_RED.x,  MID_RED.y,  MID_RED.z,  0.93f);
    else if (active)               dv = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.80f);
    else if (nx != 0.f || ny != 0.f) dv = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.70f);
    else                             dv = ImVec4(0.35f, 0.35f, 0.35f, 0.87f);

    const ImU32 dotFill = IM_COL32(
        static_cast<int>(dv.x * 255.f + 0.5f),
        static_cast<int>(dv.y * 255.f + 0.5f),
        static_cast<int>(dv.z * 255.f + 0.5f),
        static_cast<int>(dv.w * drawAlpha * 255.f + 0.5f));
    dl->AddCircleFilled({dotX, dotY}, dotR, dotFill, 24);
    dl->AddCircle      ({dotX, dotY}, dotR, C(140, 140, 140, 0.55f), 24, 1.0f);

    return pressed;
}

void GamepadWindow::Render(bool wantVisible)
{
    s_fade.Tick(wantVisible);

    using namespace LavenderHook::Input::VGamepad;
    using namespace LavenderHook::UI::Lavender;

    if (s_wasVisible && !wantVisible)
    {
        if (Available())
        {
            for (int vk = GPVK_BASE; vk < GPVK_END; ++vk)
                SetButton(vk, false);
            SetThumb(true,  0.f, 0.f);
            SetThumb(false, 0.f, 0.f);
            Update();
        }
        std::memset(s_latch, 0, sizeof(s_latch));
        s_lsx = s_lsy = s_rsx = s_rsy = 0.f;
    }
    s_wasVisible = wantVisible;

    // ViGEm popup
    {
        ImGui::SetNextWindowPos(ImVec2(-9999.f, -9999.f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(0.f, 0.f), ImGuiCond_Always);
        ImGui::Begin("##vigemPopupHost", nullptr,
            ImGuiWindowFlags_NoDecoration  |
            ImGuiWindowFlags_NoBackground  |
            ImGuiWindowFlags_NoNav         |
            ImGuiWindowFlags_NoMove        |
            ImGuiWindowFlags_NoSavedSettings);

        if (g_vigemConnectPending)
        {
            if (Available())
                g_vigemConnectPending = false;
            else if (!Initializing() && Failed()) {
                g_vigemConnectPending = false;
                g_vigemPopupTrigger = true;
            }
        }

        if (g_vigemPopupTrigger) {
            ImGui::OpenPopup("ViGEmBus Required##vigem");
            g_vigemPopupTrigger = false;
        }

        if (ImGui::BeginPopupModal("ViGEmBus Required##vigem", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("ViGEmBus is required to send virtual gamepad inputs.");
            ImGui::Spacing();
            ImGui::Text("Download and install it from:");
            ImGui::SameLine();
            if (ImGui::SmallButton("https://github.com/nefarius/ViGEmBus/releases/tag/v1.22.0")) {
                ShellExecuteW(nullptr, L"open",
                    L"https://github.com/nefarius/ViGEmBus/releases/tag/v1.22.0",
                    nullptr, nullptr, SW_SHOWNORMAL);
            }
            ImGui::Spacing();
            const float btnW = 120.f;
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - btnW) * 0.5f + ImGui::GetCursorPosX());
            if (ImGui::Button("OK##vigem_ok", ImVec2(btnW, 0.f)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::End();
    }

    if (!s_fade.ShouldRender()) return;

    const float alpha = s_fade.Alpha();

    // Animate header collapse
    {
        const float target = s_headerOpen ? 1.f : 0.f;
        s_headerAnim += (target - s_headerAnim) * ImGui::GetIO().DeltaTime * 8.f;
        s_headerAnim = GpClamp01(s_headerAnim);
    }

    float s = LavenderHook::Globals::menu_scale;
    const float w       = 330.0f * s;
    const float pad     = 8.0f * s;
    const float btnSz   = 38.0f * s;
    const float gap     = 5.0f * s;
    const float stickSz = 76.0f * s;
    const  float rowH = btnSz + gap;

    const float bodyH = ImGui::GetFrameHeightWithSpacing()
                      + rowH + 4.0f * s
                      + rowH * 3.0f + stickSz
                      + 30.0f * s;

    ImGui::SetNextWindowSize(ImVec2(w, 36.0f * s + bodyH * s_headerAnim), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(400.f, 60.f), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoResize           |
        ImGuiWindowFlags_NoScrollbar        |
        ImGuiWindowFlags_NoScrollWithMouse  |
        ImGuiWindowFlags_NoTitleBar         |
        ImGuiWindowFlags_NoCollapse;

    if (!ImGui::Begin("##GamepadCtrl", nullptr, kFlags))
    {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    RenderWindowHeader("Virtual Controller", g_zapIcoTex, g_dropLeftTex,
        w, alpha, s_headerOpen, s_headerAnim, s_arrowAnim);

    s = LavenderHook::Globals::menu_scale;

    if (s_headerAnim > 0.001f)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha * s_headerAnim);

        const bool  avail   = Available();
        const bool  initing = Initializing();
        const DWORD slot    = GetUserIndex();

		// Status and connect/disconnect button
        {
            const ImVec4 statusCol = avail   ? ImVec4(0.20f, 0.85f, 0.20f, 1.f)
                                   : initing ? ImVec4(1.00f, 0.75f, 0.00f, 1.f)
                                             : ImVec4(0.55f, 0.55f, 0.55f, 1.f);
            std::string statusText;
            if      (avail)   statusText = "Connected";
            else if (initing) statusText = "Connecting...";
            else              statusText = "Disconnected";
            ImGui::TextColored(statusCol, "%s", statusText.c_str());
        }
        {
            float btnW = 120.0f * s;
            ImGui::SameLine();
            ImGui::SetCursorPosX(w - pad - btnW);
            if (initing) {
                ImGui::BeginDisabled();
                ImGui::Button("Connecting...", ImVec2(btnW, 0));
                ImGui::EndDisabled();
            } else if (avail) {
                if (ImGui::Button("Disconnect", ImVec2(btnW, 0))) Shutdown();
            } else {
                if (ImGui::Button("Connect", ImVec2(btnW, 0))) {
                    Initialize();
                    g_vigemConnectPending = true;
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        {
            const float savedY = ImGui::GetCursorPosY();
            ImGui::SetCursorPosX(pad);
            ImGui::Dummy(ImVec2(w - pad * 2.0f, 0.0f));
            ImGui::SetCursorPosY(savedY);
        }

        s = LavenderHook::Globals::menu_scale;

        // Shift latch bookkeepin
        const bool shiftHeld = ImGui::GetIO().KeyShift;
        if (!shiftHeld && s_prevShift)
        {
            std::memset(s_latch, 0, sizeof(s_latch));
            s_lsx = s_lsy = s_rsx = s_rsy = 0.f;
        }
        s_prevShift = shiftHeld;

        // Controller layout
        const float drawAlpha = alpha * s_headerAnim;
        const ImVec2 bsz(btnSz, btnSz);
        const float backBtnW = btnSz + 18.0f * s;

        const int iLB = GPVK_LB        - GPVK_BASE, iLT = GPVK_LT        - GPVK_BASE;
        const int iRT = GPVK_RT        - GPVK_BASE, iRB = GPVK_RB        - GPVK_BASE;
        const int iDU = GPVK_DPAD_UP   - GPVK_BASE, iDD = GPVK_DPAD_DOWN - GPVK_BASE;
        const int iDL = GPVK_DPAD_LEFT - GPVK_BASE, iDR = GPVK_DPAD_RIGHT- GPVK_BASE;
        const int iY  = GPVK_Y - GPVK_BASE, iX = GPVK_X - GPVK_BASE;
        const int iA  = GPVK_A - GPVK_BASE, iB = GPVK_B - GPVK_BASE;
        const int iBK = GPVK_BACK  - GPVK_BASE, iST = GPVK_START - GPVK_BASE;
        const int iLS = GPVK_LS    - GPVK_BASE, iRS = GPVK_RS    - GPVK_BASE;

        const float dpadL  = pad;
        const float dpadCX = pad + rowH;
        const float dpadR  = pad + rowH * 2.0f;
        const float faceR  = w - pad - btnSz;
        const float faceCX = faceR - rowH;
        const float faceL  = faceCX - rowH;
        const float rsX    = w - pad - stickSz;
        const float backX  = floorf((w - backBtnW * 2.0f - gap) * 0.5f);
        const float startX = backX + backBtnW + gap; 

        bool hLT, hLB, hRB, hRT;
        bool hDU, hDD, hDL, hDR;
        bool hY,  hX,  hA,  hB;
        bool hBk, hSt;

        // Center Text
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));

        // Shoulder row
        float curY = ImGui::GetCursorPosY();
        ImGui::SetCursorPos({dpadL,  curY}); hLT = GpBtn("LT", "LT", bsz, s_latch[iLT], shiftHeld, iLT);
        ImGui::SetCursorPos({dpadCX, curY}); hLB = GpBtn("LB", "LB", bsz, s_latch[iLB], shiftHeld, iLB);
        ImGui::SetCursorPos({faceCX, curY}); hRB = GpBtn("RB", "RB", bsz, s_latch[iRB], shiftHeld, iRB);
        ImGui::SetCursorPos({faceR,  curY}); hRT = GpBtn("RT", "RT", bsz, s_latch[iRT], shiftHeld, iRT);
        curY += rowH + 4.0f;

        // Stick row
        const float lsY         = curY;
        const float bkStY       = lsY + floorf((stickSz - btnSz) * 0.5f);
        const float belowStickY = lsY + stickSz + gap;

        // Left stick
        ImGui::SetCursorPos({dpadL, lsY});
        bool hLS = StickPad("LS_pad", s_lsx, s_lsy, stickSz, shiftHeld, drawAlpha, s_latch[iLS]);

        // Right stick
        ImGui::SetCursorPos({rsX, lsY});
        bool hRS = StickPad("RS_pad", s_rsx, s_rsy, stickSz, shiftHeld, drawAlpha, s_latch[iRS]);

        // Back / Start
        ImGui::SetCursorPos({backX,  bkStY}); hBk = GpBtn("BACK",  "Back",  {backBtnW, btnSz}, s_latch[iBK], shiftHeld, iBK);
        ImGui::SetCursorPos({startX, bkStY}); hSt = GpBtn("STRT",  "Start", {backBtnW, btnSz}, s_latch[iST], shiftHeld, iST);

        // X Y A B buttons
        ImGui::SetCursorPos({faceCX, belowStickY});        hY = GpBtn("FY", "Y", bsz, s_latch[iY], shiftHeld, iY);

        curY = belowStickY + rowH;
        ImGui::SetCursorPos({faceL,  curY});               hX = GpBtn("FX", "X", bsz, s_latch[iX], shiftHeld, iX);
        ImGui::SetCursorPos({faceR,  curY});               hB = GpBtn("FB", "B", bsz, s_latch[iB], shiftHeld, iB);

        curY = belowStickY + rowH * 2.0f;
        ImGui::SetCursorPos({faceCX, curY});               hA = GpBtn("FA", "A", bsz, s_latch[iA], shiftHeld, iA);

        // Dpad buttons
        ImGui::SetCursorPos({dpadCX, belowStickY}); hDU = GpBtn("DU", "^", bsz, s_latch[iDU], shiftHeld, iDU);
        curY = belowStickY + rowH;
        ImGui::SetCursorPos({dpadL,  curY}); hDL = GpBtn("DL", "<", bsz, s_latch[iDL], shiftHeld, iDL);
        ImGui::SetCursorPos({dpadR,  curY}); hDR = GpBtn("DR", ">", bsz, s_latch[iDR], shiftHeld, iDR);
        curY = belowStickY + rowH * 2.0f;
        ImGui::SetCursorPos({dpadCX, curY}); hDD = GpBtn("DD", "v", bsz, s_latch[iDD], shiftHeld, iDD);

        ImGui::PopStyleVar();


        ImGui::SetCursorPosY(lsY + stickSz + rowH * 3.0f);
        ImGui::Dummy(ImVec2(w - pad * 2.0f, gap));

        if (avail)
        {
            SetButton(GPVK_LB,         hLB);
            SetButton(GPVK_LT,         hLT);
            SetButton(GPVK_RT,         hRT);
            SetButton(GPVK_RB,         hRB);
            SetButton(GPVK_DPAD_UP,    hDU);
            SetButton(GPVK_DPAD_DOWN,  hDD);
            SetButton(GPVK_DPAD_LEFT,  hDL);
            SetButton(GPVK_DPAD_RIGHT, hDR);
            SetButton(GPVK_Y,          hY);
            SetButton(GPVK_X,          hX);
            SetButton(GPVK_A,          hA);
            SetButton(GPVK_B,          hB);
            SetButton(GPVK_BACK,       hBk);
            SetButton(GPVK_START,      hSt);
            SetButton(GPVK_LS,         hLS);
            SetButton(GPVK_RS,         hRS);
            SetThumb(true,  s_lsx, s_lsy);
            SetThumb(false, s_rsx, s_rsy);
            Update();
        }

        ImGui::PopStyleVar();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace LavenderHook::UI::Windows

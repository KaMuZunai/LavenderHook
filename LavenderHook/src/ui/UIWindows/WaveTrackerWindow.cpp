#include "WaveTrackerWindow.h"
#include "../../misc/Globals.h"
#include "../../misc/logmonitor/LogMonitor.h"
#include "../../misc/NumberFormat.h"
#include "../../imgui/imgui.h"
#include "../../ui/components/LavenderFadeOut.h"
#include "../../memory/aobhooks/WaveDataHook.h"
#include "../../memory/aobhooks/EnemiesMaxFallbackHook.h"
#include "../../memory/aobhooks/TimeHook.h"

#include <cmath>
#include <string>
#include <algorithm>

extern ImVec4 DARK_RED;

namespace LavenderHook::UI::Windows {

    namespace {
        static LavenderHook::UI::LavenderFadeOut s_fade;
        static LavenderHook::UI::LavenderFadeOut s_combatFade;
        static float s_displayProg         = 0.f;
        static int   s_lastAlive           = -1;
        static int   s_lastMax             = -1;
        static bool  s_lastBoss            = false;
        static float s_lastValidBossHpMax  = 0.f;
        static float s_yellowCommitted     = 0.f;
        static float s_yellowDisplayProg   = 0.f;
        static float s_yellowTimer         = 0.f;
        static float s_combatGraceTimer    = 0.f;
        static float s_reviveCheckTimer    = 0.f;
        static float s_waveElapsed         = 0.f;
        static bool  s_lastInCombatOrBoss  = false;
    }

    static void RenderCombatBar(float alpha, float drawProg, bool isBoss,
                                int displayAlive, int maxE,
                                float bossHp, float bossHpMax, int wave,
                                float yellowProg, float waveElapsed)
    {
        const int   maxWave  = LavenderHook::LogMonitor::GetMaxWave();
        const int   timeVal  = LavenderHook::Globals::wave_time.load();

        ImFont* const font   = ImGui::GetFont();
        const float   baseSz = font->LegacySize;
        const float   smallSz = baseSz * 1.15f;
        const float   bigSz   = baseSz * 1.45f;

        const float displayW   = ImGui::GetIO().DisplaySize.x;
        const float displayH   = ImGui::GetIO().DisplaySize.y;
        const float uiScale    = displayH / 1440.f;
        const float barScale   = 0.406f;
        const float defaultW   = displayW * barScale;

        const float padH       = 14.f * uiScale;
        const float padV       = 9.f  * uiScale;
        const float mainBarH   = 52.f * uiScale;
        const float gapY       = 10.f * uiScale;
        const float infoPadY   = 6.f  * uiScale;
        const float trapSlant  = 16.f  * uiScale;
        const float trapExtend = 55.f  * uiScale;
        const float infoH      = font->CalcTextSizeA(smallSz, FLT_MAX, 0.f, "W").y;
        const float totalH     = padV + mainBarH + gapY + infoH + infoPadY + 4.f * uiScale;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));

        ImGui::SetNextWindowSize(ImVec2(defaultW, totalH), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2((displayW - defaultW) * 0.5f, 10.f * uiScale),
            ImGuiCond_FirstUseEver);

        constexpr ImGuiWindowFlags kFlags =
            ImGuiWindowFlags_NoTitleBar            |
            ImGuiWindowFlags_NoScrollbar           |
            ImGuiWindowFlags_NoScrollWithMouse     |
            ImGuiWindowFlags_NoCollapse            |
            ImGuiWindowFlags_NoResize              |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoFocusOnAppearing    |
            ImGuiWindowFlags_NoNav;

        if (!ImGui::Begin("##WaveCombatBar", nullptr, kFlags))
        {
            ImGui::End();
            ImGui::PopStyleColor(1);
            ImGui::PopStyleVar(3);
            return;
        }

        ImDrawList*  dl = ImGui::GetWindowDrawList();
        const ImVec2 wp    = ImGui::GetWindowPos();
        const ImVec2 winSz = ImGui::GetWindowSize();
        const float  combatW = winSz.x;

        const auto ACol = [&](const ImVec4& c, float a) -> ImU32 {
            return IM_COL32(static_cast<int>(c.x * 255),
                            static_cast<int>(c.y * 255),
                            static_cast<int>(c.z * 255),
                            static_cast<int>(a * alpha * 255.f));
        };

        const ImVec4 accentVec = ImVec4(DARK_RED.x, DARK_RED.y, DARK_RED.z, 1.f);
        const ImU32  white     = IM_COL32(255, 255, 255, static_cast<int>(240.f * alpha));
        const ImU32  dimCol    = IM_COL32(255, 255, 255, static_cast<int>(240.f * alpha));
        const ImU32  outlineC  = IM_COL32(0,   0,   0,   static_cast<int>(210.f * alpha));

        const ImVec4 fillVec = isBoss
            ? ImVec4(1.f, 0.1f + 0.55f * drawProg, 0.1f * drawProg, 1.f)
            : accentVec;

        // Main bar
        const float barX  = wp.x + padH;
        const float barY  = wp.y + padV;
        const float barW  = combatW - padH * 2.f;
        const float slant   = mainBarH * 0.28f;
        const float tipFrac = 0.32f;

        // Hexagonal bar
        ImVec2 hexPts[6] = {
            { barX + slant,        barY },
            { barX + barW - slant, barY },
            { barX + barW,         barY + mainBarH * tipFrac },
            { barX + barW - slant, barY + mainBarH },
            { barX + slant,        barY + mainBarH },
            { barX,                barY + mainBarH * tipFrac },
        };

        // Info trapezoid
        {
            const float tabTop_ = barY + mainBarH;
            const float tBot_   = tabTop_ + gapY + infoH + infoPadY;
            const float wTx0_   = barX + barW * 0.13f - 12.f * uiScale - trapExtend;
            const float tTx1_   = barX + barW * 0.87f + 12.f * uiScale + trapExtend;
            ImVec2 pts[4] = {
                { wTx0_,             tabTop_ },
                { tTx1_,             tabTop_ },
                { tTx1_ - trapSlant, tBot_   },
                { wTx0_ + trapSlant, tBot_   },
            };
            const ImU32  infoBgTop = IM_COL32(52, 48, 46, static_cast<int>(255.f * alpha));
            const ImU32  infoBgBot = IM_COL32(80, 74, 71, static_cast<int>(255.f * alpha));
            const ImVec2 uv        = ImGui::GetIO().Fonts->TexUvWhitePixel;
            dl->PrimReserve(6, 4);
            const ImDrawIdx bi = (ImDrawIdx)dl->_VtxCurrentIdx;
            dl->PrimWriteVtx(pts[0], uv, infoBgTop);
            dl->PrimWriteVtx(pts[1], uv, infoBgTop);
            dl->PrimWriteVtx(pts[2], uv, infoBgBot);
            dl->PrimWriteVtx(pts[3], uv, infoBgBot);
            dl->PrimWriteIdx(bi + 0); dl->PrimWriteIdx(bi + 1); dl->PrimWriteIdx(bi + 2);
            dl->PrimWriteIdx(bi + 0); dl->PrimWriteIdx(bi + 2); dl->PrimWriteIdx(bi + 3);
        }

        // Bar Background
        {
            const ImU32  hexBgTop = IM_COL32(10, 10, 10, static_cast<int>(255.f * alpha));
            const ImU32  hexBgMid = IM_COL32(14, 14, 14, static_cast<int>(255.f * alpha));
            const ImU32  hexBgBot = IM_COL32(20, 20, 20, static_cast<int>(255.f * alpha));
            const ImVec2 uv       = ImGui::GetIO().Fonts->TexUvWhitePixel;
            dl->PrimReserve(12, 6);
            const ImDrawIdx bi = (ImDrawIdx)dl->_VtxCurrentIdx;
            dl->PrimWriteVtx(hexPts[0], uv, hexBgTop);
            dl->PrimWriteVtx(hexPts[1], uv, hexBgTop);
            dl->PrimWriteVtx(hexPts[2], uv, hexBgMid);
            dl->PrimWriteVtx(hexPts[3], uv, hexBgBot);
            dl->PrimWriteVtx(hexPts[4], uv, hexBgBot);
            dl->PrimWriteVtx(hexPts[5], uv, hexBgMid);
            dl->PrimWriteIdx(bi+0); dl->PrimWriteIdx(bi+1); dl->PrimWriteIdx(bi+2);
            dl->PrimWriteIdx(bi+0); dl->PrimWriteIdx(bi+2); dl->PrimWriteIdx(bi+3);
            dl->PrimWriteIdx(bi+0); dl->PrimWriteIdx(bi+3); dl->PrimWriteIdx(bi+4);
            dl->PrimWriteIdx(bi+0); dl->PrimWriteIdx(bi+4); dl->PrimWriteIdx(bi+5);
        }

        // Yellow secondary bar
        if (yellowProg > drawProg + 0.001f)
        {
            const ImU32 yCol = IM_COL32(255, 210, 0, static_cast<int>(220.f * alpha));
            dl->PushClipRect(
                ImVec2(barX + barW * drawProg - 1.f, barY - 1.f),
                ImVec2(barX + barW * yellowProg + 1.f, barY + mainBarH + 1.f), true);
            dl->AddConvexPolyFilled(hexPts, 6, yCol);
            dl->PopClipRect();
        }

        // Main Bar
        if (drawProg > 0.f)
        {
            if (isBoss)
            {
                dl->PushClipRect(
                    ImVec2(barX, barY - 1.f),
                    ImVec2(barX + barW * drawProg, barY + mainBarH + 1.f), true);
                dl->AddConvexPolyFilled(hexPts, 6, ACol(fillVec, 0.88f));
                dl->PopClipRect();
            }
            else
            {
                const ImU32 gLeft    = IM_COL32( 85,   0, 110, static_cast<int>(220.f * alpha));
                const ImU32 gRight   = IM_COL32(225,  25, 145, static_cast<int>(220.f * alpha));
                const float midStart = barX + slant;
                const float midEnd   = barX + barW - slant;
                const float fillEndX = barX + barW * drawProg;

                if (fillEndX <= midStart)
                {
                    dl->PushClipRect(
                        ImVec2(barX, barY - 1.f),
                        ImVec2(fillEndX, barY + mainBarH + 1.f), true);
                    dl->AddTriangleFilled(
                        ImVec2(barX,         barY + mainBarH * tipFrac),
                        ImVec2(barX + slant, barY),
                        ImVec2(barX + slant, barY + mainBarH), gLeft);
                    dl->PopClipRect();
                }
                else
                {
                    // Left
                    dl->AddTriangleFilled(
                        ImVec2(barX,         barY + mainBarH * tipFrac),
                        ImVec2(barX + slant, barY),
                        ImVec2(barX + slant, barY + mainBarH), gLeft);

                    // Middle
                    dl->PushClipRect(
                        ImVec2(midStart, barY - 1.f),
                        ImVec2(std::min(fillEndX, midEnd), barY + mainBarH + 1.f), true);
                    dl->AddRectFilledMultiColor(
                        ImVec2(midStart, barY), ImVec2(midEnd, barY + mainBarH),
                        gLeft, gRight, gRight, gLeft);
                    dl->PopClipRect();

                    // Right
                    if (fillEndX >= midEnd)
                    {
                        dl->PushClipRect(
                            ImVec2(midEnd - 1.f, barY - 1.f),
                            ImVec2(fillEndX + 1.f, barY + mainBarH + 1.f), true);
                        dl->AddTriangleFilled(
                            ImVec2(barX + barW - slant, barY),
                            ImVec2(barX + barW,         barY + mainBarH * tipFrac),
                            ImVec2(barX + barW - slant, barY + mainBarH), gRight);
                        dl->PopClipRect();
                    }
                }
            }
        }

        // Hex outline
        dl->AddPolyline(hexPts, 6,
            IM_COL32(80, 74, 71, static_cast<int>(220.f * alpha)), ImDrawFlags_Closed, 5.0f * uiScale);

        // Centred count text
        const std::string cntStr = isBoss
            ? (LavenderHook::Misc::FormatCompactNumber(static_cast<double>(bossHp))
               + " / " + LavenderHook::Misc::FormatCompactNumber(static_cast<double>(bossHpMax)))
            : (maxE > 0
               ? (std::to_string(std::max(0, displayAlive)) + " / " + std::to_string(maxE))
               : "---");

        const ImVec2 cntSz = font->CalcTextSizeA(bigSz, FLT_MAX, 0.f, cntStr.c_str());
        const float  cntX  = barX + (barW  - cntSz.x) * 0.5f;
        const float  cntY  = barY + (mainBarH - cntSz.y) * 0.5f;
        const float  bOff  = 1.5f * uiScale;
        const ImVec2 offs[] = {
            { bOff,0.f },{ -bOff,0.f },{ 0.f,bOff },{ 0.f,-bOff }
        };
        for (const auto& o : offs)
            dl->AddText(font, bigSz, ImVec2(cntX + o.x, cntY + o.y), outlineC, cntStr.c_str());
        dl->AddText(font, bigSz, ImVec2(cntX, cntY), white, cntStr.c_str());

        // Info row
        const float infoY = barY + mainBarH + gapY;

        const std::string waveStr = "WAVE: "
            + (wave    > 0 ? std::to_string(wave)    : "-")
            + " / "
            + (maxWave > 0 ? std::to_string(maxWave) : "-");

        std::string timeStr = "TIME: ";
        if (timeVal <= 0)
        {
            timeStr += "\xe2\x88\x9e";
        }
        else
        {
            const int mins = timeVal / 60;
            const int secs = timeVal % 60;
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
            timeStr += buf;
        }

        const int   elSec  = static_cast<int>(waveElapsed);
        char elBuf[8];
        std::snprintf(elBuf, sizeof(elBuf), "%d:%02d", elSec / 60, elSec % 60);

        const ImVec2 timeSz  = font->CalcTextSizeA(smallSz, FLT_MAX, 0.f, timeStr.c_str());
        const ImVec2 elSz    = font->CalcTextSizeA(smallSz, FLT_MAX, 0.f, elBuf);
        const float  wTextX  = barX + barW * 0.13f;
        const float  tTextX  = barX + barW * 0.87f - timeSz.x;
        const float  elX     = barX + barW * 0.5f - elSz.x * 0.5f;
        dl->AddText(font, smallSz, ImVec2(wTextX, infoY), dimCol, waveStr.c_str());
        dl->AddText(font, smallSz, ImVec2(elX,    infoY), dimCol, elBuf);
        dl->AddText(font, smallSz, ImVec2(tTextX, infoY), dimCol, timeStr.c_str());

        ImGui::End();
        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(3);
    }

    void WaveTrackerWindow::Render()
    {
        LavenderHook::Memory::TimeHook::Tick();

        const int   alive     = LavenderHook::Globals::enemies_alive.load();
        const int   maxE      = LavenderHook::Globals::enemies_max.load();
        const int   displayAlive = (alive == 0 && maxE > 0) ? maxE : alive;
        const bool  isBoss    = (LavenderHook::Globals::boss_phase_active.load()
                                || LavenderHook::LogMonitor::IsBossWave())
                                && (LavenderHook::Memory::WaveDataHook::g_hookAt != 0);
        const float bossHp    = LavenderHook::Globals::boss_health_current.load();
        const float bossHpMax = LavenderHook::Globals::boss_health_max.load();

        if (isBoss && bossHpMax > 0.f)
            s_lastValidBossHpMax = bossHpMax;
        else if (!isBoss)
            s_lastValidBossHpMax = 0.f;
        const float effectiveBossHpMax = (bossHpMax > 0.f) ? bossHpMax : s_lastValidBossHpMax;

        const bool  timeHookLive = (LavenderHook::Memory::TimeHook::g_hookAt != 0);
        const int   timeVal      = timeHookLive ? LavenderHook::Globals::wave_time.load() : 0;
        const bool hasEnemyData = (maxE > 0);
        const bool hasBossData  = isBoss && (effectiveBossHpMax > 0.f);
        const int  wave         = LavenderHook::LogMonitor::GetCurrentWave();

        const bool inCombat    = LavenderHook::LogMonitor::IsCombatPhase();
        const bool inBossWave  = LavenderHook::LogMonitor::IsBossWave();

        const bool combatAborted = LavenderHook::LogMonitor::IsCombatAborted();

        // reset all local states.
        if (LavenderHook::LogMonitor::HasJustRestarted())
        {
            s_combatGraceTimer   = 0.f;
            s_reviveCheckTimer   = 0.f;
            s_displayProg        = 0.f;
            s_lastAlive          = -1;
            s_lastMax            = -1;
            s_lastBoss           = false;
            s_lastValidBossHpMax = 0.f;
            s_yellowCommitted    = 0.f;
            s_yellowDisplayProg  = 0.f;
            s_yellowTimer        = 0.f;
            s_waveElapsed        = 0.f;
            s_lastInCombatOrBoss = false;
        }

        // 1.5s grace window
        if (inCombat || inBossWave)
        {
            s_combatGraceTimer = 1.0f;
            s_reviveCheckTimer = 0.f;
        }
        else
            s_combatGraceTimer = std::max(0.f, s_combatGraceTimer - ImGui::GetIO().DeltaTime);

        // revive
        if (s_combatFade.IsFullyHidden())
        {
            s_reviveCheckTimer += ImGui::GetIO().DeltaTime;
            if (s_reviveCheckTimer >= 0.1f)
            {
                s_reviveCheckTimer = 0.f;
                if (LavenderHook::LogMonitor::IsCombatPhase() || LavenderHook::LogMonitor::IsBossWave())
                    s_combatGraceTimer = 1.5f;
            }
        }
        else
            s_reviveCheckTimer = 0.f;

        // Wave elapsed timer — counts up while combat or boss phase is active
        {
            const bool activePhase = inCombat || inBossWave;
            if (activePhase && !s_lastInCombatOrBoss)
                s_waveElapsed = 0.f;
            if (activePhase)
                s_waveElapsed += ImGui::GetIO().DeltaTime;
            s_lastInCombatOrBoss = activePhase;
        }

        const bool wantCompact = LavenderHook::Globals::show_wave_window
            && !LavenderHook::LogMonitor::IsInTavern()
            && !combatAborted
            && !inCombat
            && !inBossWave
            && (wave > 0 || (isBoss && hasBossData) || (!isBoss && hasEnemyData) || timeVal > 0);

        const bool wantCombat  = LavenderHook::Globals::show_wave_window
            && !LavenderHook::LogMonitor::IsInTavern()
            && !combatAborted
            && (inCombat || inBossWave || s_combatGraceTimer > 0.f);

        s_fade.Tick(wantCompact);
        s_combatFade.Tick(wantCombat);
        if (!s_fade.ShouldRender() && !s_combatFade.ShouldRender()) return;

        // Progress
        const float realProg = [&]() -> float {
            if (isBoss)
            {
                if (effectiveBossHpMax <= 0.f) return 0.f;
                return std::max(0.f, std::min(1.f, bossHp / effectiveBossHpMax));
            }
            if (maxE <= 0) return 0.f;
            return std::max(0.f, std::min(1.f,
                static_cast<float>(displayAlive) / static_cast<float>(maxE)));
        }();

        const int   curMax = isBoss ? static_cast<int>(effectiveBossHpMax) : maxE;
        const float dt     = ImGui::GetIO().DeltaTime;
        if (s_lastAlive < 0 || s_lastBoss != isBoss || s_lastMax != curMax)
        {
            s_displayProg        = realProg;
            s_lastAlive          = alive;
            s_lastMax            = curMax;
            s_lastBoss           = isBoss;
            s_yellowCommitted    = realProg;
            s_yellowDisplayProg  = realProg;
            s_yellowTimer        = 0.f;
        }
        s_lastAlive = alive;
        s_lastMax   = curMax;
        s_lastBoss  = isBoss;

        const float d = realProg - s_displayProg;
        if (std::fabsf(d) > 0.001f)
            s_displayProg += d * std::min(dt * 8.f, 1.f);
        else
            s_displayProg = realProg;
        const float drawProg = std::max(0.f, std::min(1.f, s_displayProg));

        // Yellow secondary bar
        if (s_yellowTimer > 0.f)
        {
            s_yellowTimer = std::max(0.f, s_yellowTimer - dt);
            if (s_yellowTimer <= 0.f)
                s_yellowCommitted = realProg;
        }
        if (std::fabsf(realProg - s_yellowCommitted) > 0.001f && s_yellowTimer <= 0.f)
            s_yellowTimer = 0.75f;

        const float yd = s_yellowCommitted - s_yellowDisplayProg;
        if (std::fabsf(yd) > 0.001f)
            s_yellowDisplayProg += yd * std::min(dt * 8.f, 1.f);
        else
            s_yellowDisplayProg = s_yellowCommitted;

        const float yellowProg = std::max(0.f, std::min(1.f, s_yellowDisplayProg));

        if (s_combatFade.ShouldRender())
            RenderCombatBar(s_combatFade.Alpha(), drawProg, isBoss,
                            displayAlive, maxE, bossHp, effectiveBossHpMax, wave,
                            yellowProg, s_waveElapsed);

        if (!s_fade.ShouldRender()) return;
        const float alpha = s_fade.Alpha();

        // Accent color
        const ImVec4 bossGold = ImVec4(1.f, 0.65f, 0.1f, 1.f);
        const ImVec4 accentV  = isBoss
            ? bossGold
            : ImVec4(DARK_RED.x, DARK_RED.y, DARK_RED.z, 1.f);

        // Boss bar
        const ImVec4 barFillV = isBoss
            ? ImVec4(1.f, 0.1f + 0.55f * drawProg, 0.1f * drawProg, 1.f)
            : accentV;

        // Font sizes
        ImFont* const font    = ImGui::GetFont();
        const float   baseSz  = font->LegacySize;
        const float   smallSz = baseSz * 0.80f;
        const float   largeSz = baseSz * 2.30f;
        const float   subSz   = baseSz * 0.86f;

        // Header
        const std::string headerStr = isBoss
            ? "Boss Wave"
            : (wave > 0 ? ("WAVE " + std::to_string(wave)) : "ENEMIES");

        // Primary number
        const std::string bigStr = isBoss
            ? LavenderHook::Misc::FormatCompactNumber(static_cast<double>(bossHp))
            : std::to_string(std::max(0, displayAlive));

        // Sub label
        const std::string subStr = isBoss
            ? (effectiveBossHpMax > 0.f
                ? ("/ " + LavenderHook::Misc::FormatCompactNumber(
                              static_cast<double>(effectiveBossHpMax)))
                : std::string{})
            : (maxE > 0
                ? ("/ " + std::to_string(maxE))
                : std::string("/ ???"));

        // Measure text
        const ImVec2 numSz  = font->CalcTextSizeA(largeSz, FLT_MAX, 0.f, bigStr.c_str());
        const ImVec2 subSzV = subStr.empty()
            ? ImVec2(0.f, font->CalcTextSizeA(subSz, FLT_MAX, 0.f, "0").y)
            : font->CalcTextSizeA(subSz, FLT_MAX, 0.f, subStr.c_str());
        const ImVec2 hdrRefSz = font->CalcTextSizeA(smallSz, FLT_MAX, 0.f, headerStr.c_str());
        const float  smallH   = font->CalcTextSizeA(smallSz, FLT_MAX, 0.f, "W").y;

        // Time string
        std::string timeRowStr;
        if (timeVal > 0)
        {
            const int mins = timeVal / 60;
            const int secs = timeVal % 60;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
            timeRowStr = buf;
        }
        const ImVec2 timeSzV = timeRowStr.empty()
            ? ImVec2(0.f, 0.f)
            : font->CalcTextSizeA(smallSz, FLT_MAX, 0.f, timeRowStr.c_str());

        // Layout constants
        const float uiScale  = ImGui::GetIO().DisplaySize.y / 1440.f;
        const float padH    = 10.f * uiScale;
        const float padV    = 8.f  * uiScale;
        const float gapY    = 2.f  * uiScale;
        const float barH    = 5.f  * uiScale;
        const float barPadY = 6.f  * uiScale;
        const float accentW = 3.f  * uiScale;
        const float timeGap = timeSzV.y > 0.f ? 5.f * uiScale : 0.f;

        // Window dimensions
        float contentW = numSz.x + 6.f * uiScale + subSzV.x;
        contentW       = std::max(contentW, hdrRefSz.x);
        contentW       = std::max(contentW, timeSzV.x);
        const float winW = accentW + padH + contentW + padH;
        const float winH = padV + smallH + gapY + numSz.y + barPadY + barH + timeGap + timeSzV.y + padV;

        // Window setup
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));

        ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(20.f * uiScale, ImGui::GetIO().DisplaySize.y - winH - 20.f * uiScale),
            ImGuiCond_FirstUseEver);

        constexpr ImGuiWindowFlags kFlags =
            ImGuiWindowFlags_NoTitleBar            |
            ImGuiWindowFlags_NoResize              |
            ImGuiWindowFlags_NoScrollbar           |
            ImGuiWindowFlags_NoScrollWithMouse     |
            ImGuiWindowFlags_NoCollapse            |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoFocusOnAppearing    |
            ImGuiWindowFlags_NoNav;

        if (!ImGui::Begin("##WaveTracker", nullptr, kFlags))
        {
            ImGui::End();
            ImGui::PopStyleColor(1);
            ImGui::PopStyleVar(3);
            return;
        }

        ImDrawList*  dl = ImGui::GetWindowDrawList();
        const ImVec2 wp = ImGui::GetWindowPos();

        // Gradient background
        {
            const ImU32 bgTop = IM_COL32(6,  6,  6,  static_cast<int>(224.f * alpha));
            const ImU32 bgBot = IM_COL32(15, 15, 15, static_cast<int>(224.f * alpha));
            dl->AddRectFilledMultiColor(wp, ImVec2(wp.x + winW, wp.y + winH),
                bgTop, bgTop, bgBot, bgBot);
        }

        const auto ACol = [&](const ImVec4& c, float a) -> ImU32 {
            return IM_COL32(static_cast<int>(c.x * 255),
                            static_cast<int>(c.y * 255),
                            static_cast<int>(c.z * 255),
                            static_cast<int>(a * alpha * 255.f));
        };

        const ImU32 white   = IM_COL32(255, 255, 255, static_cast<int>(240.f * alpha));
        const ImU32 muted   = IM_COL32(160, 160, 160, static_cast<int>(170.f * alpha));
        const ImU32 outline = IM_COL32(0,   0,   0,   static_cast<int>(200.f * alpha));
        const ImU32 accent  = ACol(accentV, 220.f / 255.f);

        // Left accent bar
        dl->AddRectFilled(wp, ImVec2(wp.x + accentW, wp.y + winH), accent);

        const float cx = wp.x + accentW + padH;

        // Header label
        dl->AddText(font, smallSz, ImVec2(cx, wp.y + padV),
                    isBoss ? accent : muted, headerStr.c_str());

        // Large primary number with black outline
        const float numX = cx;
        const float numY = wp.y + padV + smallH + gapY;
        const float bOff = 1.5f * uiScale;
        const ImVec2 offsets[] = {
            { bOff, 0.f }, { -bOff, 0.f }, { 0.f, bOff }, { 0.f, -bOff },
            { bOff, bOff }, { -bOff, bOff }, { bOff, -bOff }, { -bOff, -bOff }
        };
        for (const auto& o : offsets)
            dl->AddText(font, largeSz, ImVec2(numX + o.x, numY + o.y),
                        outline, bigStr.c_str());
        dl->AddText(font, largeSz, ImVec2(numX, numY), white, bigStr.c_str());

        // Sub label (current/max)
        if (!subStr.empty())
        {
            const float subX = numX + numSz.x + 6.f;
            const float subY = numY + numSz.y - subSzV.y;
            dl->AddText(font, subSz, ImVec2(subX, subY), muted, subStr.c_str());
        }

        // Progress bar
        const float barY = numY + numSz.y + barPadY;
        const float barX = cx;
        const float barW = winW - accentW - padH * 2.f;

        dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                          IM_COL32(40, 40, 40, static_cast<int>(200.f * alpha)), 2.f * uiScale);
        if (drawProg > 0.f)
            dl->AddRectFilled(ImVec2(barX, barY),
                              ImVec2(barX + barW * drawProg, barY + barH),
                              ACol(barFillV, 200.f / 255.f), 2.f * uiScale);

        // Time row
        if (!timeRowStr.empty())
            dl->AddText(font, smallSz,
                        ImVec2(cx, barY + barH + timeGap),
                        muted, timeRowStr.c_str());

        ImGui::End();
        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(3);
    }

} // namespace LavenderHook::UI::Windows

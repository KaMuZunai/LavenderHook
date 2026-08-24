#include "InfoOverlayWindow.h"
#include "../../misc/Globals.h"
#include "../components/LavenderFadeOut.h"
#include "../components/LavenderGradient.h"
#include "../../net/NetworkMonitor.h"
#include "../ActionsOverlay.h"

#include "../../imgui/imgui.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>

namespace LavenderHook::UI::Windows {

    namespace {
        LavenderFadeOut g_info_overlay_fade;
    }

    void InfoOverlayWindow::Init()
    {
        // no-op
    }

    void InfoOverlayWindow::Render()
    {
        g_info_overlay_fade.Tick(LavenderHook::Globals::show_info_overlay);
        if (!g_info_overlay_fade.ShouldRender())
            return;

        float alpha = g_info_overlay_fade.Alpha();

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

        ImGui::Begin(
            "##overlay_root",
            nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBackground
        );

        ImDrawList* dl = ImGui::GetWindowDrawList();

        if (LavenderHook::Globals::use_polished_overlay)
        {
            // Polished style

            const float screenW = ImGui::GetIO().DisplaySize.x;
            const float lineH   = ImGui::GetTextLineHeight();

            constexpr float kMargin      = 28.f;
            constexpr float kPadX        = 12.f;
            constexpr float kPadY        = 6.f;
            constexpr float kRounding    = 8.f;

            constexpr float kDotR        = 4.f;
            constexpr float kDotGap      = 8.f;
            constexpr float kItemSpacing = 7.f;
            constexpr float kClusterGap  = 16.f;
            constexpr float kHeaderGap   = 5.f;

            const float itemH = lineH + kPadY * 2.f;

            const ImVec4 accentV = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
            auto AccentCol = [&](float a) -> ImU32 {
                return IM_COL32((int)(accentV.x * 255.f),
                                (int)(accentV.y * 255.f),
                                (int)(accentV.z * 255.f),
                                (int)(235.f * a));
            };

            auto PillWidth = [&](const std::string& text, bool hasDot) -> float {
                float tw = ImGui::CalcTextSize(text.c_str()).x;
                float dot = hasDot ? (kDotR * 2.f + kDotGap) : 0.f;
                return kPadX + dot + tw + kPadX;
            };

            auto DrawPill = [&](const ImVec2& pos, const std::string& text,
                                float a, ImU32 dotCol)
            {
                ImVec2 tsz = ImGui::CalcTextSize(text.c_str());
                float dot = dotCol ? (kDotR * 2.f + kDotGap) : 0.f;

                ImVec2 p0 = pos;
                ImVec2 p1 = ImVec2(
                    pos.x + kPadX + dot + tsz.x + kPadX,
                    pos.y + tsz.y + kPadY * 2.f
                );

                dl->AddRectFilled(
                    ImVec2(p0.x - 3.f, p0.y + 2.f),
                    ImVec2(p1.x, p1.y + 5.f),
                    IM_COL32(0, 0, 0, (int)(85.f * a)), kRounding);

                dl->AddRectFilled(p0, p1, IM_COL32(22, 20, 28, (int)(196.f * a)), kRounding);
                dl->AddRectFilled(
                    p0, ImVec2(p1.x, p0.y + (p1.y - p0.y) * 0.55f),
                    IM_COL32(255, 255, 255, (int)(10.f * a)),
                    kRounding, ImDrawFlags_RoundCornersTop);

                dl->AddRect(p0, p1, AccentCol(a * 0.35f), kRounding, 0, 1.f);

                float contentX = p0.x + kPadX;

                if (dotCol) {
                    ImU32 d = (dotCol & 0x00FFFFFF) | ((ImU32)((dotCol >> 24) * a) << 24);
                    dl->AddCircleFilled(
                        ImVec2(contentX + kDotR, p0.y + (p1.y - p0.y) * 0.5f),
                        kDotR, d, 16);
                    contentX += kDotR * 2.f + kDotGap;
                }

                ImGui::SetCursorScreenPos(ImVec2(contentX, p0.y + kPadY));
                Lavender::GradientText(text, a, -2, 2);
            };

            auto DrawHeader = [&](float rightX, float y, const char* label, float a)
            {
                ImVec2 sz = ImGui::CalcTextSize(label);
                dl->AddText(ImVec2(rightX - sz.x - 2, y + 2),
                            IM_COL32(0, 0, 0, (int)(90 * a)), label);
                dl->AddText(ImVec2(rightX - sz.x, y),
                            IM_COL32(150, 142, 168, (int)(170.f * a)), label);
            };

            using LavenderHook::Net::NetworkMonitor;
            static double last_update_t = 0.0;
            static int    disp_ping_ms  = -1;
            static std::string disp_ip;

            double now = ImGui::GetTime();
            if (now - last_update_t >= 3.0) {
                disp_ping_ms = NetworkMonitor::Instance().GetLastPingMs();
                disp_ip      = NetworkMonitor::Instance().GetTopIp();
                last_update_t = now;
            }

            static double prevTime = ImGui::GetTime();
            float dt = (float)(ImGui::GetTime() - prevTime);
            prevTime = ImGui::GetTime();

            struct SimpleAnim { float alpha = 0.f, slide = 60.f; bool visible = false; };
            static SimpleAnim serverAnim, pingAnim;

            auto TickSimpleAnim = [&](SimpleAnim& a, bool show) {
                if (show) {
                    a.alpha += (1.f - a.alpha) * std::clamp(10.f * dt, 0.f, 1.f);
                    a.slide += (0.f - a.slide) * std::clamp(10.f * dt, 0.f, 1.f);
                } else {
                    a.alpha += (0.f - a.alpha) * std::clamp(8.f * dt, 0.f, 1.f);
                    a.slide += (80.f - a.slide) * std::clamp(8.f * dt, 0.f, 1.f);
                }
                a.visible = a.alpha > 0.02f;
            };

            TickSimpleAnim(serverAnim, LavenderHook::Globals::show_server);
            TickSimpleAnim(pingAnim,   LavenderHook::Globals::show_ping);

            const float rightEdge = screenW - kMargin;
            float cursorY = kMargin;

            bool sessionVisible = serverAnim.visible || pingAnim.visible;
            if (sessionVisible) {
                DrawHeader(rightEdge, cursorY, "SESSION",
                           alpha * std::max(serverAnim.alpha, pingAnim.alpha));
                cursorY += lineH + kHeaderGap;
            }

            if (serverAnim.visible) {
                std::string server = "Server: ";
                server += disp_ip.empty() ? "unknown" : disp_ip;

                float pw = PillWidth(server, false);
                DrawPill(ImVec2(rightEdge - pw + serverAnim.slide, cursorY),
                         server, alpha * serverAnim.alpha, 0);
                cursorY += itemH + kItemSpacing;
            }

            if (pingAnim.visible) {
                std::string ping = "Ping: ";
                ping += (disp_ping_ms >= 0) ? std::to_string(disp_ping_ms) + " ms" : "--";

                ImU32 dotCol;
                if (disp_ping_ms < 0)        dotCol = IM_COL32(120, 120, 130, 255);
                else if (disp_ping_ms < 60)  dotCol = IM_COL32(120, 220, 130, 255);
                else if (disp_ping_ms < 120) dotCol = IM_COL32(240, 200, 90, 255);
                else                         dotCol = IM_COL32(235, 110, 110, 255);

                float pw = PillWidth(ping, true);
                DrawPill(ImVec2(rightEdge - pw + pingAnim.slide, cursorY),
                         ping, alpha * pingAnim.alpha, dotCol);
                cursorY += itemH + kItemSpacing;
            }

            auto activeRaw = LavenderHook::UI::Actions::GetActiveList();

            struct Measured { std::string label; float width; };
            std::vector<Measured> measured;
            measured.reserve(activeRaw.size());
            for (auto& s : activeRaw)
                measured.push_back({ s, ImGui::CalcTextSize(s.c_str()).x });

            std::sort(measured.begin(), measured.end(), [](auto& a, auto& b) {
                if (a.width != b.width) return a.width > b.width;
                return a.label < b.label;
            });

            std::vector<std::string> active;
            active.reserve(measured.size());
            for (auto& m : measured) active.push_back(m.label);

            struct AnimState { float pos; float alpha; float slide; int target; bool exiting; };
            static std::unordered_map<std::string, AnimState> anim;

            for (auto& kv : anim) kv.second.target = -1;

            for (int i = 0; i < (int)active.size(); ++i) {
                auto& label = active[i];
                auto it = anim.find(label);
                if (it == anim.end())
                    anim[label] = { (float)active.size(), 0.f, 60.f, i, false };
                else { it->second.target = i; it->second.exiting = false; }
            }

            for (auto& kv : anim)
                if (kv.second.target == -1) kv.second.exiting = true;

            for (auto& kv : anim) {
                auto& a = kv.second;
                a.pos += (a.target - a.pos) * std::clamp(12.f * dt, 0.f, 1.f);
                if (!a.exiting) {
                    a.alpha += (1.f - a.alpha) * std::clamp(10.f * dt, 0.f, 1.f);
                    a.slide += (0.f - a.slide) * std::clamp(10.f * dt, 0.f, 1.f);
                } else {
                    a.alpha += (0.f - a.alpha) * std::clamp(8.f * dt, 0.f, 1.f);
                    a.slide += (80.f - a.slide) * std::clamp(8.f * dt, 0.f, 1.f);
                }
            }

            struct DrawItem { std::string label; float pos, alpha, slide; };
            std::vector<DrawItem> draw;
            float maxActionAlpha = 0.f;

            for (auto it = anim.begin(); it != anim.end(); ) {
                if (it->second.exiting && it->second.alpha < 0.02f) {
                    it = anim.erase(it);
                } else {
                    draw.push_back({ it->first, it->second.pos, it->second.alpha, it->second.slide });
                    maxActionAlpha = std::max(maxActionAlpha, it->second.alpha);
                    ++it;
                }
            }

            std::sort(draw.begin(), draw.end(),
                      [](auto& a, auto& b) { return a.pos < b.pos; });

            if (!draw.empty()) {
                if (sessionVisible) cursorY += kClusterGap - kItemSpacing;
                DrawHeader(rightEdge, cursorY, "ACTIVE", alpha * maxActionAlpha);
                float actionsBaseY = cursorY + lineH + kHeaderGap;

                for (auto& d : draw) {
                    float pw = PillWidth(d.label, false);
                    DrawPill(
                        ImVec2(rightEdge - pw + d.slide,
                               actionsBaseY + d.pos * (itemH + kItemSpacing)),
                        d.label, alpha * d.alpha, 0);
                }
            }
        }
        else
        {
            // Simple style

            const float screenW = ImGui::GetIO().DisplaySize.x;
            const float margin = 28.f;
            const float lineH = ImGui::GetTextLineHeight();
            const float padY = 6.f;
            const float itemH = lineH + padY * 2.f;
            const float itemSpacing = 6.f;

            float cursorY = margin;

            auto DrawTextBg = [&](const ImVec2& pos, const std::string& text, float a)
                {
                    const float padX = 10.f;
                    const float rounding = 7.f;

                    ImVec2 sz = ImGui::CalcTextSize(text.c_str());

                    ImVec2 p0 = pos;
                    ImVec2 p1 = ImVec2(
                        pos.x + sz.x + padX * 2.f,
                        pos.y + sz.y + padY * 2.f
                    );

                    ImU32 bg = IM_COL32(20, 20, 20, (int)(120 * a));
                    dl->AddRectFilled(p0, p1, bg, rounding);

                    ImGui::SetCursorScreenPos(ImVec2(
                        pos.x + padX,
                        pos.y + padY
                    ));

                    Lavender::GradientText(text, a);
                };

            using LavenderHook::Net::NetworkMonitor;

            static double last_update_t = 0.0;
            static int disp_ping_ms = -1;
            static std::string disp_ip;

            double now = ImGui::GetTime();
            if (now - last_update_t >= 3.0)
            {
                disp_ping_ms = NetworkMonitor::Instance().GetLastPingMs();
                disp_ip = NetworkMonitor::Instance().GetTopIp();
                last_update_t = now;
            }

            struct SimpleAnim { float alpha = 0.f, slide = 60.f; bool visible = false; };
            static SimpleAnim serverAnim, pingAnim;

            static double prevTime = ImGui::GetTime();
            float dt = (float)(ImGui::GetTime() - prevTime);
            prevTime = ImGui::GetTime();

            auto TickSimpleAnim = [&](SimpleAnim& a, bool show)
                {
                    if (show) {
                        a.alpha += (1.f - a.alpha) * std::clamp(10.f * dt, 0.f, 1.f);
                        a.slide += (0.f - a.slide) * std::clamp(10.f * dt, 0.f, 1.f);
                    }
                    else {
                        a.alpha += (0.f - a.alpha) * std::clamp(8.f * dt, 0.f, 1.f);
                        a.slide += (80.f - a.slide) * std::clamp(8.f * dt, 0.f, 1.f);
                    }
                    a.visible = a.alpha > 0.02f;
                };

            TickSimpleAnim(serverAnim, LavenderHook::Globals::show_server);
            TickSimpleAnim(pingAnim, LavenderHook::Globals::show_ping);

            if (serverAnim.visible)
            {
                std::string server = "Server: ";
                server += disp_ip.empty() ? "unknown" : disp_ip;

                float w = ImGui::CalcTextSize(server.c_str()).x;
                DrawTextBg(
                    ImVec2(
                        screenW - w - margin - 18.f + serverAnim.slide,
                        cursorY
                    ),
                    server,
                    alpha * serverAnim.alpha
                );
                cursorY += itemH + itemSpacing;
            }

            if (pingAnim.visible)
            {
                std::string ping = "Ping: ";
                ping += (disp_ping_ms >= 0)
                    ? std::to_string(disp_ping_ms) + " ms"
                    : "--";

                float w = ImGui::CalcTextSize(ping.c_str()).x;
                DrawTextBg(
                    ImVec2(
                        screenW - w - margin - 18.f + pingAnim.slide,
                        cursorY
                    ),
                    ping,
                    alpha * pingAnim.alpha
                );
                cursorY += itemH + itemSpacing;
            }

            cursorY += 12.f;
            float actionsBaseY = cursorY;

            auto activeRaw = LavenderHook::UI::Actions::GetActiveList();

            struct Measured { std::string label; float width; };
            std::vector<Measured> measured;

            for (auto& s : activeRaw)
                measured.push_back({ s, ImGui::CalcTextSize(s.c_str()).x });

            std::sort(measured.begin(), measured.end(),
                [](auto& a, auto& b) {
                    if (a.width != b.width) return a.width > b.width;
                    return a.label < b.label;
                });

            std::vector<std::string> active;
            for (auto& m : measured)
                active.push_back(m.label);

            struct AnimState {
                float pos;
                float alpha;
                float slide;
                int target;
                bool exiting;
            };

            static std::unordered_map<std::string, AnimState> anim;

            for (auto& kv : anim)
                kv.second.target = -1;

            for (int i = 0; i < (int)active.size(); ++i)
            {
                auto& label = active[i];
                auto it = anim.find(label);

                if (it == anim.end())
                    anim[label] = { (float)active.size(), 0.f, 60.f, i, false };
                else {
                    it->second.target = i;
                    it->second.exiting = false;
                }
            }

            for (auto& kv : anim)
                if (kv.second.target == -1)
                    kv.second.exiting = true;

            for (auto& kv : anim)
            {
                auto& a = kv.second;
                a.pos += (a.target - a.pos) * std::clamp(12.f * dt, 0.f, 1.f);

                if (!a.exiting) {
                    a.alpha += (1.f - a.alpha) * std::clamp(10.f * dt, 0.f, 1.f);
                    a.slide += (0.f - a.slide) * std::clamp(10.f * dt, 0.f, 1.f);
                }
                else {
                    a.alpha += (0.f - a.alpha) * std::clamp(8.f * dt, 0.f, 1.f);
                    a.slide += (80.f - a.slide) * std::clamp(8.f * dt, 0.f, 1.f);
                }
            }

            struct DrawItem { std::string label; float pos, alpha, slide; };
            std::vector<DrawItem> draw;

            for (auto it = anim.begin(); it != anim.end(); )
            {
                if (it->second.exiting && it->second.alpha < 0.02f)
                    it = anim.erase(it);
                else {
                    draw.push_back({ it->first, it->second.pos, it->second.alpha, it->second.slide });
                    ++it;
                }
            }

            std::sort(draw.begin(), draw.end(),
                [](auto& a, auto& b) { return a.pos < b.pos; });

            for (auto& d : draw)
            {
                float w = ImGui::CalcTextSize(d.label.c_str()).x;
                DrawTextBg(
                    ImVec2(
                        screenW - w - margin - 18.f + d.slide,
                        actionsBaseY + d.pos * (itemH + itemSpacing)
                    ),
                    d.label,
                    alpha * d.alpha
                );
            }
        }

        ImGui::End();
        ImGui::PopStyleVar(3);
    }

} // namespace LavenderHook::UI::Windows

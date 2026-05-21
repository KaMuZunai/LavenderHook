#include "WebhookManager.h"
#include "../misc/logmonitor/LogMonitor.h"
#include <Windows.h>
#include <winhttp.h>
#include <thread>
#include <sstream>
#include <chrono>

namespace LavenderHook::Webhook {

    std::string url;
    std::string user_id;
    std::string map_finished_msg = "@User Your Map has finished.";
    std::string core_destroyed_msg = "@User Your Core has been destroyed.";
    bool map_finished_enabled = false;
    bool core_destroyed_enabled = false;

    namespace {
        bool s_mapFinishedSent = false;
        bool s_coreDestroyedSent = false;

        static std::string JsonEscape(const std::string& s)
        {
            std::string r;
            r.reserve(s.size());
            for (char c : s) {
                switch (c) {
                case '"':  r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\n': r += "\\n";  break;
                case '\r': r += "\\r";  break;
                case '\t': r += "\\t";  break;
                default:   r += c;      break;
                }
            }
            return r;
        }

        static std::string FormatMessage(const std::string& tmpl)
        {
            auto session = LavenderHook::LogMonitor::GetCurrentSession();
            int wave = LavenderHook::LogMonitor::GetCurrentWave();
            std::string result = tmpl;

            auto ReplaceAll = [&](const std::string& from, const std::string& to) {
                size_t pos = 0;
                while ((pos = result.find(from, pos)) != std::string::npos) {
                    result.replace(pos, from.size(), to);
                    pos += to.size();
                }
            };

            auto StripPrefix = [](const std::string& s) -> std::string {
                size_t dot = s.rfind('.');
                return (dot != std::string::npos) ? s.substr(dot + 1) : s;
            };

            // @User -> <@USER_ID>
            if (!user_id.empty())
            {
                size_t pos = 0;
                while ((pos = result.find("@User", pos)) != std::string::npos) {
                    std::string ping = "<@" + user_id + ">";
                    result.replace(pos, 5, ping);
                    pos += ping.size();
                }
            }

            ReplaceAll("@Wave", std::to_string(wave));

            {
                std::string gm = StripPrefix(session.gameMode);
                ReplaceAll("@Gamemode", gm);
            }

            {
                std::string diff = StripPrefix(session.gameDifficulty);
                ReplaceAll("@Difficulty", diff);
            }

            {
                std::string mods;
                if (session.hardcore && session.riftMode) mods = "Hardcore Rifted";
                else if (session.hardcore) mods = "Hardcore";
                else if (session.riftMode) mods = "Rifted";
                ReplaceAll("@Modifiers", mods);
            }

            {
                std::string bonus = StripPrefix(session.bonusWave);
                ReplaceAll("@Bonus", bonus);
            }

            {
                std::string map = StripPrefix(session.mapName);
                for (auto& c : map) if (c == '_') c = ' ';
                ReplaceAll("@Map", map);
            }

            return result;
        }

        static void SendWebhook(const std::string& message)
        {
            if (url.empty() || message.empty())
                return;

            URL_COMPONENTS urlComp = { sizeof(URL_COMPONENTS) };
            urlComp.dwStructSize = sizeof(urlComp);

            wchar_t hostName[256] = {};
            wchar_t urlPath[2048] = {};
            urlComp.lpszHostName = hostName;
            urlComp.dwHostNameLength = 256;
            urlComp.lpszUrlPath = urlPath;
            urlComp.dwUrlPathLength = 2048;
            urlComp.dwSchemeLength = 32;

            std::wstring wurl(url.begin(), url.end());
            if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.length(), 0, &urlComp))
                return;

            HINTERNET hSession = WinHttpOpen(L"LavenderHook/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
            if (!hSession) return;

            HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
            if (!hConnect) { WinHttpCloseHandle(hSession); return; }

            DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath,
                NULL, NULL, NULL, flags);
            if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return; }

            std::string jsonStr = "{\"content\":\"" + JsonEscape(message) + "\"";
            if (!user_id.empty()) {
                jsonStr += ",\"allowed_mentions\":{\"users\":[\"" + JsonEscape(user_id) + "\"]}";
            }
            jsonStr += "}";

            LPCWSTR whdrs = L"Content-Type: application/json\r\n";

            WinHttpSendRequest(hRequest, whdrs, -1,
                (LPVOID)jsonStr.data(), (DWORD)jsonStr.size(), (DWORD)jsonStr.size(), 0);

            WinHttpReceiveResponse(hRequest, NULL);

            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
        }

        static bool s_lastPreSummary = false;
        static bool s_lastCombatAborted = false;
        static std::chrono::steady_clock::time_point s_coreDestroyedTime{};
    }

    void Update()
    {
        if (LavenderHook::LogMonitor::IsInTavern())
        {
            s_mapFinishedSent = false;
            s_coreDestroyedSent = false;
            s_lastPreSummary = false;
            s_lastCombatAborted = false;
            s_coreDestroyedTime = {};
            return;
        }

        bool preSummary = LavenderHook::LogMonitor::IsPreSummaryPhase();
        bool combatAborted = LavenderHook::LogMonitor::IsCombatAborted();

        // Reset send flags when signals return to false
        if (!preSummary)
            s_mapFinishedSent = false;
        if (!combatAborted)
            s_coreDestroyedSent = false;

        // Core Destroyed — rising edge on combat abort
        if (core_destroyed_enabled && combatAborted && !s_lastCombatAborted && !s_coreDestroyedSent)
        {
            s_coreDestroyedSent = true;
            s_coreDestroyedTime = std::chrono::steady_clock::now();
            std::string msg = FormatMessage(core_destroyed_msg);
            std::thread([msg]() { SendWebhook(msg); }).detach();
        }
        s_lastCombatAborted = combatAborted;

        // Map Finished — rising edge on pre-summary phase
        if (map_finished_enabled && preSummary && !s_lastPreSummary && !s_mapFinishedSent)
        {
            auto now = std::chrono::steady_clock::now();
            auto sinceCore = std::chrono::duration_cast<std::chrono::seconds>(now - s_coreDestroyedTime).count();
            if (sinceCore >= 3 || !core_destroyed_enabled)
            {
                s_mapFinishedSent = true;
                std::string msg = FormatMessage(map_finished_msg);
                std::thread([msg]() { SendWebhook(msg); }).detach();
            }
        }
        s_lastPreSummary = preSummary;
    }

} // namespace LavenderHook::Webhook

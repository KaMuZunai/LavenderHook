#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WIN32_WINNT 0x0A00
#include <sdkddkver.h>
#include <windows.h>
#include <winhttp.h>

#include "WikiWindow.h"
#include "../../misc/Globals.h"
#include "../../imgui/imgui.h"
#include "../../imgui/imgui_internal.h"
#include "../../assets/TextureLoader.h"
#include "../../assets/UITextures.h"
#include "../../ui/components/LavenderFadeOut.h"
#include "../../ui/components/LavenderGradient.h"
#include "../../ui/components/LavenderWindowHeader.h"
#include "../../ui/components/LavenderUI.h"
#include "../../ui/GUI.h"

extern ImVec4 MAIN_RED;
extern ImVec4 MID_RED;
extern ImVec4 DARK_RED;

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <fstream>
#include <cmath>
#include <filesystem>

static const char* kWikiZipUrl = "https://github.com/KaMuZunai/LavenderHookWiki/archive/refs/heads/main.zip";

namespace fs = std::filesystem;

namespace LavenderHook::UI::Windows {

    const std::vector<std::pair<std::string, std::vector<std::string>>> WikiWindow::kCategories = {
        {"Accessories", {"Bracer", "Brooch", "Mask"}},
        {"Armor", {"Boots", "Chest", "Gloves", "Helmet"}},
        {"Offhand", {"AmmoPouch", "Shield", "Tome", "Whetstone"}},
        {"Pets", {}},
        {"Runes", {}},
        {"Weapons", {"Crossbow", "Polearm", "Staff", "Sword"}}
    };

    std::string WikiWindow::m_cacheDir;
    std::string WikiWindow::m_manifestUrl = "https://raw.githubusercontent.com/KaMuZunai/LavenderHookWiki/refs/heads/main/manifest.json";

    std::vector<ManifestEntry> WikiWindow::m_manifest;
    std::unordered_map<std::string, WikiItem> WikiWindow::m_items;
    bool WikiWindow::m_manifestLoaded = false;
    bool WikiWindow::m_manifestLoading = false;
    int WikiWindow::m_manifestFetchId = -1;

    std::string WikiWindow::m_activeCategory = "Weapons";
    std::string WikiWindow::m_activeType;
    std::string WikiWindow::m_searchQuery;
    int WikiWindow::m_currentPage = 0;
    int WikiWindow::m_itemsPerPage = 50;

    std::string WikiWindow::m_selectedItemPath;
    bool WikiWindow::m_showDetailPopup = false;

    std::unordered_set<std::string> WikiWindow::m_favorites;

    std::vector<std::thread> WikiWindow::m_fetchThreads;
    std::mutex WikiWindow::m_fetchMutex;
    std::queue<FetchJob*> WikiWindow::m_fetchQueue;
    std::vector<FetchJob*> WikiWindow::m_fetchResults;
    std::atomic<bool> WikiWindow::m_fetchThreadRunning{ false };
    int WikiWindow::m_nextFetchId = 0;

    bool WikiWindow::m_initialized = false;
    std::atomic<bool> WikiWindow::m_wikiDownloading{ false };
    std::atomic<bool> WikiWindow::m_wikiDownloaded{ false };
    int WikiWindow::kFetchThreadCount = 0;
    int WikiWindow::m_loadedIconCount = 0;
    std::unordered_set<std::string> WikiWindow::m_visibleItems;
    std::unordered_map<std::string, Texture> WikiWindow::m_iconCache;

    static LavenderHook::UI::LavenderFadeOut s_wikiFade;
    static LavenderHook::UI::LavenderFadeOut s_favOverlayFade;
    static float s_favOverlayOpen = 0.0f;

    static bool s_headerOpen = true;
    static float s_headerAnim = 1.0f;
    static float s_arrowAnim = 0.0f;
    static std::string s_cachedManifestTimestamp;

    // Theme accents (defined in GUI.cpp) — drive the wiki's accent coloring so
    // it matches the menu instead of using a hardcoded purple.
    static ImVec4 kPillTextColor = ImVec4(1.0f, 1.0f, 1.0f, 0.9f);
    static ImVec4 kFavoriteColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);

    // Accent helpers (tie former hardcoded purple to MAIN_RED/MID_RED).
    static ImVec4 WikiAccent(float a)  { return ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, a); }
    static ImVec4 WikiAccentHi(float a){ return ImVec4(MID_RED.x,  MID_RED.y,  MID_RED.z,  a); }

    // Scale helper
    static float WS() { return LavenderHook::Globals::menu_scale; }

    // Helpers
    static std::string GetConfigDir() {
        char* app = nullptr; size_t len = 0;
        std::string dir = ".";
        if (_dupenv_s(&app, &len, "APPDATA") == 0 && app) {
            dir = app;
            free(app);
        }
        return dir + "\\LavenderHook";
    }

    std::string WikiWindow::GetWikiCacheDir() {
        if (m_cacheDir.empty()) {
            m_cacheDir = GetConfigDir() + "\\LavenderWiki";
            fs::create_directories(m_cacheDir);
        }
        return m_cacheDir;
    }

    std::string WikiWindow::GetCacheFilePath(const std::string& cacheKey) {
        std::string safe = cacheKey;
        for (auto& c : safe) {
            if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '<' || c == '>' || c == '|' || c == '"')
                c = '_';
        }
        return GetWikiCacheDir() + "\\" + safe;
    }

    bool WikiWindow::CacheExists(const std::string& cacheKey) {
        return fs::exists(GetCacheFilePath(cacheKey));
    }

    void WikiWindow::SaveToCache(const std::string& cacheKey, const std::string& data) {
        std::string path = GetCacheFilePath(cacheKey);
        std::ofstream f(path, std::ios::binary);
        if (f) f.write(data.data(), data.size());
    }

    std::string WikiWindow::LoadFromCache(const std::string& cacheKey) {
        std::string path = GetCacheFilePath(cacheKey);
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) return "";
        size_t size = (size_t)f.tellg();
        f.seekg(0);
        std::string data(size, '\0');
        f.read(data.data(), size);
        return data;
    }

    bool WikiWindow::DownloadAndExtractWiki() {
        std::string cacheDir = GetWikiCacheDir();
        // Delete old cache
        fs::remove_all(cacheDir);
        fs::create_directories(cacheDir);

        // Download ZIP
        std::string zipData = FetchUrlSync(kWikiZipUrl);
        if (zipData.empty()) return false;

        // Save ZIP to temp file
        std::string zipPath = cacheDir + "\\wiki.zip";
        {
            std::ofstream f(zipPath, std::ios::binary);
            if (!f) return false;
            f.write(zipData.data(), zipData.size());
        }

        // Extract using PowerShell Expand-Archive (built into Windows 10+)
        std::string cmd = "powershell -NoProfile -Command \"Expand-Archive -Path '" + zipPath + "' -DestinationPath '" + cacheDir + "' -Force\"";
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        std::wstring wcmd(cmd.begin(), cmd.end());
        if (!CreateProcessW(NULL, &wcmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            fs::remove(zipPath);
            return false;
        }
        WaitForSingleObject(pi.hProcess, 60000); // wait up to 60s
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        fs::remove(zipPath);

        // Move files from nested folder (LavenderHookWiki-main/) to cache root
        std::string nested = cacheDir + "\\LavenderHookWiki-main";
        if (fs::exists(nested)) {
            for (auto& entry : fs::recursive_directory_iterator(nested)) {
                if (entry.is_regular_file()) {
                    std::string relPath = entry.path().string().substr(nested.length() + 1);
                    std::string dest = cacheDir + "\\" + relPath;
                    fs::create_directories(fs::path(dest).parent_path());
                    fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing);
                }
            }
            fs::remove_all(nested);
        }

        return true;
    }

    std::string WikiWindow::ReadFileFromCache(const std::string& relativePath) {
        std::string fullPath = GetWikiCacheDir() + "\\" + relativePath;
        std::ifstream f(fullPath, std::ios::binary | std::ios::ate);
        if (!f) return {};
        size_t sz = (size_t)f.tellg();
        f.seekg(0);
        std::string data(sz, '\0');
        if (sz > 0) f.read(data.data(), sz);
        return data;
    }

    std::string WikiWindow::NormalizePath(const std::string& path) {
        std::string n = path;
        for (auto& c : n) if (c == '\\') c = '/';
        return n;
    }

    static std::string UrlEncodePath(const std::string& path) {
        std::string r;
        for (char c : path) {
            if (c == ' ') r += "%20";
            else r += c;
        }
        return r;
    }

    std::string WikiWindow::GetItemJsonUrl(const std::string& folderPath) {
        std::string n = UrlEncodePath(NormalizePath(folderPath));
        return "https://raw.githubusercontent.com/KaMuZunai/LavenderHookWiki/refs/heads/main/" + n + "/item.json";
    }

    std::string WikiWindow::GetIconUrl(const std::string& folderPath, const std::string& iconName) {
        std::string n = UrlEncodePath(NormalizePath(folderPath));
        std::string enc;
        for (char c : iconName) {
            if (c == ' ') enc += "%20";
            else enc += c;
        }
        return "https://raw.githubusercontent.com/KaMuZunai/LavenderHookWiki/refs/heads/main/" + n + "/" + enc;
    }

    static std::string GetDisplayName(const ManifestEntry& entry) {
        size_t lastSlash = entry.path.rfind('\\');
        if (lastSlash == std::string::npos) lastSlash = entry.path.rfind('/');
        std::string folder = (lastSlash != std::string::npos) ? entry.path.substr(lastSlash + 1) : entry.path;
        if (folder == entry.name)
            return entry.name;

        // Try exact prefix match first
        if (folder.rfind(entry.name, 0) == 0) {
            std::string suffix = folder.substr(entry.name.length());
            while (!suffix.empty() && (suffix[0] == '_' || suffix[0] == ' '))
                suffix = suffix.substr(1);
            if (!suffix.empty()) {
                for (auto& c : suffix) if (c == '_') c = ' ';
                return entry.name + " (" + suffix + ")";
            }
        }

        // Try with spaces stripped (handles "FaceMelter_Tower" vs "Face Melter")
        std::string folderCompact, nameCompact;
        for (char c : folder) if (c != ' ') folderCompact += c;
        for (char c : entry.name) if (c != ' ') nameCompact += c;
        if (folderCompact.length() > nameCompact.length() && folderCompact.rfind(nameCompact, 0) == 0) {
            // Count how many chars of entry.name matched (accounting for stripped spaces)
            size_t matched = 0;
            for (size_t fi = 0, ni = 0; ni < entry.name.length() && fi < folder.length(); fi++) {
                if (folder[fi] == ' ') continue;
                if (entry.name[ni] == ' ') { ni++; fi--; continue; }
                if (folder[fi] == entry.name[ni]) { matched = fi + 1; ni++; }
                else break;
            }
            std::string suffix = folder.substr(matched);
            while (!suffix.empty() && (suffix[0] == '_' || suffix[0] == ' '))
                suffix = suffix.substr(1);
            if (!suffix.empty()) {
                for (auto& c : suffix) if (c == '_') c = ' ';
                return entry.name + " (" + suffix + ")";
            }
        }

        return entry.name;
    }

    std::string WikiWindow::FetchUrlSync(const std::string& url) {
        std::string result;
        URL_COMPONENTSW urlComp = { sizeof(urlComp) };
        urlComp.dwSchemeLength = (DWORD)-1;
        urlComp.dwHostNameLength = (DWORD)-1;
        urlComp.dwUrlPathLength = (DWORD)-1;

        std::wstring wurl(url.begin(), url.end());
        if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &urlComp))
            return result;

        std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
        std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);

        HINTERNET hSession = WinHttpOpen(L"LavenderHook-Wiki/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return result;

        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return result; }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }

        DWORD dwRedirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &dwRedirect, sizeof(dwRedirect));

        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hRequest, nullptr)) {
            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);
            if (!WinHttpQueryHeaders(hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode, &statusCodeSize,
                WINHTTP_NO_HEADER_INDEX) || statusCode != 200) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return result;
            }
            DWORD bytesAvail = 0;
            while (WinHttpQueryDataAvailable(hRequest, &bytesAvail) && bytesAvail > 0) {
                std::string chunk(bytesAvail, '\0');
                DWORD bytesRead = 0;
                if (WinHttpReadData(hRequest, &chunk[0], bytesAvail, &bytesRead))
                    result.append(chunk.data(), bytesRead);
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    static std::string Trim(const std::string& s) {
        size_t st = 0;
        while (st < s.size() && (s[st] == ' ' || s[st] == '\t' || s[st] == '\r' || s[st] == '\n')) st++;
        if (st == s.size()) return "";
        size_t en = s.size() - 1;
        while (en > st && (s[en] == ' ' || s[en] == '\t' || s[en] == '\r' || s[en] == '\n')) en--;
        return s.substr(st, en - st + 1);
    }

    // JSON helpers
    static std::string UnescapeJson(const std::string& s) {
        std::string r;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                switch (s[i + 1]) {
                case '"': r += '"'; i++; break;
                case '\\': r += '\\'; i++; break;
                case '/': r += '/'; i++; break;
                case 'n': r += '\n'; i++; break;
                case 'r': r += '\r'; i++; break;
                case 't': r += '\t'; i++; break;
                case 'u': {
                    if (i + 5 < s.size()) {
                        unsigned int cp = 0;
                        sscanf_s(s.substr(i + 2, 4).c_str(), "%x", &cp);
                        if (cp <= 0x7F) r += (char)cp;
                        else if (cp <= 0x7FF) { r += (char)(0xC0 | (cp >> 6)); r += (char)(0x80 | (cp & 0x3F)); }
                        else { r += (char)(0xE0 | (cp >> 12)); r += (char)(0x80 | ((cp >> 6) & 0x3F)); r += (char)(0x80 | (cp & 0x3F)); }
                        i += 5;
                    }
                    break;
                }
                default: r += s[i]; break;
                }
            }
            else r += s[i];
        }
        return r;
    }

    static std::string ExtractString(const std::string& json, size_t& pos) {
        if (pos >= json.size() || json[pos] != '"') return "";
        pos++;
        std::string r;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                r += json[pos]; r += json[pos + 1]; pos += 2;
            }
            else { r += json[pos]; pos++; }
        }
        if (pos < json.size()) pos++;
        return UnescapeJson(r);
    }

    static void SkipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
            pos++;
    }

    static std::string ExtractStringValueByKey(const std::string& json, size_t& pos, const std::string& key) {
        SkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == '"') {
            std::string k = ExtractString(json, pos);
            SkipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ':') {
                pos++;
                SkipWhitespace(json, pos);
                if (pos < json.size() && json[pos] == '"') {
                    return ExtractString(json, pos);
                }
            }
        }
        return "";
    }

    static std::vector<std::string> ExtractStringArrayByKey(const std::string& json, size_t& pos, const std::string& key) {
        std::vector<std::string> r;
        SkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == '"') {
            std::string k = ExtractString(json, pos);
            if (k == key) {
                SkipWhitespace(json, pos);
                if (pos < json.size() && json[pos] == ':') {
                    pos++;
                    SkipWhitespace(json, pos);
                    if (pos < json.size() && json[pos] == '[') {
                        pos++;
                        while (pos < json.size() && json[pos] != ']') {
                            SkipWhitespace(json, pos);
                            if (pos < json.size() && json[pos] == '"') {
                                r.push_back(ExtractString(json, pos));
                            }
                            else {
                                if (pos < json.size()) pos++;
                            }
                            SkipWhitespace(json, pos);
                            if (pos < json.size() && json[pos] == ',') pos++;
                        }
                        if (pos < json.size()) pos++;
                    }
                }
            }
        }
        return r;
    }

    static bool ExtractBoolByKey(const std::string& json, size_t& pos, const std::string& key, bool def) {
        SkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == '"') {
            std::string k = ExtractString(json, pos);
            if (k == key) {
                SkipWhitespace(json, pos);
                if (pos < json.size() && json[pos] == ':') {
                    pos++;
                    SkipWhitespace(json, pos);
                    if (pos + 4 <= json.size()) {
                        std::string rest = json.substr(pos, 5);
                        if (rest.substr(0, 4) == "true") { pos += 4; return true; }
                        if (rest.substr(0, 5) == "false") { pos += 5; return false; }
                    }
                }
            }
        }
        return def;
    }

    // Helper that scans linearly for a key in a JSON object (doesn't use pos)
    static std::string FindStringValue(const std::string& obj, const std::string& key) {
        std::string search = "\"" + key + "\"";
        size_t p = obj.find(search);
        if (p == std::string::npos) return "";
        p = obj.find(':', p + search.size());
        if (p == std::string::npos) return "";
        p++;
        while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' || obj[p] == '\n' || obj[p] == '\r')) p++;
        if (p < obj.size() && obj[p] == '"') {
            p++;
            std::string r;
            while (p < obj.size() && obj[p] != '"') {
                if (obj[p] == '\\' && p + 1 < obj.size()) { r += obj[p]; r += obj[p + 1]; p += 2; }
                else { r += obj[p]; p++; }
            }
            return UnescapeJson(r);
        }
        return "";
    }

    static std::vector<std::string> FindStringArray(const std::string& obj, const std::string& key) {
        std::vector<std::string> r;
        std::string search = "\"" + key + "\"";
        size_t p = obj.find(search);
        if (p == std::string::npos) return r;
        p = obj.find(':', p + search.size());
        if (p == std::string::npos) return r;
        p++;
        while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' || obj[p] == '\n' || obj[p] == '\r')) p++;
        if (p < obj.size() && obj[p] == '[') {
            p++;
            while (p < obj.size() && obj[p] != ']') {
                while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' || obj[p] == '\n' || obj[p] == '\r' || obj[p] == ',')) p++;
                if (p < obj.size() && obj[p] == '"') {
                    p++;
                    std::string val;
                    while (p < obj.size() && obj[p] != '"') {
                        if (obj[p] == '\\' && p + 1 < obj.size()) { val += obj[p + 1]; p += 2; }
                        else { val += obj[p]; p++; }
                    }
                    if (p < obj.size()) p++;
                    r.push_back(UnescapeJson(val));
                }
            }
        }
        return r;
    }

    static bool FindBool(const std::string& obj, const std::string& key, bool def) {
        std::string search = "\"" + key + "\"";
        size_t p = obj.find(search);
        if (p == std::string::npos) return def;
        p = obj.find(':', p + search.size());
        if (p == std::string::npos) return def;
        p++;
        while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' || obj[p] == '\n' || obj[p] == '\r')) p++;
        if (p + 4 <= obj.size() && obj.substr(p, 4) == "true") return true;
        if (p + 5 <= obj.size() && obj.substr(p, 5) == "false") return false;
        return def;
    }

    static std::vector<std::string> FindStringArrayRaw(const std::string& obj, const std::string& key) {
        return FindStringArray(obj, key);
    }

    // Item JSON parsing
    static void ParseItemJsonData(WikiItem& item, const std::string& json) {
        if (json.empty()) return;
        item.name = FindStringValue(json, "name");
        item.description = FindStringValue(json, "description");
        item.icon = FindStringValue(json, "icon");
        item.category = FindStringValue(json, "category");
        item.type = FindStringValue(json, "type");
        item.subtype = FindStringArray(json, "subtype");
        item.stats = FindStringArray(json, "stats");
        item.elements = FindStringArray(json, "elements");
        item.dropLocations = FindStringArray(json, "dropLocations");
        if (item.dropLocations.empty()) {
            std::string singleDrop = FindStringValue(json, "dropLocations");
            if (!singleDrop.empty())
                item.dropLocations.push_back(singleDrop);
        }
        item.loaded = true;
        item.loading = false;
    }

    static void ParseItemJsonFromResult(WikiItem& item, const std::string& result) {
        ParseItemJsonData(item, result);
    }

    // Fetch thread
    void WikiWindow::StartFetchThread() {
        if (m_fetchThreadRunning) return;
        m_fetchThreadRunning = true;
        if (kFetchThreadCount <= 0) {
            kFetchThreadCount = (int)std::thread::hardware_concurrency();
            if (kFetchThreadCount < 2) kFetchThreadCount = 2;
            if (kFetchThreadCount > 16) kFetchThreadCount = 16;
        }
        m_fetchThreads.reserve(kFetchThreadCount);
        for (int i = 0; i < kFetchThreadCount; i++)
            m_fetchThreads.emplace_back(FetchThreadProc);
    }

    void WikiWindow::FetchThreadProc() {
        while (m_fetchThreadRunning) {
            FetchJob* job = nullptr;
            {
                std::lock_guard<std::mutex> lock(m_fetchMutex);
                if (!m_fetchQueue.empty()) {
                    job = m_fetchQueue.front();
                    m_fetchQueue.pop();
                }
            }

            if (job) {
                std::string data = FetchUrlSync(job->url);
                job->success = !data.empty();
                job->result = data;

                if (job->success) {
                    SaveToCache(job->cacheKey, data);
                }

                {
                    std::lock_guard<std::mutex> lock(m_fetchMutex);
                    job->done = true;
                    m_fetchResults.push_back(job);
                }
            }
            else {
                Sleep(10);
            }
        }
    }

    int WikiWindow::EnqueueFetch(const std::string& url, const std::string& cacheKey) {
        auto* job = new FetchJob();
        job->id = m_nextFetchId++;
        job->url = url;
        job->cacheKey = cacheKey;
        job->done = false;

        {
            std::lock_guard<std::mutex> lock(m_fetchMutex);
            m_fetchQueue.push(job);
        }
        return job->id;
    }

    void WikiWindow::ProcessFetches() {
        std::vector<FetchJob*> results;
        {
            std::lock_guard<std::mutex> lock(m_fetchMutex);
            results.swap(m_fetchResults);
        }

        for (auto* job : results) {
            if (!job->success && CacheExists(job->cacheKey)) {
                job->result = LoadFromCache(job->cacheKey);
                job->success = true;
            }

            if (job->cacheKey == "manifest" && job->success) {
                std::string prevTimestamp = s_cachedManifestTimestamp;
                ParseManifestJson(job->result);
                if (m_manifestLoaded && !prevTimestamp.empty() && prevTimestamp != s_cachedManifestTimestamp) {
                    // Manifest changed - clear all caches so items re-download
                    m_items.clear();
                    FreeAllIcons();
                    // Delete cached item.json files from disk
                    for (auto& entry : fs::directory_iterator(GetWikiCacheDir())) {
                        std::string name = entry.path().filename().string();
                        if (name.rfind("item_", 0) == 0 || name.rfind("icon_", 0) == 0) {
                            fs::remove(entry.path());
                        }
                    }
                }
                m_manifestLoaded = true;
                m_manifestLoading = false;
            }
            else if (job->success && job->cacheKey.rfind("item_", 0) == 0) {
                std::string folderPath = job->cacheKey.substr(5);
                auto it = m_items.find(folderPath);
                if (it != m_items.end()) {
                    ParseItemJsonFromResult(it->second, job->result);
                }
            }
            else if (job->success && job->cacheKey.rfind("icon_", 0) == 0) {
                std::string folderAndIcon = job->cacheKey.substr(5);
                size_t sep = folderAndIcon.find('|');
                if (sep != std::string::npos) {
                    std::string folderPath = folderAndIcon.substr(0, sep);
                    if (m_iconCache.find(folderPath) == m_iconCache.end() && TextureLoader::IsInitialized() && m_loadedIconCount < kMaxLoadedIcons) {
                        std::string cachePath = GetCacheFilePath(job->cacheKey);
                        Texture tex = TextureLoader::LoadFromFile(cachePath.c_str());
                        if (tex.IsValid()) {
                            m_iconCache[folderPath] = tex;
                            m_loadedIconCount++;
                        }
                    }
                }
            }

            delete job;
        }
    }

    // Favorites
    static std::string GetFavoritesPath() {
        return GetConfigDir() + "\\wiki_favorites.ini";
    }

    static void LoadFavorites() {
        WikiWindow::m_favorites.clear();
        std::ifstream f(GetFavoritesPath());
        if (!f) return;
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty())
                WikiWindow::m_favorites.insert(line);
        }
    }

    static void SaveFavorites() {
        std::ofstream f(GetFavoritesPath(), std::ios::trunc);
        if (!f) return;
        for (const auto& p : WikiWindow::m_favorites)
            f << p << "\n";
    }

    // Manifest parsing
    void WikiWindow::ParseManifestJson(const std::string& json) {
        m_manifest.clear();

        // Extract generatedAt for cache freshness
        s_cachedManifestTimestamp = FindStringValue(json, "generatedAt");

        // Find the "items" array
        size_t itemsPos = json.find("\"items\"");
        if (itemsPos == std::string::npos) return;
        itemsPos = json.find('[', itemsPos + 7);
        if (itemsPos == std::string::npos) return;
        itemsPos++;

        int depth = 0;
        size_t objStart = std::string::npos;
        for (size_t i = itemsPos; i < json.size(); i++) {
            if (json[i] == '{') {
                if (depth == 0) objStart = i;
                depth++;
            }
            else if (json[i] == '}') {
                depth--;
                if (depth == 0 && objStart != std::string::npos) {
                    std::string obj = json.substr(objStart, i - objStart + 1);

                    ManifestEntry entry;
                    entry.name = FindStringValue(obj, "name");
                    entry.icon = FindStringValue(obj, "icon");
                    entry.category = FindStringValue(obj, "category");
                    entry.type = FindStringValue(obj, "type");
                    entry.path = FindStringValue(obj, "path");
                    entry.enabled = FindBool(obj, "enabled", true);
                    entry.subtype = FindStringArray(obj, "subtype");

                    if (!entry.name.empty() && entry.enabled)
                        m_manifest.push_back(entry);

                    objStart = std::string::npos;
                }
            }
        }
    }

    void WikiWindow::ParseManifest() {
        if (m_manifestLoading) return;
        m_manifestLoading = true;

        // Read manifest.json from extracted wiki files
        std::string manifestContent = ReadFileFromCache("manifest.json");
        if (!manifestContent.empty()) {
            ParseManifestJson(manifestContent);
            m_manifestLoaded = true;
        }

        m_manifestLoading = false;
    }

    // Loading
    void WikiWindow::EnsureItemLoaded(const std::string& folderPath) {
        if (m_items.find(folderPath) != m_items.end()) {
            auto& item = m_items[folderPath];
            if (item.loaded || item.loading) return;
        }

        WikiItem& item = m_items[folderPath];
        item.path = folderPath;
        item.loading = true;

        // Read item.json from extracted wiki files
        std::string normalizedPath = folderPath;
        for (auto& c : normalizedPath) if (c == '\\') c = '/';
        std::string itemJson = ReadFileFromCache(normalizedPath + "/item.json");
        if (!itemJson.empty()) {
            ParseItemJsonData(item, itemJson);
        } else {
            item.loading = false;
        }
    }

    void WikiWindow::DrawCategoryTabs() {
        float sw = WS();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4 * sw, 4 * sw));

        float availW = ImGui::GetContentRegionAvail().x;
        float tabH = ImGui::GetFrameHeight() + 4 * sw;
        float startX = ImGui::GetCursorScreenPos().x;
        float curX = startX;
        float curY = ImGui::GetCursorScreenPos().y;

        for (const auto& cat : kCategories) {
            const std::string& name = cat.first;
            ImVec2 ts = ImGui::CalcTextSize(name.c_str());
            float tabW = ts.x + 24 * sw;

            if (curX + tabW > startX + availW && curX > startX) {
                curX = startX;
                curY += tabH + 4 * sw;
            }

            ImVec2 pos(curX, curY);
            ImVec2 size(tabW, tabH);

            bool active = (m_activeCategory == name);

            ImGui::SetCursorScreenPos(pos);
            ImGui::InvisibleButton(("##tab_" + name).c_str(), size);
            bool clicked = ImGui::IsItemClicked();
            bool hovered = ImGui::IsItemHovered();
            ImGui::Dummy(size);

            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImU32 bg = ImGui::GetColorU32(active ? ImGuiCol_TabActive : ImGuiCol_Button);
            if (hovered && !active)
                bg = ImGui::GetColorU32(ImGuiCol_TabHovered);

            dl->AddRectFilled(pos, ImVec2(pos.x + tabW, pos.y + tabH), bg, 4 * sw);
            if (active)
                dl->AddRectFilled(ImVec2(pos.x + 4 * sw, pos.y + tabH - 2.0f * sw),
                                  ImVec2(pos.x + tabW - 4 * sw, pos.y + tabH),
                                  ImGui::GetColorU32(WikiAccentHi(0.95f)), 1.0f * sw);
            dl->AddText(ImVec2(pos.x + (tabW - ts.x) * 0.5f, pos.y + (tabH - ts.y) * 0.5f),
                ImGui::GetColorU32(ImGuiCol_Text), name.c_str());

            if (clicked) {
                m_activeCategory = name;
                m_activeType.clear();
            }

            curX += tabW + 4 * sw;
        }

        ImGui::SetCursorScreenPos(ImVec2(startX, curY + tabH + 4 * sw));
        ImGui::PopStyleVar();

        auto it = std::find_if(kCategories.begin(), kCategories.end(),
            [](const auto& p) { return p.first == m_activeCategory; });
        if (it == kCategories.end()) return;

        const auto& subcats = it->second;
        if (subcats.empty()) { m_activeType.clear(); return; }

        float subStartX = ImGui::GetCursorScreenPos().x;
        float subCurX = subStartX;
        float subCurY = ImGui::GetCursorScreenPos().y;
        float lastSubH = ImGui::GetFrameHeight();

        for (const auto& sub : subcats) {
            ImVec2 ts = ImGui::CalcTextSize(sub.c_str());
            float subW = ts.x + 20 * sw;
            float subH = ImGui::GetFrameHeight();
            lastSubH = subH;

            if (subCurX + subW > startX + availW && subCurX > subStartX) {
                subCurX = subStartX;
                subCurY += subH + 3 * sw;
            }

            ImVec2 pos(subCurX, subCurY);
            bool active = (m_activeType == sub);

            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec4 bgCol = active ? WikiAccent(0.80f) : ImVec4(0.2f, 0.2f, 0.3f, 0.5f);

            if (!active && ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + subW, pos.y + subH)))
                bgCol = WikiAccentHi(0.70f);

            dl->AddRectFilled(pos, ImVec2(pos.x + subW, pos.y + subH), ImGui::GetColorU32(bgCol), subH * 0.5f);

            ImGui::SetCursorScreenPos(pos);
            ImGui::InvisibleButton(("##sub_" + sub).c_str(), ImVec2(subW, subH));
            bool subClicked = ImGui::IsItemClicked();
            ImGui::Dummy(ImVec2(subW, subH));
            if (subClicked)
                m_activeType = (m_activeType == sub) ? "" : sub;

            dl->AddText(ImVec2(pos.x + (subW - ts.x) * 0.5f, pos.y + (subH - ts.y) * 0.5f),
                ImGui::GetColorU32(ImGuiCol_Text), sub.c_str());

            subCurX += subW + 3 * sw;
        }

        ImGui::SetCursorScreenPos(ImVec2(subStartX, subCurY + lastSubH + 4 * sw));
    }

    void WikiWindow::DrawSearchBar() {
        float sw = WS();
        char buf[256];
        strncpy_s(buf, m_searchQuery.c_str(), sizeof(buf));
        buf[255] = 0;

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30 * sw);
        if (ImGui::InputTextWithHint("##wiki_search", "Search items...", buf, sizeof(buf))) {
            m_searchQuery = buf;
        }

        if (!m_searchQuery.empty()) {
            ImGui::SameLine();
            if (ImGui::Button("X")) m_searchQuery.clear();
        }
    }

    void WikiWindow::DrawItemCard(const ManifestEntry& entry, float cardWidth) {
        float sw = WS();
        ImGui::PushID(entry.path.c_str());

        float iconSize = 48.0f * sw;
        float padding = 8.0f * sw;
        float cardHeight = iconSize + padding * 2;
        float favBtnSize = 32.0f * sw;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 cardEnd(pos.x + cardWidth, pos.y + cardHeight);

        auto iconIt = m_iconCache.find(entry.path);
        bool hasIcon = (iconIt != m_iconCache.end() && iconIt->second.IsValid());
        bool isFav = m_favorites.find(entry.path) != m_favorites.end();

        bool hovered = ImGui::IsMouseHoveringRect(pos, cardEnd);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        const bool polished = LavenderHook::Globals::use_polished_overlay;
        if (polished) {
            // Frosted panel + sheen + accent border, brighter accent on hover
            LavenderHook::UI::Lavender::PolishedPanel(
                dl, pos, cardEnd, 6.0f, ImGui::GetStyle().Alpha, false, cardHeight * 0.35f);
            if (hovered)
                dl->AddRectFilled(pos, cardEnd, ImGui::GetColorU32(WikiAccent(0.14f)), 6);
            ImU32 bc = isFav
                ? ImGui::GetColorU32(kFavoriteColor)
                : ImGui::GetColorU32(WikiAccent(hovered ? 0.55f : 0.30f));
            dl->AddRect(pos, cardEnd, bc, 6, 0, 1.2f);
        }
        else {
            ImU32 cardBg = ImGui::GetColorU32(hovered ? ImVec4(0.15f, 0.15f, 0.2f, 0.8f) : ImVec4(0.1f, 0.1f, 0.12f, 0.6f));
            ImU32 borderCol = ImGui::GetColorU32(isFav ? ImVec4(1.0f, 0.8f, 0.0f, 0.6f) : ImVec4(0.3f, 0.3f, 0.4f, 0.3f));
            dl->AddRectFilled(pos, cardEnd, cardBg, 6);
            dl->AddRect(pos, cardEnd, borderCol, 6);
        }

        // Icon area
        ImVec2 iconPos(pos.x + padding, pos.y + padding);
        ImVec2 iconEnd(iconPos.x + iconSize, iconPos.y + iconSize);

        float iconAlpha = ImGui::GetStyle().Alpha;
        if (hasIcon) {
            dl->AddImage(iconIt->second.id, iconPos, iconEnd, ImVec2(0,0), ImVec2(1,1),
                IM_COL32(255, 255, 255, (int)(255 * iconAlpha)));
        }
        else {
            ImU32 fallbackBg = IM_COL32(51, 51, 77, (int)(128 * iconAlpha));
            dl->AddRectFilled(iconPos, iconEnd, fallbackBg, 4);
        }

        // Name (after icon + fav button)
        float textX = iconEnd.x + padding + favBtnSize + 8 * sw;
        float nameWidth = cardEnd.x - textX - 8 * sw;
        std::string displayName = GetDisplayName(entry);
        if (ImGui::CalcTextSize(displayName.c_str()).x > nameWidth) {
            while (!displayName.empty() && ImGui::CalcTextSize((displayName + "...").c_str()).x > nameWidth)
                displayName.pop_back();
            if (!displayName.empty()) displayName += "...";
        }
        dl->AddText(ImVec2(textX, pos.y + padding), ImGui::GetColorU32(ImGuiCol_Text), displayName.c_str());

        ImGui::Dummy(ImVec2(0, 0));

        // Subtype pills
        float pillY = pos.y + padding + ImGui::GetFontSize() + 4 * sw;
        float pillX = textX;
        for (const auto& st : entry.subtype) {
            ImVec2 sts = ImGui::CalcTextSize(st.c_str());
            float pw = sts.x + 12 * sw, ph = sts.y + 2 * sw;
            dl->AddRectFilled(ImVec2(pillX, pillY), ImVec2(pillX + pw, pillY + ph),
                ImGui::GetColorU32(WikiAccent(0.80f)), ph * 0.5f);
            dl->AddText(ImVec2(pillX + 6 * sw, pillY + 1 * sw), ImGui::GetColorU32(kPillTextColor), st.c_str());
            pillX += pw + 4 * sw;
        }

        // Favorite button - on the left side, centered vertically
        float favX = pos.x + iconSize + padding + 4 * sw;
        float favY = pos.y + (cardHeight - favBtnSize) * 0.5f;

        ImGui::SetCursorScreenPos(ImVec2(favX, favY));
        ImGui::InvisibleButton("##fav", ImVec2(favBtnSize, favBtnSize));
        bool favClicked = ImGui::IsItemClicked();
        ImGui::Dummy(ImVec2(favBtnSize, favBtnSize));
        if (favClicked) {
            if (isFav) m_favorites.erase(entry.path);
            else m_favorites.insert(entry.path);
            SaveFavorites();
        }
        bool favHov = ImGui::IsItemHovered();

		// Star icon drawn as polygon (no circle background)
		{
			ImVec2 sc = ImVec2(favX + favBtnSize * 0.5f, favY + favBtnSize * 0.5f);
			float rOuter = favBtnSize * 0.40f;
			// A 0.40f to 0.45f ratio creates a beautifully proportioned classic star
			float rInner = rOuter * 0.40f; 
			
			ImU32 starCol = ImGui::GetColorU32(
				isFav ? kFavoriteColor
					  : (favHov ? ImVec4(0.9f, 0.8f, 0.3f, 0.9f) : ImVec4(0.6f, 0.6f, 0.6f, 0.6f)));

			dl->PathClear();
			
			// Loop 10 times for the 10 total vertices (5 outer, 5 inner)
			for (int i = 0; i < 10; i++) {
				// Each step is 36 degrees (PI / 5). 
				// Subtracting (PI / 2) points the top tip of the star straight up.
				float angle = i * (3.14159265f / 5.0f) - (3.14159265f / 2.0f);
				float radius = (i % 2 == 0) ? rOuter : rInner;
				
				dl->PathLineTo(ImVec2(sc.x + cosf(angle) * radius, sc.y + sinf(angle) * radius));
			}
			
			// CRITICAL: A star is a concave shape. Use PathFillConcave instead of Convex.
			dl->PathFillConcave(starCol);
		}

        // Click card for detail
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton("##card", ImVec2(cardWidth, cardHeight));
        bool cardClicked = ImGui::IsItemClicked();
        ImGui::Dummy(ImVec2(cardWidth, cardHeight));
        if (cardClicked) {
            m_selectedItemPath = entry.path;
            m_showDetailPopup = false;
        }

        ImGui::SetCursorScreenPos(ImVec2(pos.x, cardEnd.y + 6 * sw));
        ImGui::PopID();
    }

    void WikiWindow::DrawItemDetail() {
        if (m_selectedItemPath.empty()) return;

        auto manifestIt = std::find_if(m_manifest.begin(), m_manifest.end(),
            [](const ManifestEntry& e) { return e.path == m_selectedItemPath; });
        if (manifestIt == m_manifest.end()) { m_selectedItemPath.clear(); return; }

        auto itemIt = m_items.find(m_selectedItemPath);
        bool loaded = (itemIt != m_items.end() && itemIt->second.loaded);

        if (itemIt == m_items.end() || (!itemIt->second.loaded && !itemIt->second.loading)) {
            EnsureItemLoaded(m_selectedItemPath);
        }

        if (!m_showDetailPopup) {
            m_showDetailPopup = true;
            ImGui::OpenPopup("Item Details");
        }
        float sw = WS();
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(450 * sw, 600 * sw), ImGuiCond_Appearing);

        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.65f));
        if (!ImGui::BeginPopupModal("Item Details", &m_showDetailPopup, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::PopStyleColor();
            m_selectedItemPath.clear();
            return;
        }

        if (!loaded) {
            ImGui::Text("Loading item data...");
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                m_selectedItemPath.clear();
                m_showDetailPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            ImGui::PopStyleColor();
            return;
        }

            const auto& item = itemIt->second;
            float iconSize = 64.0f * sw;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetCursorScreenPos();

            auto detIconIt = m_iconCache.find(item.path);
            if (detIconIt != m_iconCache.end() && detIconIt->second.IsValid()) {
                dl->AddImage(detIconIt->second.id, pos, ImVec2(pos.x + iconSize, pos.y + iconSize));
            }
            else {
                // Load icon immediately for detail view
                if (m_loadedIconCount < kMaxLoadedIcons && TextureLoader::IsInitialized()) {
                    auto detManIt = std::find_if(m_manifest.begin(), m_manifest.end(),
                        [&](const ManifestEntry& e) { return e.path == item.path; });
                    if (detManIt != m_manifest.end() && !detManIt->icon.empty()) {
                        std::string iconPath = GetWikiCacheDir() + "\\" + item.path + "\\" + detManIt->icon;
                        Texture tex = TextureLoader::LoadFromFile(iconPath.c_str());
                        if (tex.IsValid()) {
                            m_iconCache[item.path] = tex;
                            m_loadedIconCount++;
                            dl->AddImage(tex.id, pos, ImVec2(pos.x + iconSize, pos.y + iconSize));
                        }
                    }
                }
            }

            ManifestEntry detailEntry = *manifestIt;
            std::string detailDisplayName = GetDisplayName(detailEntry);
            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + iconSize + 12 * sw, ImGui::GetCursorPosY()));
            ImGui::Text("%s", detailDisplayName.c_str());

            // Big star button next to name
            bool isFav = m_favorites.find(item.path) != m_favorites.end();
            ImGui::SameLine();
            float detFavSize = 28.0f * sw;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8 * sw);
            ImGui::InvisibleButton("##det_fav", ImVec2(detFavSize, detFavSize));
            ImVec2 detFavPos = ImGui::GetItemRectMin();
            bool detFavClicked = ImGui::IsItemClicked();
            bool detFavHov = ImGui::IsItemHovered();
            ImDrawList* detDl = ImGui::GetWindowDrawList();
            {
                ImVec2 detSc = ImVec2(detFavPos.x + detFavSize * 0.5f, detFavPos.y + detFavSize * 0.5f);
                float rOuter = detFavSize * 0.40f;
                float rInner = rOuter * 0.40f;
                ImU32 detStarCol = ImGui::GetColorU32(
                    isFav ? kFavoriteColor
                          : (detFavHov ? ImVec4(0.9f, 0.8f, 0.3f, 0.9f) : ImVec4(0.6f, 0.6f, 0.6f, 0.6f)));
                detDl->PathClear();
                for (int i = 0; i < 10; i++) {
                    float angle = i * (3.14159265f / 5.0f) - (3.14159265f / 2.0f);
                    float radius = (i % 2 == 0) ? rOuter : rInner;
                    detDl->PathLineTo(ImVec2(detSc.x + cosf(angle) * radius, detSc.y + sinf(angle) * radius));
                }
                detDl->PathFillConcave(detStarCol);
            }
            if (detFavClicked) {
                if (isFav) m_favorites.erase(item.path);
                else m_favorites.insert(item.path);
                SaveFavorites();
            }

            // Push cursor below the icon to prevent overlap
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + iconSize);

            if (!item.description.empty()) {
                ImGui::TextWrapped("%s", item.description.c_str());
            }

            ImGui::Dummy(ImVec2(0, 8 * sw));

            if (!item.subtype.empty()) {
                ImGui::TextDisabled("Type: ");
                ImGui::SameLine();
                for (const auto& st : item.subtype) {
                    ImVec2 sts = ImGui::CalcTextSize(st.c_str());
                    float pw = sts.x + 12 * sw, ph = sts.y + 2 * sw;
                    ImVec2 ppos = ImGui::GetCursorScreenPos();
                    dl->AddRectFilled(ppos, ImVec2(ppos.x + pw, ppos.y + ph),
                        ImGui::GetColorU32(WikiAccent(0.80f)), ph * 0.5f);
                    dl->AddText(ImVec2(ppos.x + 6 * sw, ppos.y + 1 * sw), ImGui::GetColorU32(kPillTextColor), st.c_str());
                    ImGui::Dummy(ImVec2(pw, ph));
                    ImGui::SameLine();
                }
                ImGui::Dummy(ImVec2(0, 4 * sw));
            }

            if (!item.stats.empty()) {
                ImGui::Separator();
                ImGui::TextDisabled("Stats:");
                for (const auto& stat : item.stats)
                    ImGui::BulletText("%s", stat.c_str());
            }

            if (!item.elements.empty()) {
                ImGui::Separator();
                ImGui::TextDisabled("Elements:");
                for (const auto& elem : item.elements)
                    ImGui::BulletText("%s", elem.c_str());
            }

            if (!item.dropLocations.empty()) {
                ImGui::Separator();
                ImGui::TextDisabled("Drop Locations:");
                ImGui::PushTextWrapPos(0.0f);
                for (const auto& loc : item.dropLocations)
                    ImGui::BulletText("%s", loc.c_str());
                ImGui::PopTextWrapPos();
            }

            ImGui::EndPopup();
            ImGui::PopStyleColor();
        }


    void WikiWindow::FreeIcon(const std::string& folderPath) {
        auto it = m_iconCache.find(folderPath);
        if (it != m_iconCache.end() && it->second.IsValid()) {
            TextureLoader::Free(it->second);
            m_iconCache.erase(it);
            m_loadedIconCount--;
        }
    }

    void WikiWindow::FreeAllIcons() {
        for (auto& [path, tex] : m_iconCache) {
            if (tex.IsValid()) TextureLoader::Free(tex);
        }
        m_iconCache.clear();
        m_loadedIconCount = 0;
    }

    // Render
    void WikiWindow::Render(bool wantVisible) {
        s_wikiFade.Tick(wantVisible);
        if (!s_wikiFade.ShouldRender()) return;

        float alpha = s_wikiFade.Alpha();

        if (!m_initialized) {
            m_initialized = true;
            GetWikiCacheDir();
            LoadFavorites();
        }

        m_visibleItems.clear();

        // Reset per-frame icon load counter
        static int s_iconsThisFrame = 0;
        s_iconsThisFrame = 0;

        if (!m_manifestLoaded && !m_manifestLoading && m_wikiDownloaded) {
            ParseManifest();
        }

        // Detect filter changes to reset page and free icons
        static std::string s_prevCategory;
        static std::string s_prevType;
        static std::string s_prevSearch;
        if (s_prevCategory != m_activeCategory || s_prevType != m_activeType || s_prevSearch != m_searchQuery) {
            m_currentPage = 0;
            s_prevCategory = m_activeCategory;
            s_prevType = m_activeType;
            s_prevSearch = m_searchQuery;
            FreeAllIcons();
        }

        float s = LavenderHook::Globals::menu_scale;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

        // Compute animated window height from header state (no GetWindowHeight readback)
        float headerH = 36 * s;
        float fullH = 605 * s;
        float targetH = headerH + (fullH - headerH) * s_headerAnim;
        static float s_animH = fullH;
        s_animH += (targetH - s_animH) * ImGui::GetIO().DeltaTime * 20.0f;
        ImGui::SetNextWindowSize(ImVec2(605 * s, s_animH), ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize;

        if (ImGui::Begin("##Wiki", nullptr, flags)) {
            // Animate header
            float headerTarget = s_headerOpen ? 1.0f : 0.0f;
            s_headerAnim += (headerTarget - s_headerAnim) * ImGui::GetIO().DeltaTime * 8.0f;
            s_headerAnim = ImClamp(s_headerAnim, 0.0f, 1.0f);

            LavenderHook::UI::Lavender::RenderWindowHeader(
                "Wiki",
                g_infoIcoTex,
                g_dropLeftTex,
                ImGui::GetWindowWidth(),
                alpha,
                s_headerOpen,
                s_headerAnim,
                s_arrowAnim
            );

            if (s_headerAnim > 0.001f) {
                float ha = alpha * s_headerAnim;
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ha);

                if (m_wikiDownloading) {
                    ImGui::TextDisabled("Downloading wiki data...");
                }
                else if (m_manifestLoading && !m_manifestLoaded) {
                    ImGui::TextDisabled("Loading item database...");
                }
                else if (!m_manifestLoaded && !m_wikiDownloaded) {
                    ImGui::TextDisabled("Failed to download wiki.");
                    if (ImGui::Button("Retry")) {
                        m_wikiDownloading = true;
                        std::thread([] {
                            WikiWindow::m_wikiDownloaded = DownloadAndExtractWiki();
                            WikiWindow::m_wikiDownloading = false;
                        }).detach();
                    }
                }
                else {
                    DrawSearchBar();
                    ImGui::Separator();
                    DrawCategoryTabs();
                    ImGui::Separator();

                    bool searching = !m_searchQuery.empty();

                    // First pass: count all matching items
                    std::vector<const ManifestEntry*> filtered;
                    for (const auto& entry : m_manifest) {
                        if (searching) {
                            std::string lowerQ = m_searchQuery;
                            std::transform(lowerQ.begin(), lowerQ.end(), lowerQ.begin(), ::tolower);
                            std::string lowerName = entry.name;
                            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                            std::string lowerDisplay = GetDisplayName(entry);
                            std::transform(lowerDisplay.begin(), lowerDisplay.end(), lowerDisplay.begin(), ::tolower);
                            if (lowerName.find(lowerQ) == std::string::npos &&
                                lowerDisplay.find(lowerQ) == std::string::npos) continue;
                        }
                        else {
                            if (entry.category != m_activeCategory) continue;
                            if (!m_activeType.empty() && entry.type != m_activeType) continue;
                        }
                        filtered.push_back(&entry);
                    }

                    int totalItems = (int)filtered.size();
                    int totalPages = std::max(1, (totalItems + m_itemsPerPage - 1) / m_itemsPerPage);
                    bool hasPages = totalPages > 1;

                    // Reset page if out of bounds
                    if (m_currentPage >= totalPages) m_currentPage = totalPages - 1;
                    if (m_currentPage < 0) m_currentPage = 0;

                    int startIdx = m_currentPage * m_itemsPerPage;
                    int endIdx = std::min(startIdx + m_itemsPerPage, totalItems);

                    // Reserve space for page nav only if needed
                    float navH = hasPages ? ImGui::GetFrameHeightWithSpacing() * 2 : 0.0f;
                    float cardWidth = ImGui::GetContentRegionAvail().x;
                    float childH = ImGui::GetContentRegionAvail().y - navH;
                    if (childH < 100) childH = 100;
                    ImGui::BeginChild("##item_list", ImVec2(cardWidth, childH), false);

                    // Render only current page items
                    for (int i = startIdx; i < endIdx; i++) {
                        const auto& entry = *filtered[i];
                        m_visibleItems.insert(entry.path);

                        if (!entry.icon.empty() && m_iconCache.find(entry.path) == m_iconCache.end() && m_loadedIconCount < kMaxLoadedIcons && TextureLoader::IsInitialized() && s_iconsThisFrame < 15) {
                            std::string iconPath = GetWikiCacheDir() + "\\" + entry.path + "\\" + entry.icon;
                            Texture tex = TextureLoader::LoadFromFile(iconPath.c_str());
                            if (tex.IsValid()) {
                                m_iconCache[entry.path] = tex;
                                m_loadedIconCount++;
                            }
                            s_iconsThisFrame++;
                        }

                        DrawItemCard(*filtered[i], cardWidth);
                    }

                    if (totalItems == 0) {
                        ImGui::TextDisabled("No items found.");
                    }
                    else if (searching) {
                        ImGui::TextDisabled("%d items match your search.", totalItems);
                    }

                    // Keep favorite + selected items always visible (icons persist)
                    for (const auto& favPath : m_favorites) {
                        m_visibleItems.insert(favPath);
                    }
                    if (!m_selectedItemPath.empty())
                        m_visibleItems.insert(m_selectedItemPath);

                    // Free icons for items no longer visible
                    for (auto it = m_iconCache.begin(); it != m_iconCache.end(); ) {
                        if (m_visibleItems.find(it->first) == m_visibleItems.end()) {
                            TextureLoader::Free(it->second);
                            m_loadedIconCount--;
                            it = m_iconCache.erase(it);
                        }
                        else {
                            ++it;
                        }
                    }

                    ImGui::EndChild();

                    // Page navigation
                    if (hasPages) {
                        ImGui::Separator();
                        ImGui::TextDisabled("Page %d/%d (%d items)", m_currentPage + 1, totalPages, totalItems);
                        if (m_currentPage > 0) {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("< Prev")) { m_currentPage--; }
                        }
                        if (m_currentPage < totalPages - 1) {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Next >")) { m_currentPage++; }
                        }
                    }

                    } // end manifest-loaded else

                ImGui::PopStyleVar();
            }

            DrawItemDetail();
        }
        ImVec2 sbPos = ImGui::GetWindowPos();
        ImVec2 sbSize = ImGui::GetWindowSize();
        ImGui::End();
        if (LavenderHook::Globals::use_polished_overlay)
            LavenderHook::UI::Lavender::DrawWindowShadow(sbPos, sbSize, alpha);
        ImGui::PopStyleVar();
    }

    void WikiWindow::Update() {
        if (!m_initialized) {
            m_initialized = true;
            GetWikiCacheDir();
            LoadFavorites();
            // Check remote manifest timestamp to decide if ZIP needs redownload
            m_wikiDownloading = true;
            std::thread([] {
                std::string remoteManifest = FetchUrlSync(
                    "https://raw.githubusercontent.com/KaMuZunai/LavenderHookWiki/refs/heads/main/manifest.json");
                bool needsUpdate = true;
                if (!remoteManifest.empty()) {
                    std::string remoteTime = FindStringValue(remoteManifest, "generatedAt");
                    std::string cachedManifest = ReadFileFromCache("manifest.json");
                    if (!cachedManifest.empty()) {
                        std::string cachedTime = FindStringValue(cachedManifest, "generatedAt");
                        if (!remoteTime.empty() && remoteTime == cachedTime)
                            needsUpdate = false;
                    }
                }
                if (needsUpdate)
                    WikiWindow::m_wikiDownloaded = DownloadAndExtractWiki();
                else
                    WikiWindow::m_wikiDownloaded = true;
                WikiWindow::m_wikiDownloading = false;
            }).detach();
        }

        if (!m_manifestLoaded && !m_manifestLoading && m_wikiDownloaded) {
            ParseManifest();
        }

        // Load icons for favorites from extracted files
        for (const auto& favPath : m_favorites) {
            if (m_iconCache.find(favPath) != m_iconCache.end() || m_loadedIconCount >= kMaxLoadedIcons)
                continue;
            auto favIt = std::find_if(m_manifest.begin(), m_manifest.end(),
                [&favPath](const ManifestEntry& e) { return e.path == favPath; });
            if (favIt == m_manifest.end() || favIt->icon.empty() || !TextureLoader::IsInitialized())
                continue;
            std::string iconPath = GetWikiCacheDir() + "\\" + favPath + "\\" + favIt->icon;
            Texture tex = TextureLoader::LoadFromFile(iconPath.c_str());
            if (tex.IsValid()) { m_iconCache[favPath] = tex; m_loadedIconCount++; }
        }

        // Load icon for selected/detail item from extracted files
        if (!m_selectedItemPath.empty() && m_iconCache.find(m_selectedItemPath) == m_iconCache.end() && m_loadedIconCount < kMaxLoadedIcons) {
            auto selIt = std::find_if(m_manifest.begin(), m_manifest.end(),
                [selPath = m_selectedItemPath](const ManifestEntry& e) { return e.path == selPath; });
            if (selIt != m_manifest.end() && !selIt->icon.empty() && TextureLoader::IsInitialized()) {
                std::string iconPath = GetWikiCacheDir() + "\\" + m_selectedItemPath + "\\" + selIt->icon;
                Texture tex = TextureLoader::LoadFromFile(iconPath.c_str());
                if (tex.IsValid()) { m_iconCache[m_selectedItemPath] = tex; m_loadedIconCount++; }
            }
        }
    }

    void WikiWindow::RenderFavoriteOverlay() {
        if (m_favorites.empty()) return;

        bool wantVisible = !m_favorites.empty();
        s_favOverlayFade.Tick(wantVisible);
        if (!s_favOverlayFade.ShouldRender()) return;

        float alpha = s_favOverlayFade.Alpha();

        float sw = WS();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

        float maxFavTextW = 0.0f;
        for (const auto& favPath : m_favorites) {
            std::string favName;
            for (const auto& me : m_manifest) {
                if (me.path == favPath) { favName = me.name; break; }
            }
            if (!favName.empty()) {
                float tw = ImGui::CalcTextSize(favName.c_str()).x;
                if (tw > maxFavTextW) maxFavTextW = tw;
            }
        }
        float iconS_calc = 24.0f * sw;
        float desiredFavW = iconS_calc + 6 * sw + maxFavTextW + 14 * sw;
        ImGui::SetNextWindowBgAlpha(0.3f);
        ImGui::SetNextWindowSizeConstraints(ImVec2(desiredFavW, 0), ImVec2(desiredFavW, FLT_MAX));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_AlwaysAutoResize;

        if (ImGui::Begin("##fav_overlay", nullptr, flags)) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 winPos = ImGui::GetWindowPos();
            ImVec2 winSize = ImGui::GetWindowSize();

            dl->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
                ImGui::GetColorU32(ImVec4(0, 0, 0, 0.3f * alpha)), 6 * sw);

            for (const auto& favPath : m_favorites) {
                // Look up name from manifest (we don't cache WikiItem entries for favorites)
                std::string favName;
                for (const auto& me : m_manifest) {
                    if (me.path == favPath) { favName = me.name; break; }
                }
                if (favName.empty()) continue;

                float itemH = 28.0f * sw;
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImVec2 size(ImGui::GetContentRegionAvail().x, itemH);

                bool hovered = ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + size.x, pos.y + size.y));
                if (hovered) {
                    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                        ImGui::GetColorU32(WikiAccent(0.30f)), 3 * sw);
                }

                float iconS = 24.0f * sw;
                auto favIconIt = m_iconCache.find(favPath);
                if (favIconIt != m_iconCache.end() && favIconIt->second.IsValid()) {
                    dl->AddImage(favIconIt->second.id, ImVec2(pos.x + 2 * sw, pos.y + 2 * sw),
                        ImVec2(pos.x + 2 * sw + iconS, pos.y + 2 * sw + iconS));
                }

                ImGui::InvisibleButton(("##fav_" + favPath).c_str(), size);
                bool favClicked = ImGui::IsItemClicked();
                ImGui::Dummy(ImVec2(0, 0));
                if (favClicked) {
                    m_selectedItemPath = favPath;
                    m_showDetailPopup = false;
                }

                float textX = pos.x + iconS + 6 * sw;
                float textW = size.x - iconS - 6 * sw;
                std::string name = favName;
                if (ImGui::CalcTextSize(name.c_str()).x > textW) {
                    while (!name.empty() && ImGui::CalcTextSize((name + "...").c_str()).x > textW)
                        name.pop_back();
                    if (!name.empty()) name += "...";
                }
                dl->AddText(ImVec2(textX, pos.y + (itemH - ImGui::GetFontSize()) * 0.5f),
                    ImGui::GetColorU32(ImGuiCol_Text), name.c_str());

                ImGui::Dummy(ImVec2(0, 1 * sw));
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

} // namespace LavenderHook::UI::Windows

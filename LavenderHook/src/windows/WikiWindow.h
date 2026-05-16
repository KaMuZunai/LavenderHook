#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include "../assets/Texture.h"

namespace LavenderHook {
    namespace UI {
        namespace Windows {

            struct WikiItem {
                std::string name;
                std::string description;
                std::string icon;
                std::string category;
                std::string type;
                std::vector<std::string> subtype;
                std::vector<std::string> stats;
                std::vector<std::string> elements;
                std::vector<std::string> dropLocations;
                std::string path;
                bool enabled = true;
                bool loaded = false;
                bool loading = false;
                Texture iconTex;
                std::string iconCachePath;
            };

            struct ManifestEntry {
                std::string name;
                std::string icon;
                std::string category;
                std::string type;
                std::vector<std::string> subtype;
                std::string path; // folder path like Items\Weapons\Crossbows\Foo
                bool enabled = true;
            };

            struct FetchJob {
                int id;
                std::string url;
                std::string cacheKey;
                std::string result;
                bool done = false;
                bool success = false;
            };

            class WikiWindow {
            public:
                static void Render(bool wantVisible);
                static void Update();
                static void RenderFavoriteOverlay();

            private:
                static void StartFetchThread();
                static void FetchThreadProc();
                static int EnqueueFetch(const std::string& url, const std::string& cacheKey);
                static void ProcessFetches();
                static void SaveToCache(const std::string& cacheKey, const std::string& data);
                static std::string LoadFromCache(const std::string& cacheKey);
                static bool CacheExists(const std::string& cacheKey);

                static void ParseManifest();
                static void ParseManifestJson(const std::string& json);
                static bool DownloadAndExtractWiki();
                static std::string ReadFileFromCache(const std::string& relativePath);
                static std::string FetchUrlSync(const std::string& url);

                static std::string GetWikiCacheDir();
                static std::string GetCacheFilePath(const std::string& cacheKey);
                static std::string GetItemJsonUrl(const std::string& folderPath);
                static std::string GetIconUrl(const std::string& folderPath, const std::string& iconName);
                static std::string NormalizePath(const std::string& path);

                static void LoadItemsForCategory(const std::string& category, const std::string& type);
                static void EnsureItemLoaded(const std::string& folderPath);

                static void DrawCategoryTabs();
                static void DrawSearchBar();
                static void DrawItemCard(const ManifestEntry& entry, float cardWidth);
                static void DrawItemDetail();

                static std::string m_cacheDir;
                static std::string m_manifestUrl;

                static std::vector<ManifestEntry> m_manifest;
                static std::unordered_map<std::string, WikiItem> m_items; // keyed by folder path
                static bool m_manifestLoaded;
                static bool m_manifestLoading;
                static int m_manifestFetchId;

                static std::string m_activeCategory;
                static std::string m_activeType;
                static std::string m_searchQuery;
        static int m_currentPage;
        static int m_itemsPerPage;

        static std::string m_selectedItemPath;
        static bool m_showDetailPopup;

    public:
        static std::unordered_set<std::string> m_favorites;
    private:

                static std::vector<std::thread> m_fetchThreads;
                static std::mutex m_fetchMutex;
                static std::queue<FetchJob*> m_fetchQueue;
                static std::vector<FetchJob*> m_fetchResults;
                static std::atomic<bool> m_fetchThreadRunning;
                static int m_nextFetchId;
                static int kFetchThreadCount;

                static bool m_initialized;
                static std::atomic<bool> m_wikiDownloading;
                static std::atomic<bool> m_wikiDownloaded;

                static const std::vector<std::pair<std::string, std::vector<std::string>>> kCategories;
        static void FreeIcon(const std::string& folderPath);
        static void FreeAllIcons();
        static std::unordered_set<std::string> m_visibleItems;
        static int m_loadedIconCount;
        static constexpr int kMaxLoadedIcons = 100;
        static std::unordered_map<std::string, Texture> m_iconCache;
            };

        }
    }
}

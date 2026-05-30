#include "UIDispatch.h"
#include <mutex>
#include <queue>
#include <atomic>

namespace LavenderHook::UI
{
    static std::mutex                        g_mtx;
    static std::queue<std::function<void()>> g_q;
    static std::atomic<int>                  g_count{ 0 };

    void Enqueue(std::function<void()> fn)
    {
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            g_q.push(std::move(fn));
            ++g_count;
        }
    }

    int PlayAll()
    {
        std::queue<std::function<void()>> local;
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            std::swap(local, g_q);
        }
        int n = 0;
        while (!local.empty())
        {
            auto& fn = local.front();
            if (fn) fn();
            local.pop();
            ++n;
        }
        g_count -= n;
        return n;
    }

    int PendingApprox()
    {
        return g_count.load();
    }
}

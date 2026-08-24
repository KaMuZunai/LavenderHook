#pragma once
#include <string>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <cstdint>

struct sockaddr;

namespace LavenderHook::Net
{
    using socket_handle_t = uintptr_t;

    class NetworkMonitor
    {
    public:
        static NetworkMonitor& Instance();

        bool InitHooks();
        void Shutdown();

        std::string GetTopIp() const {
            std::lock_guard<std::mutex> lk(mtx_);
            return top_ip_;
        }
        int GetLastPingMs() const { return last_ping_ms_.load(); }

        void RecordConnect(socket_handle_t s, const sockaddr* addr, int addrlen);
        void RecordSendTo(socket_handle_t s, const sockaddr* addr, int addrlen);
        void OnCloseSocket(socket_handle_t s);

    private:
        NetworkMonitor() = default;
        ~NetworkMonitor() = default;
        NetworkMonitor(const NetworkMonitor&) = delete;
        NetworkMonitor& operator=(const NetworkMonitor&) = delete;

        static std::string SockAddrToIp(const sockaddr* addr, int addrlen);

        void StartWorker();
        void StopWorker();
        void WorkerLoop();

        mutable std::mutex mtx_;
        std::unordered_map<socket_handle_t, std::string> socket_to_ip_;
        std::unordered_map<std::string, uint64_t> ip_count_;
        std::string top_ip_;
        std::atomic<int> last_ping_ms_{ -1 };

        std::thread worker_;
        std::atomic<bool> worker_stop_{ false };
    };
}

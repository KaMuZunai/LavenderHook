#include "NetworkMonitor.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <iphlpapi.h>
#include <ipexport.h>
#include <icmpapi.h>

#include "../minhook/MinHook.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace LavenderHook::Net
{
    using connect_t = int (WSAAPI*)(SOCKET, const sockaddr*, int);
    using WSAConnect_t = int (WSAAPI*)(SOCKET, const sockaddr*, int, LPWSABUF, LPWSABUF, LPQOS, LPQOS);
    using sendto_t = int (WSAAPI*)(SOCKET, const char*, int, int, const sockaddr*, int);
    using closesocket_t = int (WSAAPI*)(SOCKET);

    static connect_t     g_orig_connect = nullptr;
    static WSAConnect_t  g_orig_WSAConnect = nullptr;
    static sendto_t      g_orig_sendto = nullptr;
    static closesocket_t g_orig_closesocket = nullptr;

    static LPVOID g_t_connect = nullptr;
    static LPVOID g_t_WSAConnect = nullptr;
    static LPVOID g_t_sendto = nullptr;
    static LPVOID g_t_closesocket = nullptr;

    static int  WSAAPI Hooked_connect(SOCKET, const sockaddr*, int);
    static int  WSAAPI Hooked_WSAConnect(SOCKET, const sockaddr*, int, LPWSABUF, LPWSABUF, LPQOS, LPQOS);
    static int  WSAAPI Hooked_sendto(SOCKET, const char*, int, int, const sockaddr*, int);
    static int  WSAAPI Hooked_closesocket(SOCKET);

    NetworkMonitor& NetworkMonitor::Instance()
    {
        static NetworkMonitor inst;
        return inst;
    }

    std::string NetworkMonitor::SockAddrToIp(const sockaddr* addr, int addrlen)
    {
        if (!addr) return {};
        char buf[64] = {};
        if (addr->sa_family == AF_INET && addrlen >= (int)sizeof(sockaddr_in)) {
            auto* v4 = reinterpret_cast<const sockaddr_in*>(addr);
            inet_ntop(AF_INET, &v4->sin_addr, buf, sizeof(buf));
        }
        else if (addr->sa_family == AF_INET6 && addrlen >= (int)sizeof(sockaddr_in6)) {
            auto* v6 = reinterpret_cast<const sockaddr_in6*>(addr);
            inet_ntop(AF_INET6, &v6->sin6_addr, buf, sizeof(buf));
        }
        return buf;
    }

    void NetworkMonitor::RecordConnect(socket_handle_t s, const sockaddr* addr, int addrlen)
    {
        const std::string ip = SockAddrToIp(addr, addrlen);
        if (ip.empty()) return;

        std::lock_guard<std::mutex> lk(mtx_);
        socket_to_ip_[static_cast<SOCKET>(s)] = ip;
        ip_count_[ip]++;
    }

    void NetworkMonitor::RecordSendTo(socket_handle_t s, const sockaddr* addr, int addrlen)
    {
        const std::string ip = SockAddrToIp(addr, addrlen);
        if (ip.empty()) return;

        std::lock_guard<std::mutex> lk(mtx_);
        socket_to_ip_[static_cast<SOCKET>(s)] = ip;
        ip_count_[ip]++;
    }

    void NetworkMonitor::OnCloseSocket(socket_handle_t s)
    {
        std::lock_guard<std::mutex> lk(mtx_);
        socket_to_ip_.erase(static_cast<SOCKET>(s));
    }

    bool NetworkMonitor::InitHooks()
    {
        const MH_STATUS init = MH_Initialize();
        if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED)
            return false;

        HMODULE ws2 = GetModuleHandleA("ws2_32.dll");
        if (!ws2) ws2 = LoadLibraryA("ws2_32.dll");
        if (!ws2) return false;

        g_t_connect = GetProcAddress(ws2, "connect");
        g_t_WSAConnect = GetProcAddress(ws2, "WSAConnect");
        g_t_sendto = GetProcAddress(ws2, "sendto");
        g_t_closesocket = GetProcAddress(ws2, "closesocket");

        if (!g_t_connect || !g_t_WSAConnect || !g_t_sendto || !g_t_closesocket)
            return false;

        auto HookOne = [](LPVOID target, LPVOID detour, LPVOID* original) -> bool
            {
                if (MH_CreateHook(target, detour, original) != MH_OK) return false;
                if (MH_EnableHook(target) != MH_OK) return false;
                return true;
            };

        if (!HookOne(g_t_connect, (LPVOID)Hooked_connect, (LPVOID*)&g_orig_connect))     return false;
        if (!HookOne(g_t_WSAConnect, (LPVOID)Hooked_WSAConnect, (LPVOID*)&g_orig_WSAConnect))  return false;
        if (!HookOne(g_t_sendto, (LPVOID)Hooked_sendto, (LPVOID*)&g_orig_sendto))      return false;
        if (!HookOne(g_t_closesocket, (LPVOID)Hooked_closesocket, (LPVOID*)&g_orig_closesocket)) return false;

        StartWorker();
        return true;
    }

    void NetworkMonitor::Shutdown()
    {
        StopWorker();

        if (g_t_connect) { MH_DisableHook(g_t_connect);     MH_RemoveHook(g_t_connect); }
        if (g_t_WSAConnect) { MH_DisableHook(g_t_WSAConnect);  MH_RemoveHook(g_t_WSAConnect); }
        if (g_t_sendto) { MH_DisableHook(g_t_sendto);      MH_RemoveHook(g_t_sendto); }
        if (g_t_closesocket) { MH_DisableHook(g_t_closesocket); MH_RemoveHook(g_t_closesocket); }

        g_t_connect = g_t_WSAConnect = g_t_sendto = g_t_closesocket = nullptr;
        g_orig_connect = nullptr; g_orig_WSAConnect = nullptr;
        g_orig_sendto = nullptr;  g_orig_closesocket = nullptr;
    }

    void NetworkMonitor::StartWorker()
    {
        worker_stop_ = false;
        worker_ = std::thread([this] { WorkerLoop(); });
    }

    void NetworkMonitor::StopWorker()
    {
        worker_stop_ = true;
        if (worker_.joinable()) worker_.join();
    }

    void NetworkMonitor::WorkerLoop()
    {
        HANDLE icmp = IcmpCreateFile();
        if (icmp == INVALID_HANDLE_VALUE)
            last_ping_ms_.store(-1);

        std::vector<char> reply(sizeof(ICMP_ECHO_REPLY) + 64);
        const DWORD timeout_ms = 1000;

        while (!worker_stop_)
        {
            std::string best_ip;
            uint64_t best_count = 0;
            {
                std::lock_guard<std::mutex> lk(mtx_);
                for (const auto& kv : ip_count_)
                {
                    if (kv.second > best_count)
                    {
                        best_count = kv.second;
                        best_ip = kv.first;
                    }
                }
                top_ip_ = best_ip;
            }

            int ping_ms = -1;
            if (!best_ip.empty() && icmp != INVALID_HANDLE_VALUE)
            {
                in_addr addr{};
                if (InetPtonA(AF_INET, best_ip.c_str(), &addr) == 1)
                {
                    DWORD rv = IcmpSendEcho(
                        icmp,
                        addr.S_un.S_addr,
                        (void*)"ah", 2,
                        nullptr,
                        reply.data(),
                        static_cast<DWORD>(reply.size()),
                        timeout_ms
                    );
                    if (rv >= 1)
                    {
                        auto* echo = reinterpret_cast<ICMP_ECHO_REPLY*>(reply.data());
                        ping_ms = static_cast<int>(echo->RoundTripTime);
                    }
                }
            }
            last_ping_ms_.store(ping_ms);

            for (int i = 0; i < 30 && !worker_stop_; ++i)
                Sleep(100);
        }

        if (icmp != INVALID_HANDLE_VALUE) IcmpCloseHandle(icmp);
    }

    static int WSAAPI Hooked_connect(SOCKET s, const sockaddr* name, int namelen)
    {
        NetworkMonitor::Instance().RecordConnect((socket_handle_t)s, name, namelen);
        return g_orig_connect(s, name, namelen);
    }

    static int WSAAPI Hooked_WSAConnect(SOCKET s, const sockaddr* name, int namelen,
        LPWSABUF a, LPWSABUF b, LPQOS c, LPQOS d)
    {
        NetworkMonitor::Instance().RecordConnect((socket_handle_t)s, name, namelen);
        return g_orig_WSAConnect(s, name, namelen, a, b, c, d);
    }

    static int WSAAPI Hooked_sendto(SOCKET s, const char* buf, int len, int flags,
        const sockaddr* to, int tolen)
    {
        if (to && tolen >= (int)sizeof(sockaddr))
            NetworkMonitor::Instance().RecordSendTo((socket_handle_t)s, to, tolen);
        return g_orig_sendto(s, buf, len, flags, to, tolen);
    }

    static int WSAAPI Hooked_closesocket(SOCKET s)
    {
        NetworkMonitor::Instance().OnCloseSocket((socket_handle_t)s);
        return g_orig_closesocket(s);
    }
}

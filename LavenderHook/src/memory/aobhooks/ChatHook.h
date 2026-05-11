#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <array>
#include <mutex>
#include <string>
#include <algorithm>
#include "../aobutils/ScannerUtils.h"
#include "../../misc/Globals.h"
#include "../../ui/components/Console.h"

namespace LavenderHook::Memory::ChatHook {

    // Chat receive function at 0x14e6b30.
    // The only unique bytes are in the NEXT function (+96..+100) which
    // differs at each call site.  We include those to disambiguate.
    //
    // On entry: RCX = this, RDX -> struct { FString playFabId; void* sender; int32 type; void* msg; }
    //
    // Byte layout:
    //    0-4:   48 89 5C 24 08          MOV [RSP+8], RBX
    //    5:     57                      PUSH RDI
    //    6-9:   48 83 EC 50             SUB RSP, 0x50
    //   10-12:  48 8D 15                LEA RDX, [RIP+...]
    //   13-16:  XX XX XX XX             RIP-relative displacement (wildcard)
    //   17-19:  48 8B D9                MOV RBX, RCX
    //   20-24:  E8 XX XX XX XX          CALL rel32 (wildcard)
    //   25-26:  33 FF                   XOR EDI, EDI
    //   27-31:  48 89 7C 24 68          MOV [RSP+0x68], RDI
    //   32-36:  E8 XX XX XX XX          CALL rel32 (wildcard)
    //   37-41:  40 88 7C 24 40          MOV [RSP+0x40], DIL
    //   42-44:  45 33 C9                XOR R9D, R9D
    //   45-49:  48 89 7C 24 38          MOV [RSP+0x38], RDI
    //   50-52:  44 8B C7                MOV R8D, EDI
    //   53-57:  40 88 7C 24 30          MOV [RSP+0x30], DIL
    //   58-60:  48 8B D3                MOV RDX, RBX
    //   61-65:  48 89 7C 24 28          MOV [RSP+0x28], RDI
    //   66-68:  48 8B C8                MOV RCX, RBX
    //   69-72:  89 7C 24 20             MOV [RSP+0x20], EDI
    //   73-77:  E8 XX XX XX XX          CALL rel32 (wildcard)
    //   78-82:  48 8B 5C 24 60          MOV RBX, [RSP+0x60]
    //   83-86:  48 83 C4 50             ADD RSP, 0x50
    //   87:     5F                      POP RDI
    //   88:     C3                      RET
    //   89-95:  CC CC CC CC CC CC CC    INT3 padding
    //   96-100: 48 89 5C 24 10          START OF NEXT FUNCTION ← unique discriminator
    inline constexpr uint8_t k_Pattern[] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,  //  0-4:   MOV [RSP+8], RBX
        0x57,                           //  5:     PUSH RDI
        0x48, 0x83, 0xEC, 0x50,         //  6-9:   SUB RSP, 0x50
        0x48, 0x8D, 0x15,               // 10-12:  LEA RDX, [RIP+...]
        0x00, 0x00, 0x00, 0x00,         // 13-16:  RIP-relative displacement (wildcard)
        0x48, 0x8B, 0xD9,               // 17-19:  MOV RBX, RCX
        0x00, 0x00, 0x00, 0x00, 0x00,   // 20-24:  CALL rel32 (wildcard)
        0x33, 0xFF,                     // 25-26:  XOR EDI, EDI
        0x48, 0x89, 0x7C, 0x24, 0x68,  // 27-31:  MOV [RSP+0x68], RDI
        0x00, 0x00, 0x00, 0x00, 0x00,   // 32-36:  CALL rel32 (wildcard)
        0x40, 0x88, 0x7C, 0x24, 0x40,  // 37-41:  MOV [RSP+0x40], DIL
        0x45, 0x33, 0xC9,               // 42-44:  XOR R9D, R9D
        0x48, 0x89, 0x7C, 0x24, 0x38,  // 45-49:  MOV [RSP+0x38], RDI
        0x44, 0x8B, 0xC7,               // 50-52:  MOV R8D, EDI
        0x40, 0x88, 0x7C, 0x24, 0x30,  // 53-57:  MOV [RSP+0x30], DIL
        0x48, 0x8B, 0xD3,               // 58-60:  MOV RDX, RBX
        0x48, 0x89, 0x7C, 0x24, 0x28,  // 61-65:  MOV [RSP+0x28], RDI
        0x48, 0x8B, 0xC8,               // 66-68:  MOV RCX, RBX
        0x89, 0x7C, 0x24, 0x20,         // 69-72:  MOV [RSP+0x20], EDI
        0x00, 0x00, 0x00, 0x00, 0x00,   // 73-77:  CALL rel32 (wildcard)
        0x48, 0x8B, 0x5C, 0x24, 0x60,  // 78-82:  MOV RBX, [RSP+0x60]
        0x48, 0x83, 0xC4, 0x50,         // 83-86:  ADD RSP, 0x50
        0x5F,                           // 87:     POP RDI
        0xC3,                           // 88:     RET
        0xCC, 0xCC, 0xCC, 0xCC,         // 89-92:  INT3 padding
        0xCC, 0xCC, 0xCC,               // 93-95:  INT3 padding
        0x48, 0x89, 0x5C, 0x24, 0x10,  // 96-100: START OF NEXT FUNCTION ← unique!
    };

    inline constexpr uint8_t k_Mask[] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  //  0-4:
        0xFF,                           //  5:
        0xFF, 0xFF, 0xFF, 0xFF,         //  6-9:
        0xFF, 0xFF, 0xFF,               // 10-12:
        0x00, 0x00, 0x00, 0x00,         // 13-16: wildcard
        0xFF, 0xFF, 0xFF,               // 17-19:
        0x00, 0x00, 0x00, 0x00, 0x00,   // 20-24: wildcard
        0xFF, 0xFF,                     // 25-26:
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 27-31:
        0x00, 0x00, 0x00, 0x00, 0x00,   // 32-36: wildcard
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 37-41:
        0xFF, 0xFF, 0xFF,               // 42-44:
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 45-49:
        0xFF, 0xFF, 0xFF,               // 50-52:
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 53-57:
        0xFF, 0xFF, 0xFF,               // 58-60:
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 61-65:
        0xFF, 0xFF, 0xFF,               // 66-68:
        0xFF, 0xFF, 0xFF, 0xFF,         // 69-72:
        0x00, 0x00, 0x00, 0x00, 0x00,   // 73-77: wildcard
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 78-82:
        0xFF, 0xFF, 0xFF, 0xFF,         // 83-86:
        0xFF,                           // 87:
        0xFF,                           // 88:
        0xFF, 0xFF, 0xFF, 0xFF,         // 89-92:
        0xFF, 0xFF, 0xFF,               // 93-95:
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 96-100: unique discriminator
    };

    struct ChatMessage {
        std::string playFabId;
        std::string sender;
        std::string message;
        int type;
    };

    inline constexpr int kMaxMessages = 200;
    inline std::array<ChatMessage, kMaxMessages> g_messages;
    inline std::atomic<int> g_count{0};
    inline int g_writeIndex = 0;
    inline std::mutex g_mutex;

    inline void*     g_stub   = nullptr;
    inline uintptr_t g_hookAt = 0;

    static std::string ReadFString(uintptr_t addr)
    {
        if (!addr) return {};
        auto* dataPtr = *reinterpret_cast<void**>(addr);
        if (!dataPtr) return {};
        const int len = *reinterpret_cast<int32_t*>(addr + 8);
        if (len <= 0 || len > 2000) return {};

        const int cbNeeded = WideCharToMultiByte(
            CP_UTF8, 0, static_cast<const wchar_t*>(dataPtr),
            len, nullptr, 0, nullptr, nullptr);
        if (cbNeeded <= 0) return {};

        std::string out(static_cast<size_t>(cbNeeded), '\0');
        WideCharToMultiByte(CP_UTF8, 0, static_cast<const wchar_t*>(dataPtr),
            len, &out[0], cbNeeded, nullptr, nullptr);
        return out;
    }

    // Called by the assembly stub immediately at hook entry (while RDX data is valid).
    // Reads the struct fields and pushes into the ring buffer.
    inline __declspec(noinline) void ChatHookCallback(uintptr_t rdx)
    {
        ChatMessage msg;
        msg.playFabId = ReadFString(rdx);
        msg.type = *reinterpret_cast<int32_t*>(rdx + 0x20);

        void* senderPtr = *reinterpret_cast<void**>(rdx + 0x10);
        if (senderPtr)
            msg.sender = ReadFString(reinterpret_cast<uintptr_t>(senderPtr) + 0x28);

        void* msgPtr = *reinterpret_cast<void**>(rdx + 0x28);
        if (msgPtr)
            msg.message = ReadFString(reinterpret_cast<uintptr_t>(msgPtr) + 0x28);

        if (msg.message.empty() && msg.sender.empty())
            return;

        {
            std::string log = "[ChatHook] " + msg.sender + ": " + msg.message;
            LavenderConsole::GetInstance().Log(log);
        }

        std::lock_guard<std::mutex> lock(g_mutex);
        g_messages[g_writeIndex] = std::move(msg);
        g_writeIndex = (g_writeIndex + 1) % kMaxMessages;
        g_count.store(std::min(g_count.load() + 1, kMaxMessages), std::memory_order_relaxed);
    }

    inline bool InstallHook(uint8_t* hookAt)
    {
        void* stub = Utils::AllocNear(reinterpret_cast<uintptr_t>(hookAt), 128);
        if (!stub) return false;

        const uintptr_t cbAddr = reinterpret_cast<uintptr_t>(&ChatHookCallback);
        const uintptr_t retAddr = reinterpret_cast<uintptr_t>(hookAt) + 5;

        uint8_t* p = static_cast<uint8_t*>(stub);

        // [+00] Original: MOV [RSP+8], RBX  (5 bytes)
        *p++ = 0x48; *p++ = 0x89; *p++ = 0x5C; *p++ = 0x24; *p++ = 0x08;

        // [+05] Save volatile integer registers & allocate shadow space.
        //       Stack layout after setup (RSP at +0x00 relative):
        //       [RSP+0x00..0x1F] = 32-byte shadow space for CALL
        //       [RSP+0x20] = saved R11
        //       [RSP+0x28] = saved R10
        //       [RSP+0x30] = saved R9
        //       [RSP+0x38] = saved R8
        //       [RSP+0x40] = saved RDX
        //       [RSP+0x48] = saved RCX
        //       [RSP+0x50] = saved RAX
        *p++ = 0x50;                    // PUSH RAX
        *p++ = 0x51;                    // PUSH RCX
        *p++ = 0x52;                    // PUSH RDX
        *p++ = 0x41; *p++ = 0x50;      // PUSH R8
        *p++ = 0x41; *p++ = 0x51;      // PUSH R9
        *p++ = 0x41; *p++ = 0x52;      // PUSH R10
        *p++ = 0x41; *p++ = 0x53;      // PUSH R11
        *p++ = 0x48; *p++ = 0x83; *p++ = 0xEC; *p++ = 0x20;  // SUB RSP, 32 (shadow space)

        // RCX = RDX (first argument to callback = captured RDX)
        *p++ = 0x48; *p++ = 0x8B; *p++ = 0xCA;  // MOV RCX, RDX

        // RAX = callback address
        *p++ = 0x48; *p++ = 0xB8;
        std::memcpy(p, &cbAddr, 8); p += 8;

        // CALL RAX
        *p++ = 0xFF; *p++ = 0xD0;

        // Undo shadow space & restore registers
        *p++ = 0x48; *p++ = 0x83; *p++ = 0xC4; *p++ = 0x20;  // ADD RSP, 32
        *p++ = 0x41; *p++ = 0x5B;      // POP R11
        *p++ = 0x41; *p++ = 0x5A;      // POP R10
        *p++ = 0x41; *p++ = 0x59;      // POP R9
        *p++ = 0x41; *p++ = 0x58;      // POP R8
        *p++ = 0x5A;                    // POP RDX
        *p++ = 0x59;                    // POP RCX
        *p++ = 0x58;                    // POP RAX

        // [+xx] JMP [RIP+0] -> retAddr  (absolute indirect)
        *p++ = 0xFF; *p++ = 0x25;
        *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00;
        std::memcpy(p, &retAddr, 8); p += 8;

        g_stub   = stub;
        g_hookAt = reinterpret_cast<uintptr_t>(hookAt);

        DWORD oldProt = 0;
        VirtualProtect(hookAt, 5, PAGE_EXECUTE_READWRITE, &oldProt);
        const int32_t rel = static_cast<int32_t>(
            reinterpret_cast<uintptr_t>(stub) - (reinterpret_cast<uintptr_t>(hookAt) + 5));
        hookAt[0] = 0xE9;
        std::memcpy(hookAt + 1, &rel, 4);
        VirtualProtect(hookAt, 5, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), hookAt, 5);
        return true;
    }

    inline void RemoveHook()
    {
        if (!g_hookAt || !g_stub) return;
        auto* hookAt = reinterpret_cast<uint8_t*>(g_hookAt);
        DWORD oldProt = 0;
        VirtualProtect(hookAt, 5, PAGE_EXECUTE_READWRITE, &oldProt);
        constexpr uint8_t orig[5] = {0x48, 0x89, 0x5C, 0x24, 0x08};
        std::memcpy(hookAt, orig, 5);
        VirtualProtect(hookAt, 5, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), hookAt, 5);
        VirtualFree(g_stub, 0, MEM_RELEASE);
        g_stub   = nullptr;
        g_hookAt = 0;
    }

    inline bool Initialize()
    {
        uint8_t* base = nullptr;
        size_t   size = 0;
        if (!Utils::GetModuleRange(L"DDS-Win64-Shipping.exe", base, size)) {
            LavenderConsole::GetInstance().Log("[ChatHook] FAILED: GetModuleRange");
            return false;
        }
        uint8_t* hit = Utils::ScanPattern(base, size, k_Pattern, k_Mask, sizeof(k_Pattern));
        if (!hit) {
            LavenderConsole::GetInstance().Log("[ChatHook] FAILED: AOB pattern not found");
            return false;
        }
        {
            char buf[128];
            _snprintf_s(buf, _TRUNCATE, "[ChatHook] pattern found at offset 0x%llX", (unsigned long long)(hit - base));
            LavenderConsole::GetInstance().Log(buf);
        }
        if (!InstallHook(hit)) {
            LavenderConsole::GetInstance().Log("[ChatHook] FAILED: InstallHook");
            return false;
        }
        Globals::chat_hook_active.store(true);
        LavenderConsole::GetInstance().Log("[ChatHook] initialized successfully");
        return true;
    }

    inline void Shutdown() { RemoveHook(); }

    inline void Tick()
    {
    }

}

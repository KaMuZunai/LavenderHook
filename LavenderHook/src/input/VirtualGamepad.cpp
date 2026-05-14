#include "VirtualGamepad.h"
#include "VkTable.h"
#include "../ui/components/Console.h"
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace LavenderHook::Input::VGamepad {

using PVIGEM_CLIENT = void*;
using PVIGEM_TARGET = void*;

#pragma pack(push,1)
struct XUSB_REPORT {
    unsigned short wButtons;
    unsigned char bLeftTrigger;
    unsigned char bRightTrigger;
    short sThumbLX;
    short sThumbLY;
    short sThumbRX;
    short sThumbRY;
};
#pragma pack(pop)

static constexpr unsigned int VIGEM_ERROR_NONE = 0x20000000u;

extern "C" {
    PVIGEM_CLIENT vigem_alloc();
    void vigem_free(PVIGEM_CLIENT client);
    unsigned int vigem_connect(PVIGEM_CLIENT client);
    PVIGEM_TARGET vigem_target_x360_alloc();
    void vigem_target_free(PVIGEM_TARGET target);
    unsigned int vigem_target_add(PVIGEM_CLIENT client, PVIGEM_TARGET target);
    unsigned int vigem_target_remove(PVIGEM_CLIENT client, PVIGEM_TARGET target);
    unsigned int vigem_target_x360_update(PVIGEM_CLIENT client, PVIGEM_TARGET target, XUSB_REPORT report);
    unsigned long vigem_target_get_index(PVIGEM_TARGET target);
}

static PVIGEM_CLIENT s_client = nullptr;
static PVIGEM_TARGET s_target = nullptr;
static DWORD s_user_index = 4;

static XUSB_REPORT s_report{};
static std::mutex s_report_mutex;
static std::atomic<bool> s_available{false};
static std::atomic<bool> s_failed{false};

// Button masks
static constexpr unsigned short XUSB_GAMEPAD_DPAD_UP    = 0x0001;
static constexpr unsigned short XUSB_GAMEPAD_DPAD_DOWN  = 0x0002;
static constexpr unsigned short XUSB_GAMEPAD_DPAD_LEFT  = 0x0004;
static constexpr unsigned short XUSB_GAMEPAD_DPAD_RIGHT = 0x0008;
static constexpr unsigned short XUSB_GAMEPAD_START      = 0x0010;
static constexpr unsigned short XUSB_GAMEPAD_BACK       = 0x0020;
static constexpr unsigned short XUSB_GAMEPAD_LEFT_THUMB = 0x0040;
static constexpr unsigned short XUSB_GAMEPAD_RIGHT_THUMB= 0x0080;
static constexpr unsigned short XUSB_GAMEPAD_LEFT_SHOULDER = 0x0100;
static constexpr unsigned short XUSB_GAMEPAD_RIGHT_SHOULDER= 0x0200;
static constexpr unsigned short XUSB_GAMEPAD_A = 0x1000;
static constexpr unsigned short XUSB_GAMEPAD_B = 0x2000;
static constexpr unsigned short XUSB_GAMEPAD_X = 0x4000;
static constexpr unsigned short XUSB_GAMEPAD_Y = 0x8000;

static void ClearReport() {
    s_report.wButtons = 0;
    s_report.bLeftTrigger = 0;
    s_report.bRightTrigger = 0;
    s_report.sThumbLX = 0;
    s_report.sThumbLY = 0;
    s_report.sThumbRX = 0;
    s_report.sThumbRY = 0;
}

static std::atomic<bool> s_initializing{false};

static void DoInitialize()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    LavenderConsole::GetInstance().Log("VirtualGamepad: background initialization started");

    PVIGEM_CLIENT client = vigem_alloc();
    if (!client) {
        LavenderConsole::GetInstance().Log("vigem_alloc failed");
        s_available.store(false);
        s_failed.store(true);
        s_initializing.store(false);
        return;
    }

    // Try connecting to the ViGEm bus
    unsigned int err = 0u;
    const int maxAttempts = 6;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        err = vigem_connect(client);
        if (err == VIGEM_ERROR_NONE) break;
        {
            std::ostringstream oss;
            oss << "vigem_connect failed (attempt " << attempt << "): 0x" << std::hex << err;
            LavenderConsole::GetInstance().Log(oss.str());
        }
        if (attempt < maxAttempts)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if (err != VIGEM_ERROR_NONE) {
        vigem_free(client);
        LavenderConsole::GetInstance().Log(std::string("vigem_connect final failure: ") + std::to_string(err));
        LavenderConsole::GetInstance().Log("Possible causes: ViGEmBus driver not installed/running, driver bitness mismatch, or insufficient privileges.");
        s_available.store(false);
        s_failed.store(true);
        s_initializing.store(false);
        return;
    }

    PVIGEM_TARGET target = vigem_target_x360_alloc();
    if (!target) {
        vigem_free(client);
        LavenderConsole::GetInstance().Log("vigem_target_x360_alloc failed");
        s_available.store(false);
        s_failed.store(true);
        s_initializing.store(false);
        return;
    }

    err = vigem_target_add(client, target);
    if (err != VIGEM_ERROR_NONE) {
        vigem_target_free(target);
        vigem_free(client);
        LavenderConsole::GetInstance().Log(std::string("vigem_target_add failed: ") + std::to_string(err));
        s_available.store(false);
        s_failed.store(true);
        s_initializing.store(false);
        return;
    }

    // commit to globals
    s_client = client;
    s_target = target;
    s_user_index = static_cast<DWORD>(vigem_target_get_index(target));
    ClearReport();
    s_failed.store(false);
    s_available.store(true);
    s_initializing.store(false);
    {
        std::ostringstream oss;
        oss << "ViGEm virtual gamepad initialized on XInput slot " << s_user_index;
        LavenderConsole::GetInstance().Log(oss.str());
    }
}

bool Initialize()
{
    if (s_available.load()) return true;
    if (s_initializing.load()) return false;

    // Start background init
    s_initializing.store(true);
    std::thread initThread(DoInitialize);
    initThread.detach();
    return false;
}

bool Available()
{
    return s_available.load();
}

bool Failed()
{
    return s_failed.load();
}

static void ApplyButtonMask(unsigned short mask, bool down)
{
    if (down) s_report.wButtons |= mask;
    else s_report.wButtons &= ~mask;
}

void SetButton(int gpvk, bool down)
{
    if (!Available()) return;

    std::lock_guard<std::mutex> lock(s_report_mutex);
    using namespace LavenderHook::UI::Lavender;
    switch (gpvk) {
        case GPVK_DPAD_UP:    ApplyButtonMask(XUSB_GAMEPAD_DPAD_UP, down); break;
        case GPVK_DPAD_DOWN:  ApplyButtonMask(XUSB_GAMEPAD_DPAD_DOWN, down); break;
        case GPVK_DPAD_LEFT:  ApplyButtonMask(XUSB_GAMEPAD_DPAD_LEFT, down); break;
        case GPVK_DPAD_RIGHT: ApplyButtonMask(XUSB_GAMEPAD_DPAD_RIGHT, down); break;
        case GPVK_START:      ApplyButtonMask(XUSB_GAMEPAD_START, down); break;
        case GPVK_BACK:       ApplyButtonMask(XUSB_GAMEPAD_BACK, down); break;
        case GPVK_LS:         ApplyButtonMask(XUSB_GAMEPAD_LEFT_THUMB, down); if (down) { s_report.sThumbLX = 0; s_report.sThumbLY = 0; } break;
        case GPVK_RS:         ApplyButtonMask(XUSB_GAMEPAD_RIGHT_THUMB, down); if (down) { s_report.sThumbRX = 0; s_report.sThumbRY = 0; } break;
        case GPVK_LB:         ApplyButtonMask(XUSB_GAMEPAD_LEFT_SHOULDER, down); break;
        case GPVK_RB:         ApplyButtonMask(XUSB_GAMEPAD_RIGHT_SHOULDER, down); break;
        case GPVK_A:          ApplyButtonMask(XUSB_GAMEPAD_A, down); break;
        case GPVK_B:          ApplyButtonMask(XUSB_GAMEPAD_B, down); break;
        case GPVK_X:          ApplyButtonMask(XUSB_GAMEPAD_X, down); break;
        case GPVK_Y:          ApplyButtonMask(XUSB_GAMEPAD_Y, down); break;
        case GPVK_LT:         s_report.bLeftTrigger = down ? 255 : 0; break;
        case GPVK_RT:         s_report.bRightTrigger = down ? 255 : 0; break;
        default: break;
    }
}

void Press(int gpvk)
{
    if (!Available()) return;
    SetButton(gpvk, true);
    Update();
    std::thread([gpvk]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        SetButton(gpvk, false);
        Update();
    }).detach();
}

bool AutomationActive()
{
    if (!s_available.load()) return false;
    return s_report.wButtons != 0 ||
           s_report.bLeftTrigger != 0 ||
           s_report.bRightTrigger != 0;
}

DWORD GetUserIndex()
{
    return s_user_index;
}

bool Initializing()
{
    return s_initializing.load();
}

void SetThumb(bool leftStick, float nx, float ny)
{
    if (!s_available.load()) return;
    nx = nx < -1.f ? -1.f : (nx > 1.f ? 1.f : nx);
    ny = ny < -1.f ? -1.f : (ny > 1.f ? 1.f : ny);
    const short sx = static_cast<short>(nx * 32767.f);
    const short sy = static_cast<short>(ny * 32767.f);
    std::lock_guard<std::mutex> lock(s_report_mutex);
    if (leftStick) { s_report.sThumbLX = sx; s_report.sThumbLY = sy; }
    else           { s_report.sThumbRX = sx; s_report.sThumbRY = sy; }
}

void Update()
{
    if (!Available()) return;
    if (!s_client || !s_target) return;
    XUSB_REPORT snapshot;
    {
        std::lock_guard<std::mutex> lock(s_report_mutex);
        snapshot = s_report;
    }
    vigem_target_x360_update(s_client, s_target, snapshot);
}

void Shutdown()
{
    if (!s_client && !s_target) return;

    if (s_client && s_target) {
        vigem_target_remove(s_client, s_target);
    }
    if (s_target) {
        vigem_target_free(s_target);
    }
    if (s_client) {
        vigem_free(s_client);
    }

    s_target = nullptr;
    s_client = nullptr;
    s_available.store(false);
}

} // namespace

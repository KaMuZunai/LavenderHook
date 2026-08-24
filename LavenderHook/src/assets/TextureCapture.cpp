#include "TextureCapture.h"
#include "../minhook/MinHook.h"
#include "../ui/UIWindows/console.h"
#include <cstdio>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <string>
#include <vector>
#include <unordered_set>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace
{
    enum { Vtbl_SetTexture = 65 };
    using SetTexture_t = HRESULT(APIENTRY*)(LPDIRECT3DDEVICE9, DWORD, IDirect3DBaseTexture9*);
    static SetTexture_t original_SetTexture = nullptr;

    static std::mutex g_mutex;
    static std::unordered_set<IDirect3DBaseTexture9*> g_seen;
    static std::vector<IDirect3DBaseTexture9*> g_pending;
    static std::string g_output_dir = "DumpedTextures";
    static bool g_initialized = false;
    static std::atomic<bool> g_enabled{ true };

    struct SaveTask
    {
        std::string path;
        int width;
        int height;
        std::vector<unsigned char> pixels;
    };

    static std::queue<SaveTask> g_save_queue;
    static std::mutex g_queue_mutex;
    static std::condition_variable g_queue_cv;
    static std::thread g_worker;
    static std::atomic<bool> g_worker_running{ false };

    static DWORD g_dump_counter = 0;

    static void DecompressDXT1Block(const BYTE* block, int stride, BYTE* out)
    {
        WORD c0 = block[0] | (block[1] << 8);
        WORD c1 = block[2] | (block[3] << 8);

        auto rgb565 = [](WORD c) {
            int r = (c >> 11) & 0x1F; r = (r << 3) | (r >> 2);
            int g = (c >> 5) & 0x3F;  g = (g << 2) | (g >> 4);
            int b = c & 0x1F;        b = (b << 3) | (b >> 2);
            return std::make_tuple(r, g, b);
        };

        auto [r0, g0, b0] = rgb565(c0);
        auto [r1, g1, b1] = rgb565(c1);

        int r2 = 0, g2 = 0, b2 = 0, r3 = 0, g3 = 0, b3 = 0;
        if (c0 > c1)
        {
            r2 = (2 * r0 + r1) / 3; g2 = (2 * g0 + g1) / 3; b2 = (2 * b0 + b1) / 3;
            r3 = (r0 + 2 * r1) / 3; g3 = (g0 + 2 * g1) / 3; b3 = (b0 + 2 * b1) / 3;
        }
        else
        {
            r2 = (r0 + r1) / 2; g2 = (g0 + g1) / 2; b2 = (b0 + b1) / 2;
        }

        DWORD indices = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);

        for (int y = 0; y < 4; y++)
        {
            for (int x = 0; x < 4; x++)
            {
                int idx = (indices >> (2 * (y * 4 + x))) & 3;
                int r, g, b, a = 255;
                switch (idx)
                {
                case 0: r = r0; g = g0; b = b0; break;
                case 1: r = r1; g = g1; b = b1; break;
                case 2: if (c0 > c1) { r = r2; g = g2; b = b2; } else { r = r2; g = g2; b = b2; } break;
                case 3: if (c0 > c1) { r = r3; g = g3; b = b3; } else { r = 0; g = 0; b = 0; a = 0; } break;
                }
                out[y * stride + x * 4 + 0] = (BYTE)r;
                out[y * stride + x * 4 + 1] = (BYTE)g;
                out[y * stride + x * 4 + 2] = (BYTE)b;
                out[y * stride + x * 4 + 3] = (BYTE)a;
            }
        }
    }

    static void DecompressDXT5Block(const BYTE* block, int stride, BYTE* out)
    {
        BYTE alphas[8];
        alphas[0] = block[0];
        alphas[1] = block[1];
        if (alphas[0] > alphas[1])
        {
            for (int i = 2; i < 8; i++)
                alphas[i] = (BYTE)(((8 - i) * alphas[0] + (i - 1) * alphas[1]) / 7);
        }
        else
        {
            for (int i = 2; i < 6; i++)
                alphas[i] = (BYTE)(((6 - i) * alphas[0] + (i - 1) * alphas[1]) / 5);
            alphas[6] = 0;
            alphas[7] = 255;
        }

        ULONG64 alphaBits = 0;
        for (int i = 0; i < 6; i++)
            alphaBits |= (ULONG64)block[2 + i] << (i * 8);

        DecompressDXT1Block(block + 8, stride, out);

        for (int y = 0; y < 4; y++)
        {
            for (int x = 0; x < 4; x++)
            {
                int idx = (alphaBits >> (3 * (y * 4 + x))) & 7;
                out[y * stride + x * 4 + 3] = alphas[idx];
            }
        }
    }

    static void DecompressDXT3Block(const BYTE* block, int stride, BYTE* out)
    {
        DecompressDXT1Block(block + 8, stride, out);
        for (int y = 0; y < 4; y++)
        {
            for (int x = 0; x < 4; x++)
            {
                WORD alphaWord = block[y * 2] | (block[y * 2 + 1] << 8);
                int a = ((alphaWord >> (x * 4)) & 0xF) * 17;
                out[y * stride + x * 4 + 3] = (BYTE)a;
            }
        }
    }

    static void DecompressBC(D3DFORMAT fmt, const void* src, int width, int height, int srcPitch, std::vector<unsigned char>& out)
    {
        out.resize(width * height * 4);
        int blockSize = (fmt == D3DFMT_DXT1) ? 8 : 16;
        int bw = (width + 3) / 4;
        int bh = (height + 3) / 4;

        for (int by = 0; by < bh; by++)
        {
            for (int bx = 0; bx < bw; bx++)
            {
                const BYTE* blockSrc = (const BYTE*)src + by * srcPitch + bx * blockSize;
                BYTE tmp[4 * 4 * 4];

                switch (fmt)
                {
                case D3DFMT_DXT1: DecompressDXT1Block(blockSrc, 4 * 4, tmp); break;
                case D3DFMT_DXT3: DecompressDXT3Block(blockSrc, 4 * 4, tmp); break;
                case D3DFMT_DXT5: DecompressDXT5Block(blockSrc, 4 * 4, tmp); break;
                default: return;
                }

                for (int py = 0; py < 4 && (by * 4 + py) < height; py++)
                {
                    for (int px = 0; px < 4 && (bx * 4 + px) < width; px++)
                    {
                        int di = ((by * 4 + py) * width + (bx * 4 + px)) * 4;
                        int si = (py * 4 + px) * 4;
                        out[di + 0] = tmp[si + 0];
                        out[di + 1] = tmp[si + 1];
                        out[di + 2] = tmp[si + 2];
                        out[di + 3] = tmp[si + 3];
                    }
                }
            }
        }
    }

    static void WorkerThreadProc()
    {
        while (g_worker_running.load())
        {
            SaveTask task;
            {
                std::unique_lock<std::mutex> lock(g_queue_mutex);
                g_queue_cv.wait_for(lock, std::chrono::milliseconds(200), [] {
                    return !g_save_queue.empty() || !g_worker_running.load();
                });
                if (!g_worker_running.load())
                    return;
                if (g_save_queue.empty())
                    continue;
                task = std::move(g_save_queue.front());
                g_save_queue.pop();
            }

            bool ok = stbi_write_png(task.path.c_str(), task.width, task.height, 4,
                task.pixels.data(), task.width * 4) != 0;

            LavenderConsole::GetInstance().Log(
                ok ? ("TextureCapture: saved " + task.path).c_str()
                   : ("TextureCapture: FAILED " + task.path).c_str());
        }
    }

    static void StartWorker()
    {
        if (g_worker_running.exchange(true))
            return;
        g_worker = std::thread(WorkerThreadProc);
    }

    static void StopWorker()
    {
        if (!g_worker_running.exchange(false))
            return;
        g_queue_cv.notify_all();
        if (g_worker.joinable())
            g_worker.join();

        std::lock_guard<std::mutex> lock(g_queue_mutex);
        while (!g_save_queue.empty())
            g_save_queue.pop();
    }

    static void QueueSaveTask(SaveTask&& task)
    {
        std::lock_guard<std::mutex> lock(g_queue_mutex);
        g_save_queue.push(std::move(task));
        g_queue_cv.notify_one();
    }

    static bool IsBlockCompressed(D3DFORMAT fmt)
    {
        return fmt == D3DFMT_DXT1 || fmt == D3DFMT_DXT2 || fmt == D3DFMT_DXT3
            || fmt == D3DFMT_DXT4 || fmt == D3DFMT_DXT5;
    }

    static bool ExtractAndQueue(LPDIRECT3DDEVICE9 device, IDirect3DTexture9* tex2d,
        D3DSURFACE_DESC& desc, const char* path)
    {
        if (desc.Pool == D3DPOOL_MANAGED || desc.Pool == D3DPOOL_SYSTEMMEM)
        {
            D3DLOCKED_RECT locked;
            if (FAILED(tex2d->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)))
                return false;

            if (IsBlockCompressed(desc.Format))
            {
                int bh = (desc.Height + 3) / 4;
                std::vector<unsigned char> compressed(locked.Pitch * bh);
                for (int y = 0; y < bh; y++)
                    memcpy(&compressed[y * locked.Pitch], (BYTE*)locked.pBits + y * locked.Pitch, locked.Pitch);

                tex2d->UnlockRect(0);

                SaveTask task;
                task.path = path;
                task.width = desc.Width;
                task.height = desc.Height;
                DecompressBC(desc.Format, compressed.data(), desc.Width, desc.Height, locked.Pitch, task.pixels);
                QueueSaveTask(std::move(task));
            }
            else
            {
                std::vector<unsigned char> pixels(desc.Width * desc.Height * 4);
                for (UINT y = 0; y < desc.Height; y++)
                {
                    const BYTE* src = (const BYTE*)locked.pBits + y * locked.Pitch;
                    BYTE* dst = &pixels[y * desc.Width * 4];
                    for (UINT x = 0; x < desc.Width; x++)
                    {
                        dst[x * 4 + 0] = src[x * 4 + 2];
                        dst[x * 4 + 1] = src[x * 4 + 1];
                        dst[x * 4 + 2] = src[x * 4 + 0];
                        dst[x * 4 + 3] = src[x * 4 + 3];
                    }
                }
                tex2d->UnlockRect(0);

                SaveTask task;
                task.path = path;
                task.width = desc.Width;
                task.height = desc.Height;
                task.pixels = std::move(pixels);
                QueueSaveTask(std::move(task));
            }
            return true;
        }

        IDirect3DSurface9* srcSurf = nullptr;
        if (FAILED(tex2d->GetSurfaceLevel(0, &srcSurf)))
            return false;

        IDirect3DSurface9* dstSurf = nullptr;
        HRESULT hr = device->CreateOffscreenPlainSurface(
            desc.Width, desc.Height, desc.Format,
            D3DPOOL_SYSTEMMEM, &dstSurf, nullptr
        );

        bool ok = false;
        if (SUCCEEDED(hr))
        {
            hr = device->GetRenderTargetData(srcSurf, dstSurf);
            if (FAILED(hr))
                hr = device->StretchRect(srcSurf, nullptr, dstSurf, nullptr, D3DTEXF_NONE);

            if (SUCCEEDED(hr))
            {
                D3DLOCKED_RECT locked;
                if (SUCCEEDED(dstSurf->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
                {
                    if (IsBlockCompressed(desc.Format))
                    {
                        int bh = (desc.Height + 3) / 4;
                        std::vector<unsigned char> compressed(locked.Pitch * bh);
                        for (int y = 0; y < bh; y++)
                            memcpy(&compressed[y * locked.Pitch], (BYTE*)locked.pBits + y * locked.Pitch, locked.Pitch);

                        SaveTask task;
                        task.path = path;
                        task.width = desc.Width;
                        task.height = desc.Height;
                        DecompressBC(desc.Format, compressed.data(), desc.Width, desc.Height, locked.Pitch, task.pixels);
                        QueueSaveTask(std::move(task));
                    }
                    else
                    {
                        std::vector<unsigned char> pixels(desc.Width * desc.Height * 4);
                        for (UINT y = 0; y < desc.Height; y++)
                        {
                            const BYTE* src = (const BYTE*)locked.pBits + y * locked.Pitch;
                            BYTE* dst = &pixels[y * desc.Width * 4];
                            for (UINT x = 0; x < desc.Width; x++)
                            {
                                dst[x * 4 + 0] = src[x * 4 + 2];
                                dst[x * 4 + 1] = src[x * 4 + 1];
                                dst[x * 4 + 2] = src[x * 4 + 0];
                                dst[x * 4 + 3] = src[x * 4 + 3];
                            }
                        }

                        SaveTask task;
                        task.path = path;
                        task.width = desc.Width;
                        task.height = desc.Height;
                        task.pixels = std::move(pixels);
                        QueueSaveTask(std::move(task));
                    }
                    ok = true;
                    dstSurf->UnlockRect();
                }
            }
            dstSurf->Release();
        }
        srcSurf->Release();
        return ok;
    }
}

static HRESULT APIENTRY HookedSetTexture(
    LPDIRECT3DDEVICE9 device,
    DWORD Stage,
    IDirect3DBaseTexture9* pTexture)
{
    if (pTexture && g_enabled.load())
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_seen.find(pTexture) == g_seen.end())
        {
            g_seen.insert(pTexture);
            pTexture->AddRef();
            g_pending.push_back(pTexture);
        }
    }
    return original_SetTexture(device, Stage, pTexture);
}

bool TextureCapture::Hook(LPDIRECT3DDEVICE9 device)
{
    if (g_initialized)
        return true;
    if (!device)
        return false;

    void** vtbl = *reinterpret_cast<void***>(device);
    MH_CreateHook(reinterpret_cast<void*>(vtbl[Vtbl_SetTexture]),
        &HookedSetTexture, reinterpret_cast<void**>(&original_SetTexture));
    MH_EnableHook(reinterpret_cast<void*>(vtbl[Vtbl_SetTexture]));

    CreateDirectoryA(g_output_dir.c_str(), nullptr);
    StartWorker();

    g_initialized = true;
    LavenderConsole::GetInstance().Log("TextureCapture: hook installed.");
    return true;
}

void TextureCapture::Unhook()
{
    if (!g_initialized)
        return;

    StopWorker();

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (auto* tex : g_pending)
            tex->Release();
        g_pending.clear();
        g_seen.clear();
    }

    g_initialized = false;
    LavenderConsole::GetInstance().Log("TextureCapture: unhooked.");
}

void TextureCapture::DumpQueuedTextures(LPDIRECT3DDEVICE9 device)
{
    if (!g_initialized || !g_enabled.load())
        return;

    std::vector<IDirect3DBaseTexture9*> batch;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_pending.empty())
            return;
        batch.swap(g_pending);
    }
    CreateDirectoryA(g_output_dir.c_str(), nullptr);

    for (auto* tex : batch)
    {
        D3DRESOURCETYPE type = tex->GetType();
        if (type != D3DRTYPE_TEXTURE)
        {
            tex->Release();
            continue;
        }

        IDirect3DTexture9* tex2d = static_cast<IDirect3DTexture9*>(tex);
        D3DSURFACE_DESC desc;
        if (FAILED(tex2d->GetLevelDesc(0, &desc)))
        {
            tex->Release();
            continue;
        }

        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s\\tex_%06u.png",
            g_output_dir.c_str(), ++g_dump_counter);

        if (!ExtractAndQueue(device, tex2d, desc, path))
        {
            LavenderConsole::GetInstance().Log(
                ("TextureCapture: FAILED " + std::string(path)).c_str());
        }

        tex->Release();
    }
}

void TextureCapture::SetOutputDirectory(const char* path)
{
    g_output_dir = path ? path : "DumpedTextures";
    CreateDirectoryA(g_output_dir.c_str(), nullptr);
}

bool TextureCapture::IsEnabled()
{
    return g_enabled.load();
}

void TextureCapture::SetEnabled(bool enabled)
{
    g_enabled.store(enabled);
}

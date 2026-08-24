#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_impl_dx9.h"
#include <d3d9.h>

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif

struct ImGui_ImplDX9_Data
{
    LPDIRECT3DDEVICE9           pd3dDevice;
    LPDIRECT3DVERTEXBUFFER9     pVB;
    LPDIRECT3DINDEXBUFFER9      pIB;
    int                         VertexBufferSize;
    int                         IndexBufferSize;
    bool                        HasRgbaSupport;

    ImGui_ImplDX9_Data()        { memset((void*)this, 0, sizeof(*this)); VertexBufferSize = 5000; IndexBufferSize = 10000; }
};

struct CUSTOMVERTEX
{
    float    pos[3];
    D3DCOLOR col;
    float    uv[2];
};
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)

#ifdef IMGUI_USE_BGRA_PACKED_COLOR
#define IMGUI_COL_TO_DX9_ARGB(_COL)     (_COL)
#else
#define IMGUI_COL_TO_DX9_ARGB(_COL)     (((_COL) & 0xFF00FF00) | (((_COL) & 0xFF0000) >> 16) | (((_COL) & 0xFF) << 16))
#endif

static ImGui_ImplDX9_Data* ImGui_ImplDX9_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplDX9_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

static void ImGui_ImplDX9_SetupRenderState(ImDrawData* draw_data)
{
    ImGui_ImplDX9_Data* bd = ImGui_ImplDX9_GetBackendData();

    D3DVIEWPORT9 vp;
    vp.X = vp.Y = 0;
    vp.Width = (DWORD)draw_data->DisplaySize.x;
    vp.Height = (DWORD)draw_data->DisplaySize.y;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;

    LPDIRECT3DDEVICE9 device = bd->pd3dDevice;
    device->SetViewport(&vp);

    device->SetPixelShader(nullptr);
    device->SetVertexShader(nullptr);
    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
    device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_RANGEFOGENABLE, FALSE);
    device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_CLIPPING, TRUE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    {
        float L = draw_data->DisplayPos.x + 0.5f;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x + 0.5f;
        float T = draw_data->DisplayPos.y + 0.5f;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y + 0.5f;
        D3DMATRIX mat_identity = { { { 1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f } } };
        D3DMATRIX mat_projection =
        { { {
            2.0f/(R-L),   0.0f,         0.0f,  0.0f,
            0.0f,         2.0f/(T-B),   0.0f,  0.0f,
            0.0f,         0.0f,         0.5f,  0.0f,
            (L+R)/(L-R),  (T+B)/(B-T),  0.5f,  1.0f
        } } };
        device->SetTransform(D3DTS_WORLD, &mat_identity);
        device->SetTransform(D3DTS_VIEW, &mat_identity);
        device->SetTransform(D3DTS_PROJECTION, &mat_projection);
    }
}

void ImGui_ImplDX9_RenderDrawData(ImDrawData* draw_data)
{
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    ImGui_ImplDX9_Data* bd = ImGui_ImplDX9_GetBackendData();
    LPDIRECT3DDEVICE9 device = bd->pd3dDevice;

    if (draw_data->Textures != nullptr)
        for (ImTextureData* tex : *draw_data->Textures)
            if (tex->Status != ImTextureStatus_OK)
                ImGui_ImplDX9_UpdateTexture(tex);

    if (!bd->pVB || bd->VertexBufferSize < draw_data->TotalVtxCount)
    {
        if (bd->pVB) { bd->pVB->Release(); bd->pVB = nullptr; }
        bd->VertexBufferSize = draw_data->TotalVtxCount + 5000;
        if (device->CreateVertexBuffer(bd->VertexBufferSize * sizeof(CUSTOMVERTEX), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &bd->pVB, nullptr) < 0)
            return;
    }
    if (!bd->pIB || bd->IndexBufferSize < draw_data->TotalIdxCount)
    {
        if (bd->pIB) { bd->pIB->Release(); bd->pIB = nullptr; }
        bd->IndexBufferSize = draw_data->TotalIdxCount + 10000;
        if (device->CreateIndexBuffer(bd->IndexBufferSize * sizeof(ImDrawIdx), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, sizeof(ImDrawIdx) == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &bd->pIB, nullptr) < 0)
            return;
    }

    IDirect3DStateBlock9* state_block = nullptr;
    if (device->CreateStateBlock(D3DSBT_ALL, &state_block) < 0)
        return;
    if (state_block->Capture() < 0)
    {
        state_block->Release();
        return;
    }

    D3DMATRIX last_world, last_view, last_projection;
    device->GetTransform(D3DTS_WORLD, &last_world);
    device->GetTransform(D3DTS_VIEW, &last_view);
    device->GetTransform(D3DTS_PROJECTION, &last_projection);

    CUSTOMVERTEX* vtx_dst;
    ImDrawIdx* idx_dst;
    if (bd->pVB->Lock(0, (UINT)(draw_data->TotalVtxCount * sizeof(CUSTOMVERTEX)), (void**)&vtx_dst, D3DLOCK_DISCARD) < 0)
    {
        state_block->Release();
        return;
    }
    if (bd->pIB->Lock(0, (UINT)(draw_data->TotalIdxCount * sizeof(ImDrawIdx)), (void**)&idx_dst, D3DLOCK_DISCARD) < 0)
    {
        bd->pVB->Unlock();
        state_block->Release();
        return;
    }

    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* draw_list = draw_data->CmdLists[n];
        const ImDrawVert* vtx_src = draw_list->VtxBuffer.Data;
        for (int i = 0; i < draw_list->VtxBuffer.Size; i++)
        {
            vtx_dst->pos[0] = vtx_src->pos.x;
            vtx_dst->pos[1] = vtx_src->pos.y;
            vtx_dst->pos[2] = 0.0f;
            vtx_dst->col = IMGUI_COL_TO_DX9_ARGB(vtx_src->col);
            vtx_dst->uv[0] = vtx_src->uv.x;
            vtx_dst->uv[1] = vtx_src->uv.y;
            vtx_dst++;
            vtx_src++;
        }
        memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        idx_dst += draw_list->IdxBuffer.Size;
    }
    bd->pVB->Unlock();
    bd->pIB->Unlock();
    device->SetStreamSource(0, bd->pVB, 0, sizeof(CUSTOMVERTEX));
    device->SetIndices(bd->pIB);
    device->SetFVF(D3DFVF_CUSTOMVERTEX);

    ImGui_ImplDX9_SetupRenderState(draw_data);

    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* draw_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplDX9_SetupRenderState(draw_data);
                else
                    pcmd->UserCallback(draw_list, pcmd);
            }
            else
            {
                ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
                ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                const RECT r = { (LONG)clip_min.x, (LONG)clip_min.y, (LONG)clip_max.x, (LONG)clip_max.y };
                device->SetScissorRect(&r);

                const LPDIRECT3DTEXTURE9 texture = (LPDIRECT3DTEXTURE9)pcmd->GetTexID();
                device->SetTexture(0, texture);
                device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, pcmd->VtxOffset + global_vtx_offset, 0, (UINT)draw_list->VtxBuffer.Size, pcmd->IdxOffset + global_idx_offset, pcmd->ElemCount / 3);
            }
        }
        global_idx_offset += draw_list->IdxBuffer.Size;
        global_vtx_offset += draw_list->VtxBuffer.Size;
    }

    device->SetTransform(D3DTS_WORLD, &last_world);
    device->SetTransform(D3DTS_VIEW, &last_view);
    device->SetTransform(D3DTS_PROJECTION, &last_projection);

    state_block->Apply();
    state_block->Release();
}

static bool ImGui_ImplDX9_CheckFormatSupport(LPDIRECT3DDEVICE9 pDevice, D3DFORMAT format)
{
    LPDIRECT3D9 pd3d = nullptr;
    if (pDevice->GetDirect3D(&pd3d) != D3D_OK)
        return false;
    D3DDEVICE_CREATION_PARAMETERS param = {};
    D3DDISPLAYMODE mode = {};
    if (pDevice->GetCreationParameters(&param) != D3D_OK || pDevice->GetDisplayMode(0, &mode) != D3D_OK)
    {
        pd3d->Release();
        return false;
    }
    bool support = (pd3d->CheckDeviceFormat(param.AdapterOrdinal, param.DeviceType, mode.Format, D3DUSAGE_DYNAMIC | D3DUSAGE_QUERY_FILTER | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, D3DRTYPE_TEXTURE, format)) == D3D_OK;
    pd3d->Release();
    return support;
}

bool ImGui_ImplDX9_Init(IDirect3DDevice9* device)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    ImGui_ImplDX9_Data* bd = IM_NEW(ImGui_ImplDX9_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_dx9";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_TextureMaxWidth = platform_io.Renderer_TextureMaxHeight = 4096;

    bd->pd3dDevice = device;
    bd->pd3dDevice->AddRef();
    bd->HasRgbaSupport = ImGui_ImplDX9_CheckFormatSupport(bd->pd3dDevice, D3DFMT_A8B8G8R8);

    return true;
}

void ImGui_ImplDX9_Shutdown()
{
    ImGui_ImplDX9_Data* bd = ImGui_ImplDX9_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplDX9_InvalidateDeviceObjects();
    if (bd->pd3dDevice) { bd->pd3dDevice->Release(); }
    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
    IM_DELETE(bd);
}

static void ImGui_ImplDX9_CopyTextureRegion(bool tex_use_colors, const ImU32* src, int src_pitch, ImU32* dst, int dst_pitch, int w, int h)
{
#ifndef IMGUI_USE_BGRA_PACKED_COLOR
    ImGui_ImplDX9_Data* bd = ImGui_ImplDX9_GetBackendData();
    const bool convert_rgba_to_bgra = (!bd->HasRgbaSupport && tex_use_colors);
#else
    const bool convert_rgba_to_bgra = false;
    IM_UNUSED(tex_use_colors);
#endif
    for (int y = 0; y < h; y++)
    {
        const ImU32* src_p = (const ImU32*)(const void*)((const unsigned char*)src + src_pitch * y);
        ImU32* dst_p = (ImU32*)(void*)((unsigned char*)dst + dst_pitch * y);
        if (convert_rgba_to_bgra)
            for (int x = w; x > 0; x--, src_p++, dst_p++)
                *dst_p = IMGUI_COL_TO_DX9_ARGB(*src_p);
        else
            memcpy(dst_p, src_p, w * 4);
    }
}

void ImGui_ImplDX9_UpdateTexture(ImTextureData* tex)
{
    ImGui_ImplDX9_Data* bd = ImGui_ImplDX9_GetBackendData();

    if (tex->Status == ImTextureStatus_WantCreate)
    {
        IM_ASSERT(tex->TexID == ImTextureID_Invalid && tex->BackendUserData == nullptr);
        IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);
        LPDIRECT3DTEXTURE9 dx_tex = nullptr;
        HRESULT hr = bd->pd3dDevice->CreateTexture(tex->Width, tex->Height, 1, D3DUSAGE_DYNAMIC, bd->HasRgbaSupport ? D3DFMT_A8B8G8R8 : D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &dx_tex, nullptr);
        if (hr < 0)
        {
            IM_ASSERT(hr >= 0 && "Backend failed to create texture!");
            return;
        }

        D3DLOCKED_RECT locked_rect;
        if (dx_tex->LockRect(0, &locked_rect, nullptr, 0) == D3D_OK)
        {
            ImGui_ImplDX9_CopyTextureRegion(tex->UseColors, (ImU32*)tex->GetPixels(), tex->Width * 4, (ImU32*)locked_rect.pBits, (ImU32)locked_rect.Pitch, tex->Width, tex->Height);
            dx_tex->UnlockRect(0);
        }

        tex->SetTexID((ImTextureID)(intptr_t)dx_tex);
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantUpdates)
    {
        LPDIRECT3DTEXTURE9 backend_tex = (LPDIRECT3DTEXTURE9)(intptr_t)tex->TexID;
        RECT update_rect = { (LONG)tex->UpdateRect.x, (LONG)tex->UpdateRect.y, (LONG)(tex->UpdateRect.x + tex->UpdateRect.w), (LONG)(tex->UpdateRect.y + tex->UpdateRect.h) };
        D3DLOCKED_RECT locked_rect;
        if (backend_tex->LockRect(0, &locked_rect, &update_rect, 0) == D3D_OK)
            for (ImTextureRect& r : tex->Updates)
                ImGui_ImplDX9_CopyTextureRegion(tex->UseColors, (ImU32*)tex->GetPixelsAt(r.x, r.y), tex->Width * 4,
                    (ImU32*)locked_rect.pBits + (r.x - update_rect.left) + (r.y - update_rect.top) * (locked_rect.Pitch / 4), (int)locked_rect.Pitch, r.w, r.h);
        backend_tex->UnlockRect(0);
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantDestroy)
    {
        LPDIRECT3DTEXTURE9 backend_tex = (LPDIRECT3DTEXTURE9)tex->TexID;
        if (backend_tex == nullptr)
            return;
        IM_ASSERT(tex->TexID == (ImTextureID)(intptr_t)backend_tex);
        backend_tex->Release();

        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}

bool ImGui_ImplDX9_CreateDeviceObjects()
{
    ImGui_ImplDX9_Data* bd = ImGui_ImplDX9_GetBackendData();
    if (!bd || !bd->pd3dDevice)
        return false;
    return true;
}

void ImGui_ImplDX9_InvalidateDeviceObjects()
{
    ImGui_ImplDX9_Data* bd = ImGui_ImplDX9_GetBackendData();
    if (!bd || !bd->pd3dDevice)
        return;

    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
        if (tex->RefCount == 1)
        {
            tex->SetStatus(ImTextureStatus_WantDestroy);
            ImGui_ImplDX9_UpdateTexture(tex);
        }
    if (bd->pVB) { bd->pVB->Release(); bd->pVB = nullptr; }
    if (bd->pIB) { bd->pIB->Release(); bd->pIB = nullptr; }
}

void ImGui_ImplDX9_NewFrame()
{
    ImGui_ImplDX9_Data* bd = ImGui_ImplDX9_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplDX9_Init()?");
    IM_UNUSED(bd);
}

#endif

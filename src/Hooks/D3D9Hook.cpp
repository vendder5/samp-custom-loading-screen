#include "D3D9Hook.h"
#include <detours/detours.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>

#include "../Utils/TextureLoader.h"
#include "../Utils/DataLoader.h"
#include "../Utils/Config.h"
#include "../Utils/ISprite.h"

namespace D3D9Hook
{
    using Direct3DCreate9_t = IDirect3D9*(WINAPI*)(UINT);
    using CreateDevice_t      = HRESULT(WINAPI*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
    using EndScene_t          = HRESULT(WINAPI*)(IDirect3DDevice9*);
    using Reset_t             = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
    using SetTexture_t        = HRESULT(WINAPI*)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
    using DrawPrimitiveUP_t   = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, CONST void*, UINT);
    using DrawIndexedPrimitiveUP_t = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT, UINT, CONST void*, D3DFORMAT, CONST void*, UINT);

    static Direct3DCreate9_t oDirect3DCreate9 = nullptr;
    static CreateDevice_t    oCreateDevice    = nullptr;
    static EndScene_t        oEndScene        = nullptr;
    static Reset_t           oReset           = nullptr;
    static SetTexture_t      oSetTexture      = nullptr;
    static DrawPrimitiveUP_t oDrawPrimitiveUP = nullptr;
    static DrawIndexedPrimitiveUP_t oDrawIndexedPrimitiveUP = nullptr;

    static bool m_bInitialized = false;

    static ISprite* m_SplashSprite = nullptr;
    static bool m_bTextureLoaded = false;

    enum class FitMode
    {
        STRETCH,
        FIT,
        FILL
    };
    static FitMode m_FitMode = FitMode::STRETCH;

    static std::atomic<bool> m_bLoaderThreadStarted = false;
    static std::atomic<bool> m_bLoadingFinished = false;
    static std::atomic<TextureLoader::DecodedSpriteData*> m_DecodedData = nullptr;
    static IDirect3DBaseTexture9* m_DefaultLoadingTexture = nullptr;

    struct Vertex
    {
        float x, y, z, rhw;
        float u, v;
    };

    static int* pGameState = (int*)0xC8D4C0;

    IDirect3D9* WINAPI Hook_Direct3DCreate9(UINT SDKVersion);
    HRESULT WINAPI Hook_CreateDevice(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface);
    HRESULT WINAPI Hook_EndScene(IDirect3DDevice9* pDevice);
    HRESULT WINAPI Hook_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);
    HRESULT WINAPI Hook_SetTexture(IDirect3DDevice9* pDevice, DWORD Stage, IDirect3DBaseTexture9* pTexture);
    HRESULT WINAPI Hook_DrawPrimitiveUP(IDirect3DDevice9* pDevice, D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
    HRESULT WINAPI Hook_DrawIndexedPrimitiveUP(IDirect3DDevice9* pDevice, D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride);

    bool IsValidReadPtr(void* ptr, size_t size)
    {
        if (!ptr) return false;
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
        return (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    }

    void CalculateVertices(float screenW, float screenH, float imgW, float imgH, FitMode mode, Vertex* verts)
    {
        float targetW = screenW;
        float targetH = screenH;
        float xOffset = 0.0f;
        float yOffset = 0.0f;

        if (mode != FitMode::STRETCH && imgW > 0.0f && imgH > 0.0f)
        {
            float screenAspect = screenW / screenH;
            float imgAspect = imgW / imgH;

            if (mode == FitMode::FIT)
            {
                if (imgAspect > screenAspect)
                {
                    targetW = screenW;
                    targetH = screenW / imgAspect;
                    yOffset = (screenH - targetH) / 2.0f;
                }
                else
                {
                    targetH = screenH;
                    targetW = screenH * imgAspect;
                    xOffset = (screenW - targetW) / 2.0f;
                }
            }
            else if (mode == FitMode::FILL)
            {
                if (imgAspect > screenAspect)
                {
                    targetH = screenH;
                    targetW = screenH * imgAspect;
                    xOffset = (screenW - targetW) / 2.0f;
                }
                else
                {
                    targetW = screenW;
                    targetH = screenW / imgAspect;
                    yOffset = (screenH - targetH) / 2.0f;
                }
            }
        }

        verts[0] = { xOffset - 0.5f,           yOffset - 0.5f,           0.5f, 1.0f, 0.0f, 0.0f };
        verts[1] = { xOffset + targetW - 0.5f, yOffset - 0.5f,           0.5f, 1.0f, 1.0f, 0.0f };
        verts[2] = { xOffset - 0.5f,           yOffset + targetH - 0.5f, 0.5f, 1.0f, 0.0f, 1.0f };
        verts[3] = { xOffset + targetW - 0.5f, yOffset + targetH - 0.5f, 0.5f, 1.0f, 1.0f, 1.0f };
    }

    void AsyncLoadProc()
    {
        std::string url = Config::GetConfigValue("loadscs_config.cfg", "IMAGE_URL");
        std::string modeStr = Config::GetConfigValue("loadscs_config.cfg", "FIT_MODE");
        
        std::transform(modeStr.begin(), modeStr.end(), modeStr.begin(), ::toupper);
        if (modeStr == "FIT") m_FitMode = FitMode::FIT;
        else if (modeStr == "FILL") m_FitMode = FitMode::FILL;
        else m_FitMode = FitMode::STRETCH;

        std::string cachePath = Config::GetAbsoluteGamePath("loadscs/cache/cached_image.dat");
        std::vector<unsigned char> data;
        bool loadedFromCache = false;

        if (!url.empty())
        {
            if (std::filesystem::exists(cachePath))
            {
                data = DataLoader::LoadFromFile(cachePath);
                if (!data.empty())
                {
                    loadedFromCache = true;
                }
            }

            if (loadedFromCache)
            {
                auto decoded = TextureLoader::DecodeSpriteFromMemory(data);
                if (decoded)
                {
                    m_DecodedData = decoded;
                    m_bLoadingFinished = true;
                }
            }

            std::string tempDownloadPath = Config::GetAbsoluteGamePath("loadscs/cache/temp_download.dat");
            std::filesystem::create_directories(std::filesystem::path(cachePath).parent_path());
            
            std::error_code ec;
            std::filesystem::remove(tempDownloadPath, ec);

            if (DataLoader::DownloadFile(url, tempDownloadPath))
            {
                auto downloadedData = DataLoader::LoadFromFile(tempDownloadPath);
                if (!downloadedData.empty())
                {
                    bool shouldUpdate = !loadedFromCache;
                    if (loadedFromCache)
                    {
                        if (downloadedData != data)
                        {
                            shouldUpdate = true;
                        }
                    }

                    if (shouldUpdate)
                    {
                        std::filesystem::copy_file(tempDownloadPath, cachePath, std::filesystem::copy_options::overwrite_existing, ec);
                        
                        auto decoded = TextureLoader::DecodeSpriteFromMemory(downloadedData);
                        if (decoded)
                        {
                            auto oldDecoded = m_DecodedData.exchange(decoded);
                            if (oldDecoded) delete oldDecoded;
                            m_bLoadingFinished = true;
                        }
                    }
                }
            }
            std::filesystem::remove(tempDownloadPath, ec);
        }
        else
        {
            const std::vector<std::string> extensions = { ".png", ".jpg", ".jpeg", ".bmp", ".gif" };
            std::string foundPath = "";

            for (const auto& ext : extensions)
            {
                std::string path = Config::GetAbsoluteGamePath("loadscs/loading_screen" + ext);
                if (std::filesystem::exists(path))
                {
                    foundPath = path;
                    break;
                }
            }

            if (!foundPath.empty())
            {
                data = DataLoader::LoadFromFile(foundPath);
                if (!data.empty())
                {
                    auto decoded = TextureLoader::DecodeSpriteFromMemory(data);
                    if (decoded)
                    {
                        m_DecodedData = decoded;
                        m_bLoadingFinished = true;
                    }
                }
            }
        }

        m_bLoadingFinished = true;
    }

    void UpdateLoadingState(IDirect3DDevice9* pDevice)
    {
        if (!m_bTextureLoaded)
        {
            bool expected = false;
            if (m_bLoaderThreadStarted.compare_exchange_strong(expected, true))
            {
                std::thread(AsyncLoadProc).detach();
            }

            if (m_bLoadingFinished)
            {
                TextureLoader::DecodedSpriteData* decoded = m_DecodedData.exchange(nullptr);
                if (decoded)
                {
                    m_SplashSprite = TextureLoader::CreateSpriteFromDecodedData(pDevice, decoded);
                    delete decoded;
                }
                m_bTextureLoaded = true;
            }
        }
    }

    void Install()
    {
        HMODULE hD3D9 = GetModuleHandleW(L"d3d9.dll");
        if (!hD3D9) hD3D9 = LoadLibraryW(L"d3d9.dll");

        if (hD3D9)
        {
            oDirect3DCreate9 = (Direct3DCreate9_t)GetProcAddress(hD3D9, "Direct3DCreate9");

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&)oDirect3DCreate9, Hook_Direct3DCreate9);
            DetourTransactionCommit();
        }
    }

    void Uninstall()
    {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        
        if (oDirect3DCreate9)
            DetourDetach(&(PVOID&)oDirect3DCreate9, Hook_Direct3DCreate9);

        if (oCreateDevice)
            DetourDetach(&(PVOID&)oCreateDevice, Hook_CreateDevice);

        if (oEndScene)
            DetourDetach(&(PVOID&)oEndScene, Hook_EndScene);

        if (oReset)
            DetourDetach(&(PVOID&)oReset, Hook_Reset);

        if (oSetTexture)
            DetourDetach(&(PVOID&)oSetTexture, Hook_SetTexture);

        if (oDrawPrimitiveUP)
            DetourDetach(&(PVOID&)oDrawPrimitiveUP, Hook_DrawPrimitiveUP);

        if (oDrawIndexedPrimitiveUP)
            DetourDetach(&(PVOID&)oDrawIndexedPrimitiveUP, Hook_DrawIndexedPrimitiveUP);
        
        DetourTransactionCommit();
        
        if (m_SplashSprite)
        {
            delete m_SplashSprite;
            m_SplashSprite = nullptr;
        }

        m_DefaultLoadingTexture = nullptr;

        TextureLoader::DecodedSpriteData* decoded = m_DecodedData.exchange(nullptr);
        if (decoded)
        {
            delete decoded;
        }
    }

    IDirect3D9* WINAPI Hook_Direct3DCreate9(UINT SDKVersion)
    {
        IDirect3D9* pD3D = oDirect3DCreate9(SDKVersion);

        if (pD3D && !m_bInitialized)
        {
            void** vTable = *(void***)pD3D;
            oCreateDevice = (CreateDevice_t)vTable[16];

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&)oCreateDevice, Hook_CreateDevice);
            DetourTransactionCommit();

            m_bInitialized = true;
        }

        return pD3D;
    }

    HRESULT WINAPI Hook_CreateDevice(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
    {
        HRESULT hr = oCreateDevice(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

        if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface)
        {
            IDirect3DDevice9* pDevice = *ppReturnedDeviceInterface;
            void** vTable = *(void***)pDevice;

            oEndScene   = (EndScene_t)vTable[42];
            oReset      = (Reset_t)vTable[16];
            oSetTexture = (SetTexture_t)vTable[65];
            oDrawPrimitiveUP = (DrawPrimitiveUP_t)vTable[83];
            oDrawIndexedPrimitiveUP = (DrawIndexedPrimitiveUP_t)vTable[84];

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&)oEndScene, Hook_EndScene);
            DetourAttach(&(PVOID&)oReset, Hook_Reset);
            DetourAttach(&(PVOID&)oSetTexture, Hook_SetTexture);
            DetourAttach(&(PVOID&)oDrawPrimitiveUP, Hook_DrawPrimitiveUP);
            DetourAttach(&(PVOID&)oDrawIndexedPrimitiveUP, Hook_DrawIndexedPrimitiveUP);
            DetourTransactionCommit();
        }

        return hr;
    }

    HRESULT WINAPI Hook_EndScene(IDirect3DDevice9* pDevice)
    {
        return oEndScene(pDevice);
    }

    HRESULT WINAPI Hook_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
    {
        if (m_SplashSprite)
        {
            delete m_SplashSprite;
            m_SplashSprite = nullptr;
        }
        m_bTextureLoaded = false;
        m_bLoaderThreadStarted = false;
        m_bLoadingFinished = false;
        m_DefaultLoadingTexture = nullptr;
        
        TextureLoader::DecodedSpriteData* decoded = m_DecodedData.exchange(nullptr);
        if (decoded)
        {
            delete decoded;
        }

        return oReset(pDevice, pPresentationParameters);
    }

    HRESULT WINAPI Hook_DrawPrimitiveUP(IDirect3DDevice9* pDevice, D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
    {
        UpdateLoadingState(pDevice);

        if (m_SplashSprite)
        {
            m_SplashSprite->Update();

            IDirect3DBaseTexture9* pCurrentTex = nullptr;
            IDirect3DBaseTexture9* pMyTex = m_SplashSprite->GetTexture();

            if (pMyTex && SUCCEEDED(pDevice->GetTexture(0, &pCurrentTex)) && pCurrentTex)
            {
                bool isDefaultTex = (m_DefaultLoadingTexture && pCurrentTex == m_DefaultLoadingTexture);
                bool isMyTexture = (pCurrentTex == pMyTex || isDefaultTex);

                if (isMyTexture)
                {
                    IDirect3DSurface9* pRenderTarget = nullptr;
                    if (SUCCEEDED(pDevice->GetRenderTarget(0, &pRenderTarget)) && pRenderTarget)
                    {
                        D3DSURFACE_DESC desc;
                        pRenderTarget->GetDesc(&desc);
                        pRenderTarget->Release();

                        float w = (float)desc.Width;
                        float h = (float)desc.Height;

                        DWORD oldScissor, oldZEnable, oldCull, oldFill;
                        pDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &oldScissor);
                        pDevice->GetRenderState(D3DRS_ZENABLE, &oldZEnable);
                        pDevice->GetRenderState(D3DRS_CULLMODE, &oldCull);
                        pDevice->GetRenderState(D3DRS_FILLMODE, &oldFill);

                        pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
                        pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
                        pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
                        pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
                        
                        D3DVIEWPORT9 oldVp;
                        pDevice->GetViewport(&oldVp);

                        D3DVIEWPORT9 newVp = { 0, 0, (DWORD)w, (DWORD)h, 0.0f, 1.0f };
                        pDevice->SetViewport(&newVp);

                        pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 1.0f, 0);

                        Vertex verts[4];
                        CalculateVertices(w, h, (float)m_SplashSprite->GetWidth(), (float)m_SplashSprite->GetHeight(), m_FitMode, verts);

                        if (isDefaultTex)
                        {
                            pDevice->SetTexture(0, pMyTex);
                        }

                        pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
                        HRESULT res = oDrawPrimitiveUP(pDevice, D3DPT_TRIANGLESTRIP, 2, verts, sizeof(Vertex));

                        if (isDefaultTex)
                        {
                            pDevice->SetTexture(0, pCurrentTex);
                        }

                        pDevice->SetViewport(&oldVp);
                        pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, oldScissor);
                        pDevice->SetRenderState(D3DRS_ZENABLE, oldZEnable);
                        pDevice->SetRenderState(D3DRS_CULLMODE, oldCull);
                        pDevice->SetRenderState(D3DRS_FILLMODE, oldFill);

                        pCurrentTex->Release();
                        return res;
                    }
                }
                pCurrentTex->Release();
            }
        }
        return oDrawPrimitiveUP(pDevice, PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
    }

    HRESULT WINAPI Hook_DrawIndexedPrimitiveUP(IDirect3DDevice9* pDevice, D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
    {
        UpdateLoadingState(pDevice);

        if (m_SplashSprite)
        {
            m_SplashSprite->Update();

            IDirect3DBaseTexture9* pCurrentTex = nullptr;
            IDirect3DBaseTexture9* pMyTex = m_SplashSprite->GetTexture();

            if (pMyTex && SUCCEEDED(pDevice->GetTexture(0, &pCurrentTex)) && pCurrentTex)
            {
                bool isDefaultTex = (m_DefaultLoadingTexture && pCurrentTex == m_DefaultLoadingTexture);
                bool isMyTexture = (pCurrentTex == pMyTex || isDefaultTex);

                if (isMyTexture)
                {
                    IDirect3DSurface9* pRenderTarget = nullptr;
                    if (SUCCEEDED(pDevice->GetRenderTarget(0, &pRenderTarget)) && pRenderTarget)
                    {
                        D3DSURFACE_DESC desc;
                        pRenderTarget->GetDesc(&desc);
                        pRenderTarget->Release();

                        float w = (float)desc.Width;
                        float h = (float)desc.Height;

                        DWORD oldScissor, oldZEnable, oldCull, oldFill;
                        pDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &oldScissor);
                        pDevice->GetRenderState(D3DRS_ZENABLE, &oldZEnable);
                        pDevice->GetRenderState(D3DRS_CULLMODE, &oldCull);
                        pDevice->GetRenderState(D3DRS_FILLMODE, &oldFill);

                        pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
                        pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
                        pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
                        pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

                        D3DVIEWPORT9 oldVp;
                        pDevice->GetViewport(&oldVp);

                        D3DVIEWPORT9 newVp = { 0, 0, (DWORD)w, (DWORD)h, 0.0f, 1.0f };
                        pDevice->SetViewport(&newVp);

                        pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 1.0f, 0);

                        Vertex verts[4];
                        CalculateVertices(w, h, (float)m_SplashSprite->GetWidth(), (float)m_SplashSprite->GetHeight(), m_FitMode, verts);

                        if (isDefaultTex)
                        {
                            pDevice->SetTexture(0, pMyTex);
                        }

                        pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
                        HRESULT res = oDrawPrimitiveUP(pDevice, D3DPT_TRIANGLESTRIP, 2, verts, sizeof(Vertex));

                        if (isDefaultTex)
                        {
                            pDevice->SetTexture(0, pCurrentTex);
                        }

                        pDevice->SetViewport(&oldVp);
                        pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, oldScissor);
                        pDevice->SetRenderState(D3DRS_ZENABLE, oldZEnable);
                        pDevice->SetRenderState(D3DRS_CULLMODE, oldCull);
                        pDevice->SetRenderState(D3DRS_FILLMODE, oldFill);

                        pCurrentTex->Release();
                        return res;
                    }
                }
                pCurrentTex->Release();
            }
        }
        return oDrawIndexedPrimitiveUP(pDevice, PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
    }

    HRESULT WINAPI Hook_SetTexture(IDirect3DDevice9* pDevice, DWORD Stage, IDirect3DBaseTexture9* pTexture)
    {
        UpdateLoadingState(pDevice);

        if (m_SplashSprite)
            m_SplashSprite->Update();

        if (Stage == 0 && pTexture)
        {
            static DWORD fvf;
            if (SUCCEEDED(pDevice->GetFVF(&fvf)) && (fvf & D3DFVF_XYZRHW))
            {
                int state = -1;
                if (IsValidReadPtr(pGameState, sizeof(int)))
                {
                    state = *pGameState;
                }
                
                if (state >= 0 && state <= 8)
                {
                    IDirect3DTexture9* pTex2D = static_cast<IDirect3DTexture9*>(pTexture);
                    D3DSURFACE_DESC desc;
                    
                    if (pTexture->GetType() == D3DRTYPE_TEXTURE && SUCCEEDED(pTex2D->GetLevelDesc(0, &desc)))
                    {
                        if (desc.Width > 256 && desc.Height > 256)
                        {
                             m_DefaultLoadingTexture = pTexture;

                             if (m_SplashSprite)
                             {
                                 pDevice->SetSamplerState(Stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                                 pDevice->SetSamplerState(Stage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                 pDevice->SetSamplerState(Stage, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
                                 
                                 return oSetTexture(pDevice, Stage, m_SplashSprite->GetTexture());
                             }
                        }
                    }
                }
            }
        }

        return oSetTexture(pDevice, Stage, pTexture);
    }
}
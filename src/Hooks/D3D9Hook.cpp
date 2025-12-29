#include "D3D9Hook.h"
#include <detours/detours.h>
#include <iostream>

#include "../Utils/TextureLoader.h"

namespace D3D9Hook
{
    using Direct3DCreate9_t = IDirect3D9*(WINAPI*)(UINT);
    using CreateDevice_t      = HRESULT(WINAPI*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
    using EndScene_t          = HRESULT(WINAPI*)(IDirect3DDevice9*);
    using Reset_t             = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
    using SetTexture_t        = HRESULT(WINAPI*)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);

    static Direct3DCreate9_t oDirect3DCreate9 = nullptr;
    static CreateDevice_t    oCreateDevice    = nullptr;
    static EndScene_t        oEndScene        = nullptr;
    static Reset_t           oReset           = nullptr;
    static SetTexture_t      oSetTexture      = nullptr;

    static bool m_bInitialized = false;

    static IDirect3DTexture9* m_SplashTexture = nullptr;
    static bool m_bTextureLoaded = false;

    static int* pGameState = (int*)0xC8D4C0;

    IDirect3D9* WINAPI Hook_Direct3DCreate9(UINT SDKVersion);
    HRESULT WINAPI Hook_CreateDevice(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface);
    HRESULT WINAPI Hook_EndScene(IDirect3DDevice9* pDevice);
    HRESULT WINAPI Hook_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);
    HRESULT WINAPI Hook_SetTexture(IDirect3DDevice9* pDevice, DWORD Stage, IDirect3DBaseTexture9* pTexture);

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
        
        DetourTransactionCommit();
        
        if (m_SplashTexture)
        {
            m_SplashTexture->Release();
            m_SplashTexture = nullptr;
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

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&)oEndScene, Hook_EndScene);
            DetourAttach(&(PVOID&)oReset, Hook_Reset);
            DetourAttach(&(PVOID&)oSetTexture, Hook_SetTexture);
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
        if (m_SplashTexture)
        {
            m_SplashTexture->Release();
            m_SplashTexture = nullptr;
            m_bTextureLoaded = false;
        }
        return oReset(pDevice, pPresentationParameters);
    }

    HRESULT WINAPI Hook_SetTexture(IDirect3DDevice9* pDevice, DWORD Stage, IDirect3DBaseTexture9* pTexture)
    {
        if (!m_bTextureLoaded)
        {
            m_SplashTexture  = TextureLoader::LoadTexture(pDevice, "loading_screen.png");
            m_bTextureLoaded = true; 
        }

        if (m_SplashTexture && Stage == 0 && pTexture)
        {
            static DWORD fvf;
            if (SUCCEEDED(pDevice->GetFVF(&fvf)) && (fvf & D3DFVF_XYZRHW))
            {
                int state = -1;
                try { if (pGameState) state = *pGameState; } catch(...) {}
                
                if (state >= 0 && state <= 8)
                {
                    IDirect3DTexture9* pTex2D = static_cast<IDirect3DTexture9*>(pTexture);
                    D3DSURFACE_DESC desc;
                    
                    if (pTexture->GetType() == D3DRTYPE_TEXTURE && SUCCEEDED(pTex2D->GetLevelDesc(0, &desc)))
                    {
                        if (desc.Width > 256 && desc.Height > 256)
                        {
                             return oSetTexture(pDevice, Stage, m_SplashTexture);
                        }
                    }
                }
            }
        }
        return oSetTexture(pDevice, Stage, pTexture);
    }
}

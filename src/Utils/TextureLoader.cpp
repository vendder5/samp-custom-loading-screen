#include "TextureLoader.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <windows.h>
#include <wininet.h>
#include <vector>
#include <iostream>

#pragma comment(lib, "wininet.lib")

namespace TextureLoader
{
    IDirect3DTexture9* CreateTextureFromData(IDirect3DDevice9* pDevice, unsigned char* data, int width, int height)
    {
        if (!data) return nullptr;

        IDirect3DTexture9* pSysTexture = nullptr;
        if (FAILED(pDevice->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pSysTexture, nullptr)))
        {
            return nullptr;
        }

        D3DLOCKED_RECT rect;
        if (SUCCEEDED(pSysTexture->LockRect(0, &rect, nullptr, 0)))
        {
            unsigned char* dest = static_cast<unsigned char*>(rect.pBits);
            
            for (int y = 0; y < height; ++y)
            {
                unsigned char* srcRow = data + (y * width * 4);
                unsigned char* destRow = dest + (y * rect.Pitch);
                
                for (int x = 0; x < width; ++x)
                {
                    destRow[x * 4 + 0] = srcRow[x * 4 + 2];
                    destRow[x * 4 + 1] = srcRow[x * 4 + 1];
                    destRow[x * 4 + 2] = srcRow[x * 4 + 0];
                    destRow[x * 4 + 3] = srcRow[x * 4 + 3];
                }
            }
            pSysTexture->UnlockRect(0);
        }

        IDirect3DTexture9* pVidTexture = nullptr;
        if (FAILED(pDevice->CreateTexture(width, height, 0, D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pVidTexture, nullptr)))
        {
            pSysTexture->Release();
            return nullptr;
        }

        pDevice->UpdateTexture(pSysTexture, pVidTexture);
        pSysTexture->Release();

        return pVidTexture;
    }

    IDirect3DTexture9* LoadTexture(IDirect3DDevice9* pDevice, const std::string& filename)
    {
        int width, height, channels;
        unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 4);

        if (!data)
            return nullptr;

        IDirect3DTexture9* texture = CreateTextureFromData(pDevice, data, width, height);
        stbi_image_free(data);
        return texture;
    }

    IDirect3DTexture9* LoadTextureFromURL(IDirect3DDevice9* pDevice, const std::string& url)
    {
        HINTERNET hInternet = InternetOpenA("Loadscs/1.0", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
        if (!hInternet)
            return nullptr;

        HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), nullptr, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
        if (!hUrl)
        {
            InternetCloseHandle(hInternet);
            return nullptr;
        }

        std::vector<unsigned char> buffer;
        DWORD bytesRead = 0;
        constexpr DWORD chunkSize = 4096;
        char chunk[chunkSize];

        while (InternetReadFile(hUrl, chunk, chunkSize, &bytesRead) && bytesRead > 0)
        {
            buffer.insert(buffer.end(), chunk, chunk + bytesRead);
        }

        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);

        if (buffer.empty())
            return nullptr;

        int width, height, channels;
        unsigned char* data = stbi_load_from_memory(buffer.data(), (int)buffer.size(), &width, &height, &channels, 4);

        if (!data)
            return nullptr;

        IDirect3DTexture9* texture = CreateTextureFromData(pDevice, data, width, height);
        stbi_image_free(data);
        return texture;
    }
}
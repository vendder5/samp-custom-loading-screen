#include "TextureLoader.h"
#include "DataLoader.h"
#include "SpriteImpl.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <vector>

namespace TextureLoader
{
    IDirect3DTexture9* CreateTextureFromRGBA(IDirect3DDevice9* pDevice, unsigned char* data, int width, int height)
    {
        if (!data)
            return nullptr;

        IDirect3DTexture9* pSysTexture = nullptr;
        if (FAILED(pDevice->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pSysTexture, nullptr)))
            return nullptr;

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

    ISprite* LoadSprite(IDirect3DDevice9* pDevice, const std::string& source, bool isUrl)
    {
        std::vector<unsigned char> data;
        if (isUrl)
            data = DataLoader::DownloadFromURL(source);
        else
            data = DataLoader::LoadFromFile(source);

        if (data.empty())
            return nullptr;

        int *delays = nullptr;
        int x, y, z, comp;
        unsigned char* gif_data = stbi_load_gif_from_memory(data.data(), (int)data.size(), &delays, &x, &y, &z, &comp, 4);

        if (gif_data)
        {
            std::vector<GifSprite::Frame> frames;
            int stride = x * y * 4;

            for (int i = 0; i < z; ++i)
            {
                unsigned char* framePixelData = gif_data + (i * stride);
                IDirect3DTexture9* tex = CreateTextureFromRGBA(pDevice, framePixelData, x, y);
                if (tex)
                {
                    frames.push_back({ tex, delays[i] });
                }
            }

            stbi_image_free(gif_data);
            stbi_image_free(delays);

            if (frames.empty()) return nullptr;
            return new GifSprite(frames);
        }
        else
        {
            int width, height, channels;
            unsigned char* static_data = stbi_load_from_memory(data.data(), (int)data.size(), &width, &height, &channels, 4);
            
            if (static_data)
            {
                IDirect3DTexture9* tex = CreateTextureFromRGBA(pDevice, static_data, width, height);
                stbi_image_free(static_data);
                
                if (tex) return new StaticSprite(tex);
            }
        }

        return nullptr;
    }
}
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

    DecodedSpriteData* DecodeSpriteFromMemory(const std::vector<unsigned char>& fileData)
    {
        if (fileData.empty())
            return nullptr;

        int* delays = nullptr;
        int x = 0, y = 0, z = 0, comp = 0;
        unsigned char* gif_data = stbi_load_gif_from_memory(fileData.data(), (int)fileData.size(), &delays, &x, &y, &z, &comp, 4);

        if (gif_data)
        {
            DecodedSpriteData* spriteData = new DecodedSpriteData();
            spriteData->width = x;
            spriteData->height = y;
            spriteData->isGif = true;

            int stride = x * y * 4;
            for (int i = 0; i < z; ++i)
            {
                unsigned char* framePixelData = gif_data + (i * stride);
                DecodedFrame frame;
                frame.rgbaData.assign(framePixelData, framePixelData + stride);
                frame.delayMs = delays[i];
                spriteData->frames.push_back(std::move(frame));
            }

            stbi_image_free(gif_data);
            stbi_image_free(delays);

            if (spriteData->frames.empty())
            {
                delete spriteData;
                return nullptr;
            }
            return spriteData;
        }
        else
        {
            int width = 0, height = 0, channels = 0;
            unsigned char* static_data = stbi_load_from_memory(fileData.data(), (int)fileData.size(), &width, &height, &channels, 4);
            
            if (static_data)
            {
                DecodedSpriteData* spriteData = new DecodedSpriteData();
                spriteData->width = width;
                spriteData->height = height;
                spriteData->isGif = false;

                DecodedFrame frame;
                frame.rgbaData.assign(static_data, static_data + (width * height * 4));
                frame.delayMs = 0;
                spriteData->frames.push_back(std::move(frame));

                stbi_image_free(static_data);
                return spriteData;
            }
        }

        return nullptr;
    }

    ISprite* CreateSpriteFromDecodedData(IDirect3DDevice9* pDevice, DecodedSpriteData* decodedData)
    {
        if (!decodedData || decodedData->frames.empty())
            return nullptr;

        if (decodedData->isGif)
        {
            std::vector<GifSprite::Frame> frames;
            for (auto& f : decodedData->frames)
            {
                IDirect3DTexture9* tex = CreateTextureFromRGBA(pDevice, const_cast<unsigned char*>(f.rgbaData.data()), decodedData->width, decodedData->height);
                if (tex)
                {
                    frames.push_back({ tex, f.delayMs });
                }
            }

            if (frames.empty()) return nullptr;
            return new GifSprite(frames, decodedData->width, decodedData->height);
        }
        else
        {
            auto& f = decodedData->frames[0];
            IDirect3DTexture9* tex = CreateTextureFromRGBA(pDevice, const_cast<unsigned char*>(f.rgbaData.data()), decodedData->width, decodedData->height);
            if (tex)
            {
                return new StaticSprite(tex, decodedData->width, decodedData->height);
            }
        }

        return nullptr;
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

        DecodedSpriteData* decoded = DecodeSpriteFromMemory(data);
        if (!decoded)
            return nullptr;

        ISprite* sprite = CreateSpriteFromDecodedData(pDevice, decoded);
        delete decoded;
        return sprite;
    }
}
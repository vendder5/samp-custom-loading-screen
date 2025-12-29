#include "TextureLoader.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace TextureLoader
{
    IDirect3DTexture9* LoadTexture(IDirect3DDevice9* pDevice, const std::string& filename)
    {
        int width, height, channels;
        unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 4);

        if (!data)
            return nullptr;

        IDirect3DTexture9* pSysTexture = nullptr;
        if (FAILED(pDevice->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pSysTexture, nullptr)))
        {
            stbi_image_free(data);
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
        stbi_image_free(data);

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

}

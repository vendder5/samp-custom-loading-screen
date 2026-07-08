#pragma once
#include <d3d9.h>
#include <string>
#include <vector>
#include "ISprite.h"

namespace TextureLoader
{
    struct DecodedFrame
    {
        std::vector<unsigned char> rgbaData;
        int delayMs;
    };

    struct DecodedSpriteData
    {
        std::vector<DecodedFrame> frames;
        int width;
        int height;
        bool isGif;
    };

    DecodedSpriteData* DecodeSpriteFromMemory(const std::vector<unsigned char>& fileData);
    ISprite* CreateSpriteFromDecodedData(IDirect3DDevice9* pDevice, DecodedSpriteData* decodedData);
    ISprite* LoadSprite(IDirect3DDevice9* pDevice, const std::string& source, bool isUrl);
}

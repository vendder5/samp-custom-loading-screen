#pragma once
#include <d3d9.h>
#include <string>
#include "ISprite.h"

namespace TextureLoader
{
    ISprite* LoadSprite(IDirect3DDevice9* pDevice, const std::string& source, bool isUrl);
}

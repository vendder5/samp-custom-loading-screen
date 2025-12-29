#pragma once

#include <d3d9.h>
#include <string>

namespace TextureLoader
{
    IDirect3DTexture9* LoadTexture(IDirect3DDevice9* pDevice, const std::string& filename);
    IDirect3DTexture9* LoadTextureFromURL(IDirect3DDevice9* pDevice, const std::string& url);
}

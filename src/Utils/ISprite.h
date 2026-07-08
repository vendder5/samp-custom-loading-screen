#pragma once

#include <d3d9.h>

class ISprite
{
public:
    virtual ~ISprite() = default;
    virtual void Update() = 0;
    virtual IDirect3DTexture9* GetTexture() = 0;
    virtual int GetWidth() = 0;
    virtual int GetHeight() = 0;
};

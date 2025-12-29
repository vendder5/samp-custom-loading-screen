#pragma once
#include "ISprite.h"
#include <vector>
#include <windows.h>
#include <chrono>

class StaticSprite : public ISprite
{
    IDirect3DTexture9* m_pTexture;
public:
    StaticSprite(IDirect3DTexture9* pTex) : m_pTexture(pTex) {}
    ~StaticSprite() { if (m_pTexture) m_pTexture->Release(); }

    void Update() override {}
    IDirect3DTexture9* GetTexture() override { return m_pTexture; }
};

class GifSprite : public ISprite
{
public:
    struct Frame
    {
        IDirect3DTexture9* texture;
        int delayMs;
    };
private:
    std::vector<Frame> m_Frames;
    int m_CurrentFrameIndex;
    
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point m_LastTime;
    double m_TimeAccumulator;

public:
    GifSprite(const std::vector<Frame>& frames) 
        : m_Frames(frames), m_CurrentFrameIndex(0), m_TimeAccumulator(0.0) 
    {
        m_LastTime = Clock::now();

        for (auto& f : m_Frames)
        {
            if (f.delayMs <= 10) f.delayMs = 100;
        }
    }

    ~GifSprite()
    {
        for (auto& f : m_Frames)
            if (f.texture) f.texture->Release();
    }

    void Update() override
    {
        if (m_Frames.empty()) return;

        auto now = Clock::now();
        std::chrono::duration<double, std::milli> diff = now - m_LastTime;
        m_LastTime = now;

        m_TimeAccumulator += diff.count();

        while (true)
        {
            int currentDelay = m_Frames[m_CurrentFrameIndex].delayMs;
            
            if (m_TimeAccumulator >= currentDelay)
            {
                m_TimeAccumulator -= currentDelay;
                m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % m_Frames.size();
            }
            else
            {
                break;
            }
        }
    }

    IDirect3DTexture9* GetTexture() override
    {
        if (m_Frames.empty()) return nullptr;
        return m_Frames[m_CurrentFrameIndex].texture;
    }
};

class VideoSprite : public ISprite
{
public:
    void Update() override {}
    IDirect3DTexture9* GetTexture() override { return nullptr; }
};

class LottieSprite : public ISprite
{
public:
    void Update() override {}
    IDirect3DTexture9* GetTexture() override { return nullptr; }
};

#pragma once
#include "ISprite.h"
#include <vector>
#include <windows.h>
#include <chrono>

class StaticSprite : public ISprite
{
    IDirect3DTexture9* m_pTexture;
    int m_Width;
    int m_Height;
public:
    StaticSprite(IDirect3DTexture9* pTex, int w, int h) : m_pTexture(pTex), m_Width(w), m_Height(h) {}
    ~StaticSprite() { if (m_pTexture) m_pTexture->Release(); }

    void Update() override {}
    IDirect3DTexture9* GetTexture() override { return m_pTexture; }
    int GetWidth() override { return m_Width; }
    int GetHeight() override { return m_Height; }
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
    int m_Width;
    int m_Height;
    
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point m_LastTime;
    double m_TimeAccumulator;

public:
    GifSprite(const std::vector<Frame>& frames, int w, int h) 
        : m_Frames(frames), m_CurrentFrameIndex(0), m_Width(w), m_Height(h), m_TimeAccumulator(0.0) 
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

    int GetWidth() override { return m_Width; }
    int GetHeight() override { return m_Height; }
};

class VideoSprite : public ISprite
{
public:
    void Update() override {}
    IDirect3DTexture9* GetTexture() override { return nullptr; }
    int GetWidth() override { return 0; }
    int GetHeight() override { return 0; }
};

class LottieSprite : public ISprite
{
public:
    void Update() override {}
    IDirect3DTexture9* GetTexture() override { return nullptr; }
    int GetWidth() override { return 0; }
    int GetHeight() override { return 0; }
};

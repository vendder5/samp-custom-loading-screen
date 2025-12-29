#include "Plugin.h"
#include <thread>
#include "../Hooks/D3D9Hook.h"

namespace Plugin
{
    void Init()
    {
        D3D9Hook::Install();
    }

    void OnAttach()
    {
        std::thread([]()
        {
            Init();
        }).detach();
    }

    void OnDetach()
    {
        D3D9Hook::Uninstall();
    }
}
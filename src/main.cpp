#include <Windows.h>
#include "Core/Plugin.h"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            Plugin::OnAttach();
            break;

        case DLL_PROCESS_DETACH:
            Plugin::OnDetach();
            break;
    }
    
    return TRUE;
}

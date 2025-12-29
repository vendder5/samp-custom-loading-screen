#include "DataLoader.h"
#include <windows.h>
#include <urlmon.h>
#include <fstream>

#pragma comment(lib, "urlmon.lib")

namespace DataLoader
{
    std::vector<unsigned char> DownloadFromURL(const std::string& url)
    {
        IStream* pStream = nullptr;
        HRESULT hr = URLOpenBlockingStreamA(nullptr, url.c_str(), &pStream, 0, nullptr);
        
        if (FAILED(hr) || !pStream)
            return {};

        std::vector<unsigned char> buffer;
        const ULONG chunkSize = 4096;
        char temp[chunkSize];
        ULONG bytesRead = 0;

        do
        {
            bytesRead = 0;
            hr = pStream->Read(temp, chunkSize, &bytesRead);
            if (bytesRead > 0)
            {
                buffer.insert(buffer.end(), temp, temp + bytesRead);
            }
        } while (SUCCEEDED(hr) && bytesRead > 0);

        pStream->Release();
        return buffer;
    }

    std::vector<unsigned char> LoadFromFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
            return {};

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (size <= 0)
            return {};

        std::vector<unsigned char> buffer(size);
        if (file.read((char*)buffer.data(), size))
        {
            return buffer;
        }

        return {};
    }
}

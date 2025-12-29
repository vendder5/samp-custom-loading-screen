#pragma once

#include <string>
#include <vector>

namespace DataLoader
{
    std::vector<unsigned char> DownloadFromURL(const std::string& url);
    std::vector<unsigned char> LoadFromFile(const std::string& path);
}

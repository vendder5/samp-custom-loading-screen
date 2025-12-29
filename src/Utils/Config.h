#pragma once

#include <string>

namespace Config
{
    std::string GetConfigValue(const std::string& filename, const std::string& key);
}

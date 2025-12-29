#include "Config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Config
{
    std::string Trim(const std::string& str)
    {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (std::string::npos == first)
            return "";

        size_t last = str.find_last_not_of(" \t\r\n");
        return
            str.substr(first, (last - first + 1));
    }

    std::string GetConfigValue(const std::string& filename, const std::string& key)
    {
        std::ifstream file(filename);
        if (!file.is_open())
            return "";

        std::string line;
        while (std::getline(file, line))
        {
            size_t delimiterPos = line.find('=');
            if (delimiterPos != std::string::npos)
            {
                std::string currentKey = Trim(line.substr(0, delimiterPos));
                if (currentKey == key)
                {
                    return Trim(line.substr(delimiterPos + 1));
                }
            }
        }
        return "";
    }
}

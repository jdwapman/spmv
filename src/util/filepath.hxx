#pragma once

#include <string>

namespace util {

std::string extract_filename(std::string path, 
                             std::string delim = "/")
{
  size_t lastSlashIndex = path.find_last_of("/");
  return path.substr(lastSlashIndex + 1);
}

std::string extract_dataset(std::string filename)
{
    size_t lastindex = filename.find_last_of(".");
    return filename.substr(0, lastindex);
}

}   // namespace util
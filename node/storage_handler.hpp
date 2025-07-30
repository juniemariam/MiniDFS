#pragma once
#include <string>

class StorageHandler {
public:
    bool store_chunk(const std::string& filename, const std::string& chunk);
    std::string retrieve_chunk(const std::string& filename);
};

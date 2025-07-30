#pragma once
#include <map>
#include <string>
#include <vector>

class MetadataStore {
public:
    void add_file(const std::string& filename, const std::vector<std::string>& chunks);
    std::vector<std::string> get_chunks(const std::string& filename);
private:
    std::map<std::string, std::vector<std::string>> file_chunks_;
};

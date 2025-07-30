#include <iostream>
#include <string>

void upload(const std::string& filename) {
    std::cout << "Uploading " << filename << std::endl;
}

void download(const std::string& filename) {
    std::cout << "Downloading " << filename << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: ./client <upload/download> <filename>\n";
        return 1;
    }

    std::string command = argv[1];
    std::string filename = argv[2];

    if (command == "upload") upload(filename);
    else if (command == "download") download(filename);
    else std::cerr << "Invalid command\n";

    return 0;
}

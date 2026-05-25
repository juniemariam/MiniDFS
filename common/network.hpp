#pragma once

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace net {

inline bool send_all(int socket_fd, const char* data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        ssize_t n = ::send(socket_fd, data + sent, length - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

inline bool recv_all(int socket_fd, char* data, size_t length) {
    size_t received = 0;
    while (received < length) {
        ssize_t n = ::recv(socket_fd, data + received, length - received, 0);
        if (n <= 0) {
            return false;
        }
        received += static_cast<size_t>(n);
    }
    return true;
}

inline bool write_line(int socket_fd, const std::string& line) {
    std::string payload = line + "\n";
    return send_all(socket_fd, payload.data(), payload.size());
}

inline bool read_line(int socket_fd, std::string& line) {
    line.clear();
    char c = '\0';
    while (true) {
        ssize_t n = ::recv(socket_fd, &c, 1, 0);
        if (n <= 0) {
            return false;
        }
        if (c == '\n') {
            return true;
        }
        if (c != '\r') {
            line.push_back(c);
        }
    }
}

inline int connect_to(const std::string& host, int port) {
    int socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        return -1;
    }

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port));
    if (::inet_pton(AF_INET, host.c_str(), &address.sin_addr) <= 0) {
        ::close(socket_fd);
        return -1;
    }

    if (::connect(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        ::close(socket_fd);
        return -1;
    }
    return socket_fd;
}

inline int create_server_socket(int port) {
    int socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        throw std::runtime_error("socket failed: " + std::string(std::strerror(errno)));
    }

    int reuse = 1;
    ::setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        ::close(socket_fd);
        throw std::runtime_error("bind failed on port " + std::to_string(port) + ": " +
                                 std::string(std::strerror(errno)));
    }
    if (::listen(socket_fd, 64) < 0) {
        ::close(socket_fd);
        throw std::runtime_error("listen failed: " + std::string(std::strerror(errno)));
    }
    return socket_fd;
}

inline void close_socket(int socket_fd) {
    if (socket_fd >= 0) {
        ::close(socket_fd);
    }
}

}  // namespace net


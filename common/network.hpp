#pragma once

int send_data(int socket, const char* data, size_t length);
int receive_data(int socket, char* buffer, size_t length);

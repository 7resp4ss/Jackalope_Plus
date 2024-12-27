#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <string>
#include <cstring>
#include <stdexcept>

#pragma comment(lib, "ws2_32.lib")  // Linking to the Windows socket library

void send_file_data(const std::string& file_path, const std::string& server_ip, int server_port) {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed!" << std::endl;
        return;
    }

    // Create a socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed!" << std::endl;
        WSACleanup();
        return;
    }

    // Define server address
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());

    // Connect to server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed!" << std::endl;
        closesocket(sock);
        WSACleanup();
        return;
    }

    // Read file contents
    std::ifstream file(file_path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        closesocket(sock);
        WSACleanup();
        return;
    }

    // Read file into buffer
    std::string file_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Send the file content to the server
    int bytes_sent = send(sock, file_data.c_str(), file_data.size(), 0);
    if (bytes_sent == SOCKET_ERROR) {
        std::cerr << "Failed to send data to the server!" << std::endl;
    } else {
        std::cout << "Sent " << bytes_sent << " bytes to the server." << std::endl;
    }

    // Close the socket and clean up Winsock
    closesocket(sock);
    WSACleanup();
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <file_path> <server_ip> <server_port>" << std::endl;
        return 1;
    }

    std::string file_path = argv[1];
    std::string server_ip = argv[2];
    int server_port = std::stoi(argv[3]);

    // Send file data to the server
    send_file_data(file_path, server_ip, server_port);

    return 0;
}

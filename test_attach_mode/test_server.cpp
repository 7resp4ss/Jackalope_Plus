#include <iostream>
#include <string>
#include <winsock2.h>  // Using Windows socket library
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>  // For std::min

void process_data(const char* data, int read_size) {
    // Branch 1: Data starts with "s"
    if (data[0] == 's') {
        if (data[1] == 'p') {
            // Simulating memory copy
            char buffer[0x20];
            // Ensure no overflow
            memcpy(buffer, data, read_size);
            printf("Data processed in 'sp' branch: %s\n", buffer);
        } else {
            printf("Nothing here for 's' branch!\n");
        }
    }
    // Branch 2: Data starts with "e" and "r"
    else if (data[0] == 'e' && data[1] == 'x' && data[read_size-2] == 'i' && data[read_size-1] == 't' ) {
        printf("Simulating error condition...\n");
        exit(0);
    }
    // Branch 3: Data starts with "uaf"
    else if (data[0] == 'u' && data[1] == 'a' && data[2] == 'f') {
        printf("Simulating Use After Free (UAF)...\n");
        char* p = (char*)malloc(0x10);  // Allocate memory
        if (p == NULL) {
            printf("Memory allocation failed!\n");
            return;
        }
        free(p);  // Free memory
        p[0] = 1;  // Access freed memory, simulating UAF error
    }
    // Branch 4: Data starts with "o", simulating buffer overflow
    else if (data[0] == 'o') {
        char buffer[16];
        printf("Processing overflow branch...\n");
        memcpy(buffer, data, read_size);  // Potential buffer overflow
        printf("Overflow branch: %s\n", buffer);
    }
    // Branch 5: Data starts with "m", simulating heap overflow
    else if (data[0] == 'm') {
        printf("Simulating heap overflow...\n");
        char* heap_buffer = (char*)malloc(1024);  // Allocate heap memory
        if (heap_buffer == NULL) {
            printf("Heap allocation failed!\n");
            return;
        }
        memset(heap_buffer, 'A', 1024);  // Fill heap memory
        free(heap_buffer);  // Free heap memory
        heap_buffer[0] = 'B';  // Access freed heap memory, simulating heap overflow
    }
    // Branch 6: Data starts with "b", simulating stack overflow
    else if (data[0] == 'b') {
        printf("Simulating stack overflow...\n");
        if (data[1] == 'b') {
            printf("aaaaaa");
        }
    }
    // Branch 7: Data starts with "f", simulating file operation error
    else if (data[0] == 'f') {
        if (data[1] == 'b') {
            printf("aaaaaa");
        }
    }
    // Branch 8: Data starts with "v", simulating integer overflow
    else if (data[0] == 'v') {
        printf("Simulating integer overflow...\n");
        int a = INT_MAX;
        int b = 1;
        int result = a + b;  // Integer overflow
        printf("Overflow result: %d\n", result);
    }
    // Default branch
    else {
        printf("Unknown data: %s\n", data);
    }
}

void handle_client(SOCKET client_socket) {
    char buffer[1024];
    int read_size = recv(client_socket, buffer, sizeof(buffer), 0);

    if (read_size == SOCKET_ERROR) {
        perror("recv failed");
        return;
    }

    buffer[read_size] = '\0';
    std::cout << "Received from client: " << buffer << std::endl;

    try {
        process_data(buffer, read_size);
    } catch (const std::exception& ex) {
        std::cerr << "Error processing data: " << ex.what() << std::endl;
        send(client_socket, "Server encountered an error", 26, 0);
        closesocket(client_socket);  // Use closesocket
        return;
    }

    // Respond to the client
    std::string response = "Server successfully processed: " + std::string(buffer);
    send(client_socket, response.c_str(), response.size(), 0);

    closesocket(client_socket);  // Use closesocket
}

int main() {
    WSADATA wsaData;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int client_len = sizeof(client_addr);  // Fix: Declare client_len

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed!" << std::endl;
        return 1;
    }

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed!" << std::endl;
        WSACleanup();
        return 1;
    }

    // Setup server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed!" << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 3) == SOCKET_ERROR) {
        std::cerr << "Listen failed!" << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port 8080..." << std::endl;

    // Accept incoming client connections
    client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Accept failed!" << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Client connected." << std::endl;

    // Handle client request
    handle_client(client_socket);

    // Cleanup
    closesocket(server_socket);
    WSACleanup();
    return 0;
}

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>

// Link with the Ws2_32.lib library
#pragma comment(lib, "Ws2_32.lib")

int main(int argc, char* argv[]) {
    // Check if file path is provided
    if (argc < 2) {
        printf("Usage: %s <file_path>\n", argv[0]);
        return 1;
    }

    const char* filePath = argv[1];
    FILE* file = fopen(filePath, "r");
    if (file == NULL) {
        printf("Failed to open file: %s\n", filePath);
        return 1;
    }

    // Determine file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    if (fileSize < 0) {
        printf("Failed to determine file size.\n");
        fclose(file);
        return 1;
    }
    fseek(file, 0, SEEK_SET);

    // Allocate buffer and read file content
    char* buffer = (char*)malloc(fileSize);
    if (buffer == NULL) {
        printf("Memory allocation failed.\n");
        fclose(file);
        return 1;
    }

    size_t bytesRead = fread(buffer, 1, fileSize, file);

    fclose(file);
    printf("Reading %s\n",buffer);

    // Initialize Winsock
    WSADATA wsaData;
    int wsaInit = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaInit != 0) {
        printf("WSAStartup failed with error: %d\n", wsaInit);
        free(buffer);
        return 1;
    }

    // Create a socket
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        printf("Socket creation failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        free(buffer);
        return 1;
    }

    // Setup server address structure
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888); // Server port
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Server IP address

    // Connect to the server
    int connectResult = connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (connectResult == SOCKET_ERROR) {
        printf("Connection to server failed with error: %ld\n", WSAGetLastError());
        closesocket(clientSocket);
        WSACleanup();
        free(buffer);
        return 1;
    }

    // Send the data
    int totalBytesSent = 0;
    while (totalBytesSent < fileSize) {
        int bytesToSend = (fileSize - totalBytesSent) > 1024 ? 1024 : (fileSize - totalBytesSent);
        int sendResult = send(clientSocket, buffer + totalBytesSent, bytesToSend, 0);
        if (sendResult == SOCKET_ERROR) {
            printf("Failed to send data with error: %ld\n", WSAGetLastError());
            closesocket(clientSocket);
            WSACleanup();
            free(buffer);
            return 1;
        }
        totalBytesSent += sendResult;
    }

    printf("File sent successfully, total bytes sent: %d\n", totalBytesSent);

    // Clean up
    closesocket(clientSocket);
    WSACleanup();
    free(buffer);

    return 0;
}

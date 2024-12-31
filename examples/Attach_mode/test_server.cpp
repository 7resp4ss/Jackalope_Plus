#include<winsock2.h>
#include<iostream>
#include<string>
using namespace std;
#pragma comment(lib, "ws2_32.lib")

bool process_data(const char* data, int read_size) {
    // Branch 1: Data starts with "s"
    if (data[0] == 's') {
        if (data[1] == 'p') {
            // Simulating memory copy
            char buffer[0x20];
            // Ensure no overflow
            memcpy(buffer, data, read_size);
            printf("Data processed in 'sp' branch: %s\n", buffer);
            return 0;
        } else {
            printf("Nothing here for 's' branch!\n");
            return 0;
        }
    }
    // Branch 2: Data starts with "e" and "r"
    else if (data[0] == 'e' && data[1] == 'x' && data[read_size-2] == 'i' && data[read_size-1] == 't' ) {
        printf("Simulating error condition...\n");
        return 0;
    }
    // Branch 3: Data starts with "uaf"
    else if (data[0] == 'u' && data[1] == 'a' && data[2] == 'f') {
        printf("Simulating Use After Free (UAF)...\n");
        char* p = (char*)malloc(0x10);  // Allocate memory
        if (p == NULL) {
            printf("Memory allocation failed!\n");
            return 0;
        }
        free(p);  // Free memory
        p[0] = 1;  // Access freed memory, simulating UAF error
        return 0;
    }
    // Branch 4: Data starts with "o", simulating buffer overflow
    else if (data[0] == 'o') {
        char buffer[16];
        printf("Processing overflow branch...\n");
        memcpy(buffer, data, read_size);  // Potential buffer overflow
        printf("Overflow branch: %s\n", buffer);
        return 0;
    }
    // Branch 5: Data starts with "m", simulating heap overflow
    else if (data[0] == 'm') {
        printf("Simulating heap overflow...\n");
        char* heap_buffer = (char*)malloc(1024);  // Allocate heap memory
        if (heap_buffer == NULL) {
            printf("Heap allocation failed!\n");
            return 0;
        }
        memset(heap_buffer, 'A', 1024);  // Fill heap memory
        heap_buffer[1025] = 'B';  // Access freed heap memory, simulating heap overflow
        return 0;
    }
    // Branch 6: Data starts with "b", simulating stack overflow
    else if (data[0] == 'b') {
        if (data[1] == 'b') {
            printf("aaaaaa");
            return 0;
        }
    }
    // Branch 7: Data starts with "f", simulating file operation error
    else if (data[0] == 'f') {
        if (data[1] == 'b') {
            printf("aaaaaa");
            return 0;
        }
    }
    // Branch 8: Data starts with "v", simulating integer overflow
    else if (data[0] == 'v') {
        printf("Simulating integer overflow...\n");
        int a = INT_MAX;
        int b = 1;
        int result = a + b;  // Integer overflow
        printf("Overflow result: %d\n", result);
        return 0;
    } if(data[0] == 'e' && data[1] == 'x' && data[2] == 'i' && data[3] == 't') {
        return 1;
    }
    // Default branch
    else {
        printf("Unknown data: %s\n", data);
        return 0;
    }
}

void handle_client(SOCKET client_socket) {
    std::string received_data;

    while (true) {
        char buffer[1024];
        // Leave space for null terminator
        int read_size = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (read_size == SOCKET_ERROR) {
            perror("recv failed");
            break;
        }

        if (read_size == 0) {
            std::cout << "Client has closed the connection." << std::endl;
            break;
        }

        buffer[read_size] = '\0'; // Ensure the string is null-terminated
        received_data += buffer;

        std::cout << "Received from client: " << buffer << std::endl;

        // Check if received_data ends with "exit[]"
        if (received_data.size() >= 6) { // "exit[]" length is 6
            if (received_data.compare(received_data.size() - 6, 6, "exit[]") == 0) {
                std::cout << "Received exit[] command. Closing connection." << std::endl;
                break;
            }
        }

        // Optional: Process data here if needed
        process_data(buffer, read_size);
    }

    closesocket(client_socket); // Close the socket
}

void print_pid() {
    DWORD pid = GetCurrentProcessId();  // Get the current process ID
    printf("Process ID: %lu\n", pid);  // Print the PID
}

int main(int argc, char* argv[]) {
    print_pid();

    // Initialize Winsock
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsdata;
    if (WSAStartup(sockVersion, &wsdata) != 0) {
        cout << "WSAStartup failed" << endl;
        return 1;
    }

    // Create a socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        cout << "Socket error" << endl;
        WSACleanup();
        return 1;
    }

    // Bind the socket
    sockaddr_in sockAddr;
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(8888);
    sockAddr.sin_addr.S_un.S_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr*)&sockAddr, sizeof(sockAddr)) == SOCKET_ERROR) {
        cout << "Bind error" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Start listening
    if (listen(serverSocket, 10) == SOCKET_ERROR) {
        cout << "Listen error" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    SOCKET clientSocket;
    sockaddr_in client_sin;
    char msg[100]; // Buffer to store received message
    int flag = 0;  // Connection status
    int len = sizeof(client_sin);

    while (true) {
        if (!flag)
            cout << "Waiting for connection..." << endl;

        clientSocket = accept(serverSocket, (sockaddr*)&client_sin, &len);
        if (clientSocket == INVALID_SOCKET) {
            cout << "Accept error" << endl;
            flag = 0;
            closesocket(serverSocket);
            WSACleanup();
            return 1;
        }

        if (!flag)
            cout << "Received a connection from: " << inet_ntoa(client_sin.sin_addr) << endl;

        flag = 1;
        int num = recv(clientSocket, msg, sizeof(msg) - 1, 0);
        if (num > 0) {
            msg[num] = '\0';
            cout << "Client says: " << msg << endl;
            process_data(msg, num);
        } else if (num == 0) {
            cout << "Connection closed by client." << endl;
        } else {
            cout << "Receive error" << endl;
        }

        closesocket(clientSocket);
        flag = 0;
    }

    closesocket(serverSocket);
    WSACleanup();

    return 0;
}


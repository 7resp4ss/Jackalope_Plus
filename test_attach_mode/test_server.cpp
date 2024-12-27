#include <iostream>
#include <string>
#include <sstream>
#include <winsock2.h>  // 使用 Windows 套接字库
#include <cstring>
#include <cstdlib>
#include <stdexcept>

void process_data(const char* data, int read_size) {
    // 分支 1: 数据以 "s" 开头
    if (data[0] == 's') {
        if (data[1] == 'p') {
            // 模拟内存复制
            char buffer[0x20];
            memcpy(buffer, data, read_size);
            printf("Data processed in 'sp' branch: %s\n", buffer);
        } else {
            printf("Nothing here for 's' branch!\n");
        }
    }
    // 分支 2: 数据以 "e" 和 "r" 开头
    else if (data[0] == 'e' && data[1] == 'x' && data[read_size-2] == 'i' && data[read_size-1] == 't' ) {
        printf("Simulating error condition...\n");
        exit(0);
    }
    // 分支 3: 数据以 "uaf" 开头
    else if (data[0] == 'u' && data[1] == 'a' && data[2] == 'f') {
        printf("Simulating Use After Free (UAF)...\n");
        char* p = (char*)malloc(0x10);  // 分配内存
        if (p == NULL) {
            printf("Memory allocation failed!\n");
            return;
        }
        free(p);  // 释放内存
        p[0] = 1;  // 访问已经释放的内存，模拟UAF错误
    }
    // 分支 4: 数据以 "o" 开头，模拟缓冲区溢出
    else if (data[0] == 'o') {
        char buffer[16];
        printf("Processing overflow branch...\n");
        memcpy(buffer, data, read_size);  // 可能会造成缓冲区溢出
        printf("Overflow branch: %s\n", buffer);
    }
    // 分支 5: 数据以 "m" 开头，模拟堆溢出
    else if (data[0] == 'm') {
        printf("Simulating heap overflow...\n");
        char* heap_buffer = (char*)malloc(1024);  // 分配堆内存
        if (heap_buffer == NULL) {
            printf("Heap allocation failed!\n");
            return;
        }
        memset(heap_buffer, 'A', 1024);  // 填充堆内存
        free(heap_buffer);  // 释放堆内存
        heap_buffer[0] = 'B';  // 访问已释放的堆内存，模拟堆溢出
    }
    // 分支 6: 数据以 "b" 开头，模拟递归导致栈溢出
    else if (data[0] == 'b') {
        printf("Simulating stack overflow...\n");
        if (data[1] == 'b') {
            printf("aaaaaa");
        }
    }
    // 分支 7: 数据以 "f" 开头，模拟文件操作异常
    else if (data[0] == 'f') {
        if (data[1] == 'b') {
            printf("aaaaaa");
        }
    }
    // 分支 8: 数据以 "v" 开头，模拟整数溢出
    else if (data[0] == 'v') {
        printf("Simulating integer overflow...\n");
        int a = INT_MAX;
        int b = 1;
        int result = a + b;  // 整数溢出
        printf("Overflow result: %d\n", result);
    }
    // 默认分支
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
        closesocket(client_socket);  // 使用 closesocket
        return;
    }

    // Responding to client
    std::string response = "Server successfully processed: " + std::string(buffer);
    send(client_socket, response.c_str(), response.size(), 0);

    closesocket(client_socket);  // 使用 closesocket
}

int main() {
    WSADATA wsaData;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // 初始化 WinSock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    // 创建服务器套接字
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // 绑定套接字
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // 监听客户端连接
    if (listen(server_socket, 5) == SOCKET_ERROR) {
        std::cerr << "Listen failed." << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is waiting for incoming connections..." << std::endl;

    while (true) {
        // 接受客户端连接
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Accept failed." << std::endl;
            continue;
        }

        std::cout << "Client connected." << std::endl;
        handle_client(client_socket);  // 处理客户端请求
    }

    // 关闭服务器套接字
    closesocket(server_socket);
    WSACleanup();
    return 0;
}

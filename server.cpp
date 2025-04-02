#include <arpa/inet.h>   // for htons, htonl
#include <netinet/in.h>  // for sockaddr_in
#include <sys/socket.h>  // for socket, bind, listen, accept, send, recv
#include <unistd.h>      // for close, read
#include <stdlib.h>      // for exit
#include <stdio.h>
#include <string.h>      // for memset, strlen


void die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    // Create the socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        die("socket()");
    }

    // reuse port everytime we compile
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        die("setsockopt()");
    }

    // setup server
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(1234);  // Port 1234
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all interfaces (0.0.0.0)

    // bind socket to address and port
    if (bind(socket_fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) != 0) {
        die("bind()");
    }

    // start listening
    if (listen(socket_fd, 10) < 0) {
        die("listen()");
    }
    printf("Server listening on port 1234...\n");

    // handle incoming connections
    while (true) {
        struct sockaddr_in clientAddress;
        socklen_t clientAddrSize = sizeof(clientAddress);
        int connfd = accept(socket_fd, (struct sockaddr *)&clientAddress, &clientAddrSize);
        if (connfd < 0) {
            perror("accept()");
            continue;  // error, try accepting the next connection
        }
        printf("Accepted a connection.\n");

        // process client requests until error
        while (true) {
            char buffer[1024] = {0};
            ssize_t bytesRead = read(connfd, buffer, sizeof(buffer) - 1);
            if (bytesRead < 0) {
                perror("read()");
                break;
            }
            if (bytesRead == 0) {
                printf("Client disconnected.\n");
                break;
            }
            printf("Received: %s\n", buffer);

            // send respose to client.
            const char* response = "Hello from server!\n";
            ssize_t bytesSent = send(connfd, response, strlen(response), 0);
            if (bytesSent < 0) {
                perror("send()");
                break;
            }
        }
        // close current connection before server acceps another one
        close(connfd);
    }

    //c lose the listening socket if loop breaks
    close(socket_fd);
    return 0;
}

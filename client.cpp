#include <arpa/inet.h>   // using inet_pton, htons
#include <netinet/in.h>  // using sockaddr_in
#include <sys/socket.h>  // using socket, connect, send
#include <stdlib.h>      // using exit
#include <stdio.h>
#include <string.h>      // using memset, strlen
#include <unistd.h>      // using close


void die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    // create socket
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        die("socket()");
    }

    // setup server
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(1234);  

    // set server IP address to just be localhost
    if (inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr) <= 0) {
        die("inet_pton()");
    }

    // connect to server
    if (connect(client_fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        die("connect()");
    }

    // send a message to the server.
    const char* msg = "Hello, server!";
    ssize_t bytes_sent = send(client_fd, msg, strlen(msg), 0);
    if (bytes_sent < 0) {
        die("send()");
    }
    printf("Message sent: %s\n", msg);

    // close connection 
    close(client_fd);
    return 0;
}
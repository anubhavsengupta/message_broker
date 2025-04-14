#include <arpa/inet.h>   // using inet_pton, htons
#include <netinet/in.h>  // using sockaddr_in
#include <sys/socket.h>  // using socket, connect, send, recv
#include <unistd.h>      // using close, read
#include <stdlib.h>      // using exit
#include <stdio.h>
#include <string.h>      // using memset, strlen
#include <fcntl.h>       // using fcntl, O_NONBLOCK
#include <poll.h>        // using poll
#include <vector>
#include <iostream>

void die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// helper: set fd to non-blocking mode
static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        die("fcntl(get)");
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0)
        die("fcntl(set)");
}

int main() {
    // create socket
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        die("socket()");
    }
    // setup server address
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(1234);
    // set server ip address to localhost
    if (inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr) <= 0) {
        die("inet_pton()");
    }
    // connect to server
    if (connect(client_fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        die("connect()");
    }
    printf("connected to server\n");
    // set client socket to non-blocking mode
    set_nonblocking(client_fd);

    // for demo, let user type commands (SET/GET) to send to server
    printf("enter commands (e.g., SET key value, GET key):\n");

    // use poll to monitor both client socket (for responses) and stdin (user input)
    std::vector<struct pollfd> pollfds;
    struct pollfd client_pfd;
    client_pfd.fd = client_fd;
    client_pfd.events = POLLIN;
    client_pfd.revents = 0;
    pollfds.push_back(client_pfd);
    struct pollfd stdin_pfd;
    stdin_pfd.fd = 0; // stdin
    stdin_pfd.events = POLLIN;
    stdin_pfd.revents = 0;
    pollfds.push_back(stdin_pfd);

    char sendbuf[1024];
    char recvbuf[1024];

    while (true) {
        // wait for readiness (timeout 5 sec)
        int rv = poll(pollfds.data(), pollfds.size(), 5000);
        if (rv < 0) {
            if (errno == EINTR)
                continue;
            die("poll()");
        }
        if (rv == 0) {
            // no event; just continue polling
            continue;
        }
        // check for user input on stdin
        if (pollfds[1].revents & POLLIN) {
            if (fgets(sendbuf, sizeof(sendbuf), stdin) != NULL) {
                ssize_t bytesSent = send(client_fd, sendbuf, strlen(sendbuf), 0);
                if (bytesSent < 0)
                    perror("send");
            }
        }
        // check if server sent a response
        if (pollfds[0].revents & POLLIN) {
            ssize_t bytesRead = read(client_fd, recvbuf, sizeof(recvbuf)-1);
            if (bytesRead < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    continue;
                die("read");
            } else if (bytesRead == 0) {
                printf("server closed connection\n");
                break;
            } else {
                recvbuf[bytesRead] = '\0';
                printf("received from server: %s", recvbuf);
            }
        }
    }

    close(client_fd);
    return 0;
}

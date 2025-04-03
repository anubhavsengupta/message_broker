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

void die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// helper to set a fd to non-blocking mode
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
    
    // set the client socket to non-blocking mode
    set_nonblocking(client_fd);

    // send a message to the server
    const char* msg = "Hello, server!";
    ssize_t bytes_sent = send(client_fd, msg, strlen(msg), 0);
    if (bytes_sent < 0) {
        die("send()");
    }
    printf("message sent: %s\n", msg);

    // create a vector of pollfd, we'll only have our client fd here
    std::vector<struct pollfd> pollfds;
    struct pollfd pfd;
    pfd.fd = client_fd;
    pfd.events = POLLIN;  // we want to know when data is available to read
    pfd.revents = 0;
    pollfds.push_back(pfd);

    // event loop to wait for a response from the server
    while (true) {
        // wait for readiness (5 second timeout here, adjust as needed)
        int rv = poll(pollfds.data(), pollfds.size(), 5000);
        if (rv < 0) {
            if (errno == EINTR)
                continue;  // interrupted by a signal, retry
            die("poll()");
        }
        if (rv == 0) {
            // timeout occurred, no response received
            printf("no response from server, timing out\n");
            break;
        }
        // check if our client fd is ready to read
        if (pollfds[0].revents & POLLIN) {
            char buffer[1024] = {0};
            ssize_t bytesRead = read(client_fd, buffer, sizeof(buffer) - 1);
            if (bytesRead < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    continue;  // nothing available, try again
                die("read()");
            }
            if (bytesRead == 0) {
                printf("server closed connection\n");
                break;
            }
            buffer[bytesRead] = '\0';
            printf("received from server: %s\n", buffer);
            // assume we exit after receiving the response
            break;
        }
    }

    // close connection
    close(client_fd);
    return 0;
}

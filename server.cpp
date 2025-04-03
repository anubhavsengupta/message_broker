#include <arpa/inet.h>   // for htons, htonl
#include <netinet/in.h>  // for sockaddr_in
#include <sys/socket.h>  // for socket, bind, listen, accept, send, recv
#include <unistd.h>      // for close, read
#include <stdlib.h>      // for exit
#include <stdio.h>
#include <string.h>      // for memset, strlen
#include <fcntl.h>       // for fcntl, O_NONBLOCK
#include <poll.h>        // for poll
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
    // create the listening socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        die("socket()");
    }

    // reuse port so we don't have to wait for the socket to free up
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        die("setsockopt()");
    }

    // setup server address
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(1234);  // port 1234
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); // listen on all interfaces

    // bind the socket to the address and port
    if (bind(socket_fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) != 0) {
        die("bind()");
    }

    // start listening
    if (listen(socket_fd, 10) < 0) {
        die("listen()");
    }
    printf("server listening on port 1234...\n");

    // set the listening socket to non-blocking mode
    set_nonblocking(socket_fd);

    // create a vector of pollfd structures
    std::vector<struct pollfd> pollfds;
    // add the listening socket at index 0
    struct pollfd pfd;
    pfd.fd = socket_fd;
    pfd.events = POLLIN; // we want to know when new connections come in
    pfd.revents = 0;
    pollfds.push_back(pfd);

    // event loop
    while (true) {
        // wait for readiness on our fds (blocking call, but only here)
        int rv = poll(pollfds.data(), pollfds.size(), -1);
        if (rv < 0) {
            if (errno == EINTR)
                continue; // interrupted by a signal, retry
            die("poll");
        }

        // loop through the pollfd list
        // note: index 0 is the listening socket
        for (size_t i = 0; i < pollfds.size(); i++) {
            // if it's the listening socket, check for incoming connections
            if (i == 0 && (pollfds[i].revents & POLLIN)) {
                // accept as many new connections as we can
                while (true) {
                    struct sockaddr_in clientAddress;
                    socklen_t clientAddrSize = sizeof(clientAddress);
                    int connfd = accept(socket_fd, (struct sockaddr *)&clientAddress, &clientAddrSize);
                    if (connfd < 0) {
                        // if no more connections are waiting, break out
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        perror("accept");
                        break;
                    }
                    printf("accepted a connection.\n");

                    // set the new connection to non-blocking
                    set_nonblocking(connfd);

                    // add this connection to our pollfd vector, watch for read events
                    struct pollfd client_pfd;
                    client_pfd.fd = connfd;
                    client_pfd.events = POLLIN;
                    client_pfd.revents = 0;
                    pollfds.push_back(client_pfd);
                }
            }
            // handle client connections
            else if (i > 0) {
                // if there is any error or hangup, close the connection
                if (pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    printf("closing connection (error/hangup).\n");
                    close(pollfds[i].fd);
                    pollfds.erase(pollfds.begin() + i);
                    i--; // adjust index after removal
                    continue;
                }
                // if the socket is ready for reading
                if (pollfds[i].revents & POLLIN) {
                    char buffer[1024] = {0};
                    ssize_t bytesRead = read(pollfds[i].fd, buffer, sizeof(buffer) - 1);
                    if (bytesRead < 0) {
                        // if nothing to read, continue
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            continue;
                        perror("read");
                        close(pollfds[i].fd);
                        pollfds.erase(pollfds.begin() + i);
                        i--;
                        continue;
                    } else if (bytesRead == 0) {
                        // connection closed by client
                        printf("client disconnected.\n");
                        close(pollfds[i].fd);
                        pollfds.erase(pollfds.begin() + i);
                        i--;
                        continue;
                    } else {
                        buffer[bytesRead] = '\0';
                        printf("received: %s\n", buffer);
                        // send a response
                        const char* response = "hello from server!\n";
                        ssize_t bytesSent = send(pollfds[i].fd, response, strlen(response), 0);
                        if (bytesSent < 0) {
                            perror("send");
                            close(pollfds[i].fd);
                            pollfds.erase(pollfds.begin() + i);
                            i--;
                            continue;
                        }
                    }
                }
            }
        } // end for loop over pollfds
    } // end event loop

    close(socket_fd);
    return 0;
}

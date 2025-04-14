#include <arpa/inet.h>   // for htons, htonl
#include <netinet/in.h>  // for sockaddr_in
#include <sys/socket.h>  // for socket, bind, listen, accept, send, recv
#include <unistd.h>      // for close, read
#include <stdlib.h>      // for exit
#include <stdio.h>
#include <string.h>      // for memset, strlen, strtok
#include <fcntl.h>       // for fcntl, O_NONBLOCK
#include <poll.h>        // for poll
#include <vector>
#include <string>
#include <sstream>

#include "hashtable.h"

// create a global hashtable for our key–value store, using 1024 buckets
hashtable *ht = ht_create(1024);

void die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// helper: set a fd to non-blocking mode
static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        die("fcntl(get)");
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0)
        die("fcntl(set)");
}

// simple command parser supporting "SET key value" and "GET key"
// returns a std::string response
std::string process_command(const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string token;
    iss >> token;
    if(token == "SET") {
        std::string key, value;
        iss >> key;
        std::getline(iss, value);
        // trim leading spaces in value
        size_t start = value.find_first_not_of(" ");
        if(start != std::string::npos)
            value = value.substr(start);
        if(ht_set(ht, key.c_str(), value.c_str()) == 0)
            return "OK\n";
        else
            return "ERR setting value\n";
    } else if(token == "GET") {
        std::string key;
        iss >> key;
        char *val = ht_get(ht, key.c_str());
        if(val)
            return std::string(val) + "\n";
        else
            return "(nil)\n";
    }
    return "ERR unknown command\n";
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
    // add the listening socket at index 0 (we want to know when new connections come in)
    struct pollfd pfd;
    pfd.fd = socket_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    pollfds.push_back(pfd);

    // event loop: wait for readiness on our fds
    while (true) {
        int rv = poll(pollfds.data(), pollfds.size(), -1);
        if (rv < 0) {
            if (errno == EINTR)
                continue; // if interrupted by signal, retry
            die("poll()");
        }
        // iterate over our pollfd list
        for (size_t i = 0; i < pollfds.size(); i++) {
            // if index 0 (listening socket) is ready, accept new connections
            if (i == 0 && (pollfds[i].revents & POLLIN)) {
                while (true) {
                    struct sockaddr_in clientAddress;
                    socklen_t clientAddrSize = sizeof(clientAddress);
                    int connfd = accept(socket_fd, (struct sockaddr *)&clientAddress, &clientAddrSize);
                    if (connfd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break; // no more pending connections
                        perror("accept");
                        break;
                    }
                    printf("accepted a connection.\n");
                    // set new connection to non-blocking and add to pollfds (watch for read events)
                    set_nonblocking(connfd);
                    struct pollfd client_pfd;
                    client_pfd.fd = connfd;
                    client_pfd.events = POLLIN;
                    client_pfd.revents = 0;
                    pollfds.push_back(client_pfd);
                }
            }
            // for client connections
            else if (i > 0) {
                // if error/hangup, close and remove this connection
                if (pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    printf("closing connection (error/hangup).\n");
                    close(pollfds[i].fd);
                    pollfds.erase(pollfds.begin() + i);
                    i--; // adjust index after removal
                    continue;
                }
                // if data available to read
                if (pollfds[i].revents & POLLIN) {
                    char buffer[1024] = {0};
                    ssize_t bytesRead = read(pollfds[i].fd, buffer, sizeof(buffer) - 1);
                    if (bytesRead < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            continue;
                        perror("read");
                        close(pollfds[i].fd);
                        pollfds.erase(pollfds.begin() + i);
                        i--;
                        continue;
                    } else if (bytesRead == 0) {
                        printf("client disconnected.\n");
                        close(pollfds[i].fd);
                        pollfds.erase(pollfds.begin() + i);
                        i--;
                        continue;
                    } else {
                        buffer[bytesRead] = '\0';
                        printf("received: %s\n", buffer);
                        // process command using our custom hashtable
                        std::string response = process_command(std::string(buffer));
                        ssize_t bytesSent = send(pollfds[i].fd, response.c_str(), response.length(), 0);
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
        } // end for over pollfds
    } // end event loop

    close(socket_fd);
    ht_destroy(ht); // free our hashtable
    return 0;
}

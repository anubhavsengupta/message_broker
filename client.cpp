#include <arpa/inet.h>   // for htons and htonl
#include <netinet/in.h>  // for sockaddr_in
#include <sys/socket.h>  // for socket, bind, listen
#include <stdlib.h>      // for exit

// 'die' function is defined to handle errors.
void die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    // Create a socket (file descriptor)
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        die("socket()");
    }

    // set up  server address structure
    struct sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;         
    serverAddress.sin_port = htons(1234);            
    serverAddress.sin_addr.s_addr = htonl(0);           //  wildcardaddress (0.0.0.0)

    // bind socket to the specified address and port
    int bindResult = bind(socket_fd, (const struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (bindResult != 0) {
        die("bind()");
    }

    // Start listening for incoming connections 
    if (listen(socket_fd, 10) < 0) {
        die("listen()");
    }
    
    return 0;
}

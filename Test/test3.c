#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>


int main() {
    //Create a simple TCP server
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Create a new network socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return -1;

    // Bind the socket to a port
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8081);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        return -1;

    // Listen for incoming connections
    if (listen(server_fd, 1) == -1)
        return -1;

    printf("Server: Waiting for connections...\n");

    // Accept a connection
    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) == -1)
        return -1;

    printf("Server: Connection established\n");

    // Send a message to the client
    if (send(client_fd, "Hello, client!", 14, 0) == -1)
        return -1;

    sleep(10);

    // Clean up
    close(client_fd);
    close(server_fd);
}
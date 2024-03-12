#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


int send_fd(int socket_fd, int fd_to_send) {
    struct msghdr msg = {0};
    struct iovec iov[1];
    char buf[1]; // We just need some data to send, it can be anything
    struct cmsghdr *cmsg;
    char cms[CMSG_SPACE(sizeof(int))];

    iov[0].iov_base = buf;
    iov[0].iov_len = 1;

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cms;
    msg.msg_controllen = sizeof(cms);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));

    *((int *)CMSG_DATA(cmsg)) = fd_to_send;

    return sendmsg(socket_fd, &msg, 0);
}

// Function to receive file descriptor over UNIX domain socket
int recv_fd(int socket_fd) {
    struct msghdr msg = {0};
    struct iovec iov[1];
    char buf[1]; // We just need some data to receive, it can be anything
    struct cmsghdr *cmsg;
    char cms[CMSG_SPACE(sizeof(int))];
    int fd;

    iov[0].iov_base = buf;
    iov[0].iov_len = 1;

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cms;
    msg.msg_controllen = sizeof(cms);

    if (recvmsg(socket_fd, &msg, 0) < 0)
        return -1;

    cmsg = CMSG_FIRSTHDR(&msg);

    if (cmsg == NULL || cmsg->cmsg_type != SCM_RIGHTS)
        return -1;

    fd = *((int *)CMSG_DATA(cmsg));

    return fd;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <fcntl.h>

#define SOCKET_PATH "/tmp/test_socket"

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    int server_fd, client_fd, received_fd;
    struct sockaddr_un addr;
    socklen_t addr_len = sizeof(addr);

    // Create a UNIX domain socket
    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        error("socket");

    // Bind the socket to a path
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCKET_PATH); // Remove any existing socket file
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        error("bind");

    // Listen for incoming connections
    if (listen(server_fd, 1) == -1)
        error("listen");

    printf("Server: Waiting for connections...\n");

    // Accept a connection
    if ((client_fd = accept(server_fd, (struct sockaddr *)&addr, &addr_len)) == -1)
        error("accept");

    printf("Server: Connection established\n");

    // Receive the network socket file descriptor from the client
    received_fd = recv_fd(client_fd);
    if (received_fd == -1)
        error("recv_fd");

    printf("Server: Received network socket file descriptor %d\n", received_fd);

    // Print the IP address and port of the client using getsockname()
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    if (getsockname(received_fd, (struct sockaddr *)&client_addr, &client_addr_len) == -1)
        error("getsockname");
    printf("Server: Client IP address: %s\n", inet_ntoa(client_addr.sin_addr));
    printf("Server: Client port: %d\n", ntohs(client_addr.sin_port));

    // Receive a message from the client
    char buffer[1024];
    if (recv(received_fd, buffer, sizeof(buffer), 0) == -1)
        error("recv");
    printf("Server: Received message from client: %s\n", buffer);

    // Use the received network socket file descriptor (for demonstration, close it)
    close(received_fd);

    // Clean up
    close(client_fd);
    close(server_fd);

    return 0;
}



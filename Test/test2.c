#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define SOCKET_PATH "/tmp/test_socket"

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
#include <netinet/in.h>
#include <fcntl.h>

#define SOCKET_PATH "/tmp/test_socket"

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    int client_fd, new_fd;
    struct sockaddr_un addr;

    // Create a UNIX domain socket
    if ((client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        error("socket");

    // Connect to the server
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        error("connect");

    printf("Client: Connected to server\n");

    // Create a new network socket
    if ((new_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        error("socket");
    // Bind the socket to a port
    struct sockaddr_in cli_addr;
    cli_addr.sin_family = AF_INET;
    cli_addr.sin_port = htons(8080);
    cli_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(new_fd, (struct sockaddr *)&cli_addr, sizeof(cli_addr)) == -1)
        error("bind");
    // Connect to the server
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8081);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(new_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error("connect");

    printf("Client: Connected to TCP server\n");

    // Send the network socket file descriptor to the server
    if (send_fd(client_fd, new_fd) == -1)
        error("send_fd");

    printf("Client: Sent network socket file descriptor %d\n", new_fd);

    // Clean up
    close(new_fd);
    close(client_fd);

    return 0;
}



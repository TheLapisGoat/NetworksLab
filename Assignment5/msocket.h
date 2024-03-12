#ifndef MSOCKET_H
#define MSOCKET_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#define SHM_NAME "msocket_shm"
#define SHM_SOCKINFO_NAME "msocket_shm_sockinfo"
#define SHM_MUTEX_NAME "msocket_shm_mtx"
#define SOCK_MUTEX_NAME "sock_mtx"
#define INIT_MUTEX_NAME "init_mtx"
#define SOCK_CREATION_MUTEX_NAME "sock_creation_mtx"
#define SOCK_MTP 42
#define N 25
#define ENOTBOUND 218
struct SOCK_INFO {
    int sock_id;    // Socket ID
    char ip[32];    // IP Address
    int port;       // Port Number
    int errnum;      // Error Number
};

struct wnd_t {
    int size;   // Size of the window
    int msg_seq_nos[20];    // Message Sequence Nos
    struct timeval timeouts[16];    // Timeouts
    char msgs[16][1024];    // Messages (This is the error control buffer)
};

struct msocket_t {
    int status;         // 0: Free, 1: Alloted
    pid_t pid;          // Process ID of process that created the socket
    int udp_id;         // ID of corresponding UDP socket
    char opp_ip[32];    // IP address of the opposite end
    int opp_port;       // Port number of the opposite end
    char send_buffer[10][1024];  // Send Buffer (This is the flow control buffer)
    int send_free_slot;   // First free slot in Send Buffer Free Slot
    char recv_buffer[5][1024];  // Receive Buffer (This is the flow control buffer)
    int recv_filled[5]; // 0: Empty, 1: Filled
    struct wnd_t swnd;  // Send Window
    struct wnd_t rwnd;  // Receive Window
};

int m_socket(int domain, int type, int protocol);

int m_bind(int sockfd, char * src_ip, int src_port, char * dst_ip, int dst_port);

ssize_t m_sendto(int sockfd, const void *buf, size_t len, const struct sockaddr *dest_addr, size_t addrlen);

ssize_t m_recvfrom(int sockfd, void *buf, size_t len, struct sockaddr *src_addr, size_t *addrlen);

int m_close(int sockfd);

#endif
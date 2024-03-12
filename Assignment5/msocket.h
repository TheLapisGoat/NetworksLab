#ifndef MSOCKET_H
#define MSOCKET_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#define SHM_NAME "msocket_shm"
#define SHM_MUTEX_NAME "msocket_shm_mtx"
#define N 25

struct wnd_t {
    int size;   // Size of the window
    int msg_ids[20];    // Message IDs
};

struct msocket_t {
    int status;         // 0: Free, 1: Alloted
    pid_t pid;          // Process ID of process that created the socket
    int udp_id;         // ID of corresponding UDP socket
    char opp_ip[32];    // IP address of the opposite end
    int opp_port;       // Port number of the opposite end
    char send_buffer[10][1024];  // Send Buffer
    char recv_buffer[5][1024];  // Receive Buffer
    struct wnd_t swnd;  // Send Window
    struct wnd_t rwnd;  // Receive Window
};
#endif
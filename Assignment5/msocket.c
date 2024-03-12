#include "msocket.h"
#include <stdio.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <errno.h>
#include <arpa/inet.h>

int m_socket(int domain, int type, int protocol) {
    //Checking if the domain is AF_INET
    if (domain != AF_INET) {
        return -1;
    }
    //Checking if the type is SOCK_MTP
    if (type != SOCK_MTP) {
        return -1;
    }
    //Getting the shared memory
    int shmid = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shmid == -1) {
        return -1;
    }
    struct msocket_t *SM = (struct msocket_t *) mmap(NULL, sizeof(struct msocket_t) * N, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);
    //Getting the shared memory for the socket info
    int shmid_sockinfo = shm_open(SHM_SOCKINFO_NAME, O_RDWR, 0666);
    if (shmid_sockinfo == -1) {
        return -1;
    }
    struct SOCK_INFO *sock_info = (struct SOCK_INFO *) mmap(NULL, sizeof(struct SOCK_INFO) * N, PROT_READ | PROT_WRITE, MAP_SHARED, shmid_sockinfo, 0);
    //Getting the mutex for the shared memory
    sem_t *shm_mtx = sem_open(SHM_MUTEX_NAME, O_RDWR);
    if (shm_mtx == SEM_FAILED) {
        //Removing the shared memory
        return -1;
    }
    
    //Getting the mutex for socket creation/binding
    sem_t *sock_mtx = sem_open(SOCK_MUTEX_NAME, O_RDWR);
    if (sock_mtx == SEM_FAILED) {
        return -1;
    }

    //Getting the mutex that init is waiting on
    sem_t *init_mtx = sem_open(INIT_MUTEX_NAME, O_RDWR);
    if (init_mtx == SEM_FAILED) {
        return -1;
    }

    //Getting the mutex for socket creation (to be used to wait on)
    sem_t *sock_creation_mtx = sem_open(SOCK_CREATION_MUTEX_NAME, O_RDWR);
    if (sock_creation_mtx == SEM_FAILED) {
        return -1;
    }

    //Locking the mutex for socket creation
    sem_wait(sock_mtx);
    //Locking the mutex for shared memory
    sem_wait(shm_mtx);

    //Finding a free socket
    int i;
    for (i = 0; i < N; i++) {
        if (SM[i].status == 0) {
            //Found a free socket
            break;
        }
    }

    //Checking if the number of sockets is exhausted
    if (i == N) {
        //Unlocking the mutex for shared memory
        sem_post(shm_mtx);
        //Unlocking the mutex for socket creation
        sem_post(sock_mtx);
        //Setting the errno
        errno = ENOBUFS;
        return -1;
    }

    //If empty socket is found, setting SOCK_INFO
    memset(sock_info, 0, sizeof(struct SOCK_INFO));

    //Unlocking the mutex for shared memory
    sem_post(shm_mtx);

    //Signalling the init that a socket has to be created
    sem_post(init_mtx);
    //Waiting for the init to create the socket
    sem_wait(sock_creation_mtx);

    //Getting the socket details from SOCK_INFO
    int sock_id = sock_info->sock_id;
    int errnum = sock_info->errnum;
    int port = sock_info->port;
    char ip[32];
    strcpy(ip, sock_info->ip);

    //Checking if the socket creation was successful
    if (sock_id == -1) {
        //Unlocking the mutex for socket creation
        sem_post(sock_mtx);
        //Setting the errno
        errno = errnum;
        return -1;
    }

    //Setting the socket details in the shared memory
    sem_wait(shm_mtx);
    SM[sock_id].pid = getpid();
    sem_post(shm_mtx);

    memset(sock_info, 0, sizeof(struct SOCK_INFO));

    //Unlocking the mutex for socket creation
    sem_post(sock_mtx);

    //Returning the socket id
    return sock_id;
}

int m_bind(int sockfd, char * src_ip, int src_port, char * dst_ip, int dst_port) {
    //Checking if the socket id is valid
    if (sockfd < 0 || sockfd >= N) {
        return -1;
    }
    //Checking if the source port is valid
    if (src_port < 1024 || src_port > 65535) {
        return -1;
    }
    //Checking if the destination port is valid
    if (dst_port < 1024 || dst_port > 65535) {
        return -1;
    }
    //Getting the shared memory
    int shmid = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shmid == -1) {
        return -1;
    }
    struct msocket_t *SM = (struct msocket_t *) mmap(NULL, sizeof(struct msocket_t) * N, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);
    //Getting the shared memory for the socket info
    int shmid_sockinfo = shm_open(SHM_SOCKINFO_NAME, O_RDWR, 0666);
    if (shmid_sockinfo == -1) {
        return -1;
    }
    struct SOCK_INFO *sock_info = (struct SOCK_INFO *) mmap(NULL, sizeof(struct SOCK_INFO) * N, PROT_READ | PROT_WRITE, MAP_SHARED, shmid_sockinfo, 0);
    //Getting the mutex for the shared memory
    sem_t *shm_mtx = sem_open(SHM_MUTEX_NAME, O_RDWR);
    if (shm_mtx == SEM_FAILED) {
        //Removing the shared memory
        return -1;
    }
    
    //Getting the mutex for socket creation/binding
    sem_t *sock_mtx = sem_open(SOCK_MUTEX_NAME, O_RDWR);
    if (sock_mtx == SEM_FAILED) {
        return -1;
    }

    //Getting the mutex that init is waiting on
    sem_t *init_mtx = sem_open(INIT_MUTEX_NAME, O_RDWR);
    if (init_mtx == SEM_FAILED) {
        return -1;
    }

    //Getting the mutex for socket creation (to be used to wait on)
    sem_t *sock_creation_mtx = sem_open(SOCK_CREATION_MUTEX_NAME, O_RDWR);
    if (sock_creation_mtx == SEM_FAILED) {
        return -1;
    }
    sem_wait(shm_mtx);
    //Checking if the socket belongs to the process
    if (SM[sockfd].pid != getpid()) {
        //Unlocking the mutex for shared memory
        sem_post(shm_mtx);
        //Setting the errno
        errno = EACCES;
        return -1;
    }
    sem_post(shm_mtx);
    sem_wait(sock_mtx);

    memset(sock_info, 0, sizeof(struct SOCK_INFO));
    sock_info->sock_id = sockfd;
    strcpy(sock_info->ip, src_ip);
    sock_info->port = src_port;

    sem_post(init_mtx);
    sem_wait(sock_creation_mtx);

    int sock_id = sock_info->sock_id;
    if (sock_id == -1) {
        sem_post(sock_mtx);
        errno = sock_info->errnum;
        return -1;
    }

    sem_wait(shm_mtx);
    strcpy(SM[sock_id].opp_ip, dst_ip);
    SM[sock_id].opp_port = dst_port;
    sem_post(shm_mtx);

    memset(sock_info, 0, sizeof(struct SOCK_INFO));
    sem_post(sock_mtx);

    return 0;
}

ssize_t m_sendto(int sockfd, const void *buf, size_t len, const struct sockaddr *dest_addr, size_t addrlen) {
    //Checking if the socket id is valid
    if (sockfd < 0 || sockfd >= N) {
        return -1;
    }
    //Getting the address and port from the sockaddr
    struct sockaddr_in *addr = (struct sockaddr_in *) dest_addr;
    char *dst_ip = inet_ntoa(addr->sin_addr);
    int dst_port = ntohs(addr->sin_port);
    
    //Getting the shared memory
    int shmid = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shmid == -1) {
        return -1;
    }
    struct msocket_t *SM = (struct msocket_t *) mmap(NULL, sizeof(struct msocket_t) * N, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);

    //Getting the mutex for the corresponding send buffer
    char name[32];
    sprintf(name, "send_mtx_%d", sockfd);
    sem_t *send_mtx = sem_open(name, O_RDWR);
    if (send_mtx == SEM_FAILED) {
        return -1;
    }

    //Checking if the socket belongs to the process
    if (SM[sockfd].pid != getpid()) {
        return -1;
    }
    //Checking if the destination IP and port are same as the bound IP and port
    if (strcmp(SM[sockfd].opp_ip, dst_ip) != 0 || SM[sockfd].opp_port != dst_port) {
        errno = ENOTBOUND;
        return -1;
    }

    sem_wait(send_mtx);
    //Looking for a free slot in the send buffer
    int send_free_slot = SM[sockfd].send_free_slot;
    if (send_free_slot == 10) {
        sem_post(send_mtx);
        errno = ENOBUFS;
        return -1;
    }
    //If the message is too large
    if (len > 1024) {
        sem_post(send_mtx);
        errno = EMSGSIZE;
        return -1;
    }
    //Copying the data to the send buffer 
    memcpy(SM[sockfd].send_buffer[send_free_slot], buf, len);
    SM[sockfd].send_free_slot++;
    sem_post(send_mtx);

    return len;
}

ssize_t m_recvfrom(int sockfd, void *buf, size_t len, struct sockaddr *src_addr, size_t *addrlen) {
    //Checking if the socket id is valid
    if (sockfd < 0 || sockfd >= N) {
        return -1;
    }
    //Getting the shared memory
    int shmid = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shmid == -1) {
        return -1;
    }
    struct msocket_t *SM = (struct msocket_t *) mmap(NULL, sizeof(struct msocket_t) * N, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);

    //Getting the mutex for the corresponding recv buffer
    char name[32];
    sprintf(name, "recv_mtx_%d", sockfd);
    sem_t *recv_mtx = sem_open(name, O_RDWR);
    if (recv_mtx == SEM_FAILED) {
        return -1;
    }

    //Checking if the socket belongs to the process
    if (SM[sockfd].pid != getpid()) {
        return -1;
    }

    sem_wait(recv_mtx);
    //Looking for a filled slot in the receive buffer
    int i;
    for (i = 0; i < 5; i++) {
        if (SM[sockfd].recv_filled[i] == 1) {
            break;
        }
    }
    //If no message is received
    if (i == 5) {
        sem_post(recv_mtx);
        errno = ENOMSG;
        return -1;
    }
    //Copying the data from the receive buffer
    memset(buf, 0, len);
    ssize_t cpy_len = len < 1024 ? len : 1024;
    memcpy(buf, SM[sockfd].recv_buffer[i], cpy_len);
    SM[sockfd].recv_filled[i] = 0;
    sem_post(recv_mtx);

    return cpy_len;
}

int m_close(int sockfd) {
    //Checking if the socket id is valid
    if (sockfd < 0 || sockfd >= N) {
        return -1;
    }
    //Getting the shared memory
    int shmid = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shmid == -1) {
        return -1;
    }
    struct msocket_t *SM = (struct msocket_t *) mmap(NULL, sizeof(struct msocket_t) * N, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);
    //Getting the shared memory for the socket info
    int shmid_sockinfo = shm_open(SHM_SOCKINFO_NAME, O_RDWR, 0666);
    if (shmid_sockinfo == -1) {
        return -1;
    }
    struct SOCK_INFO *sock_info = (struct SOCK_INFO *) mmap(NULL, sizeof(struct SOCK_INFO) * N, PROT_READ | PROT_WRITE, MAP_SHARED, shmid_sockinfo, 0);
    //Getting the mutex for the shared memory
    sem_t *shm_mtx = sem_open(SHM_MUTEX_NAME, O_RDWR);
    if (shm_mtx == SEM_FAILED) {
        //Removing the shared memory
        return -1;
    }
    
    //Getting the mutex for socket creation/binding
    sem_t *sock_mtx = sem_open(SOCK_MUTEX_NAME, O_RDWR);
    if (sock_mtx == SEM_FAILED) {
        return -1;
    }

    //Getting the mutex that init is waiting on
    sem_t *init_mtx = sem_open(INIT_MUTEX_NAME, O_RDWR);
    if (init_mtx == SEM_FAILED) {
        return -1;
    }

    //Getting the mutex for socket creation (to be used to wait on)
    sem_t *sock_creation_mtx = sem_open(SOCK_CREATION_MUTEX_NAME, O_RDWR);
    if (sock_creation_mtx == SEM_FAILED) {
        return -1;
    }

    //Getting the mutex for the corresponding send buffer
    char name[32];
    sprintf(name, "send_mtx_%d", sockfd);
    sem_t *send_mtx = sem_open(name, O_RDWR);
    if (send_mtx == SEM_FAILED) {
        return -1;
    }

    //Getting the mutex for the corresponding recv buffer
    sprintf(name, "recv_mtx_%d", sockfd);
    sem_t *recv_mtx = sem_open(name, O_RDWR);
    if (recv_mtx == SEM_FAILED) {
        return -1;
    }

    //Checking if the socket belongs to the process
    if (SM[sockfd].pid != getpid()) {
        return -1;
    }

    sem_wait(sock_mtx);
    //Stop all the send and receive operations
    sem_wait(send_mtx);
    sem_wait(recv_mtx);

    memset(sock_info, 0, sizeof(struct SOCK_INFO));
    sock_info->sock_id = sockfd;
    sock_info->port = -1;
    
    sem_post(init_mtx);
    sem_wait(sock_creation_mtx);

    int sock_id = sock_info->sock_id;
    if (sock_id == -1) {
        sem_post(send_mtx);
        sem_post(recv_mtx);
        sem_post(sock_mtx);
        errno = sock_info->errnum;
        return -1;
    }

    sem_wait(shm_mtx);
    memset(&SM[sock_id], 0, sizeof(struct msocket_t));
    sem_post(shm_mtx);

    memset(sock_info, 0, sizeof(struct SOCK_INFO));

    sem_post(send_mtx);
    sem_post(recv_mtx);
    sem_post(sock_mtx);

    return 0;
}


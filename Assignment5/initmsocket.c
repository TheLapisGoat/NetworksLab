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
#include "msocket.h"

void * R_main(void * arg) {
    // This is the R thread
    printf("Started Thread R\n");
    struct msocket_t *SM = (struct msocket_t *) arg;

    pthread_exit(NULL);
}

void * S_main(void * arg) {
    // This is the S thread
    printf("Started Thread S\n");
    struct msocket_t *SM = (struct msocket_t *) arg;

    pthread_exit(NULL);
}

void * G_main(void * arg) {
    // This is the G thread (Garbage Collector thread)
    printf("Started Thread G\n");
    struct msocket_t *SM = (struct msocket_t *) arg;

    pthread_exit(NULL);
}

int main() {
    // This is the P thread
    // Create a shared memory for the msocket using shm_open
    int shmid = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shmid == -1) {
        perror("shm_open");
        exit(1);
    }
    // Set the size of the shared memory
    ftruncate(shmid, sizeof(struct msocket_t) * N);
    // Map the shared memory to the address space of the process
    struct msocket_t *SM = (struct msocket_t *) mmap(NULL, sizeof(struct msocket_t) * N, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);

    // Initialize the SM
    memset(SM, 0, sizeof(struct msocket_t) * N);

    //Creating the shared memory for the socket info
    int shmid_sockinfo = shm_open(SHM_SOCKINFO_NAME, O_CREAT | O_RDWR, 0666);
    if (shmid_sockinfo == -1) {
        perror("shm_open");
        exit(1);
    }
    // Set the size of the shared memory
    ftruncate(shmid_sockinfo, sizeof(struct SOCK_INFO));
    // Map the shared memory to the address space of the process
    struct SOCK_INFO *sock_info = (struct SOCK_INFO *) mmap(NULL, sizeof(struct SOCK_INFO), PROT_READ | PROT_WRITE, MAP_SHARED, shmid_sockinfo, 0);

    // Initialize the sock_info
    memset(sock_info, 0, sizeof(struct SOCK_INFO));

    //Creating mtx for the shared memory
    sem_t *shm_mtx = sem_open(SHM_MUTEX_NAME, O_CREAT, 0666, 1);
    if (shm_mtx == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }
    sem_init(shm_mtx, 1, 1);

    //Creating mtx for the socket creation/binding
    sem_t *sock_mtx = sem_open(SOCK_MUTEX_NAME, O_CREAT, 0666, 1);
    if (sock_mtx == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }
    sem_init(sock_mtx, 1, 1);

    //Creating a mtx for init to wait on
    sem_t *init_mtx = sem_open(INIT_MUTEX_NAME, O_CREAT, 0666, 0);
    if (init_mtx == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }
    sem_init(init_mtx, 1, 0);

    //Creating a mtx for the socket creation (to be used to wait on)
    sem_t *sock_creation_mtx = sem_open(SOCK_CREATION_MUTEX_NAME, O_CREAT, 0666, 0);
    if (sock_creation_mtx == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }
    sem_init(sock_creation_mtx, 1, 0);

    //Creating 25 mutexes, one for each send buffer of a socket
    for (int i = 0; i < N; i++) {
        char name[32];
        sprintf(name, "send_mtx_%d", i);
        sem_t *send_mtx = sem_open(name, O_CREAT, 0666, 1);
        if (send_mtx == SEM_FAILED) {
            perror("sem_open");
            exit(1);
        }
        sem_init(send_mtx, 1, 1);
    }

    //Creating 25 mutexes, one for each recv buffer of a socket
    for (int i = 0; i < N; i++) {
        char name[32];
        sprintf(name, "recv_mtx_%d", i);
        sem_t *recv_mtx = sem_open(name, O_CREAT, 0666, 1);
        if (recv_mtx == SEM_FAILED) {
            perror("sem_open");
            exit(1);
        }
        sem_init(recv_mtx, 1, 1);
    }

    // Creating the threads R, S and G
    pthread_t R, S, G;
    pthread_create(&R, NULL, R_main, (void *) SM);
    pthread_create(&S, NULL, S_main, (void *) SM);
    pthread_create(&G, NULL, G_main, (void *) SM);

    while (1) {
        // Wait on the init_mtx
        sem_wait(init_mtx);

        // Lock the mutex for shared memory
        sem_wait(shm_mtx);
        
        // Checking value of sock_info
        if (sock_info->sock_id == 0 && strcmp(sock_info->ip, "") == 0 && sock_info->port == 0 && sock_info->errnum == 0) {
            // If all fields are 0, it is a m_socket call
            // Finding a free socket
            int i;
            for (i = 0; i < N; i++) {
                if (SM[i].status == 0) {
                    // Found a free socket
                    break;
                }
            }

            // Creating a socket
            int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd == -1) {
                // If the socket creation fails
                sock_info->errnum = errno;
                sock_info->sock_id = -1;
                sem_post(shm_mtx);
                sem_post(sock_creation_mtx);
                continue;
            }

            // Setting the SOCK_INFO
            sock_info->sock_id = i;
            sock_info->errnum = 0;
            sock_info->port = 0;
            strcpy(sock_info->ip, "");

            // Setting SM
            SM[i].status = 1;
            SM[i].udp_id = sockfd;
            SM[i].swnd.size = 10;
            SM[i].rwnd.size = 5;

            sem_post(shm_mtx);
            sem_post(sock_creation_mtx);
            continue;
        } else if (sock_info->port >= 0) {
            //Bind call
            int sock_id = sock_info->sock_id;
            char src_ip[32];
            strcpy(src_ip, sock_info->ip);
            int src_port = sock_info->port;

            // Do the bind
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(src_port);
            addr.sin_addr.s_addr = inet_addr(src_ip);
            int ret = bind(SM[sock_id].udp_id, (struct sockaddr *) &addr, sizeof(addr));
            if (ret == -1) {
                // If the bind fails
                sock_info->errnum = errno;
                sock_info->sock_id = -1;
                sem_post(shm_mtx);
                sem_post(sock_creation_mtx);
                continue;
            }

            //Debug Msg: Print IP and Port
            printf("BIND: IP: %s, Port: %d\n", src_ip, src_port);
            fflush(stdout);

            sem_post(shm_mtx);
            sem_post(sock_creation_mtx);
            continue;
        } else {
            //Close call
            int sock_id = sock_info->sock_id;
            int ret = close(SM[sock_id].udp_id);
            if (ret == -1) {
                // If the close fails
                sock_info->errnum = errno;
                sock_info->sock_id = -1;
                sem_post(shm_mtx);
                sem_post(sock_creation_mtx);
                continue;
            }

            //Debug Msg: Print Socket ID
            printf("CLOSE: Socket ID: %d\n", sock_id);

            sem_post(shm_mtx);
            sem_post(sock_creation_mtx);
            continue;
        }
    }

    // Wait for the threads to finish
    pthread_join(R, NULL);
    printf("Thread R finished\n");
    pthread_join(S, NULL);
    printf("Thread S finished\n");
    pthread_join(G, NULL);
    printf("Thread G finished\n");

    // Unmap the shared memory
    munmap(SM, sizeof(struct msocket_t) * N);
    // Close the shared memory
    close(shmid);
    // Unlink the shared memory
    shm_unlink(SHM_NAME);

    // Unmap the shared memory
    munmap(sock_info, sizeof(struct SOCK_INFO));
    // Close the shared memory
    close(shmid_sockinfo);
    // Unlink the shared memory
    shm_unlink(SHM_SOCKINFO_NAME);

    // Close the semaphore
    sem_close(shm_mtx);
    // Unlink the semaphore
    sem_unlink(SHM_MUTEX_NAME);

    // Close the semaphore
    sem_close(sock_mtx);
    // Unlink the semaphore
    sem_unlink(SOCK_MUTEX_NAME);

    // Close the semaphore
    sem_close(init_mtx);
    // Unlink the semaphore
    sem_unlink(INIT_MUTEX_NAME);
    return 0;
}
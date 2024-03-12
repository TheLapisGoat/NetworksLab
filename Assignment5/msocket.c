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

int m_socket(int domain, int type, int protocol) {
    //Checking if the domain is 
    //Getting the shared memory
    int shmid = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shmid == -1) {
        return -1;
    }
    struct msocket_t *SM = (struct msocket_t *) mmap(NULL, sizeof(struct msocket_t) * N, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);

    //Getting the mutex for the shared memory
    sem_t *shm_mtx = sem_open(SHM_MUTEX_NAME, O_RDWR);
    if (shm_mtx == SEM_FAILED) {
        return -1;
    }

}


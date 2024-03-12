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

void garbage_collector(struct msocket_t *SM) {
    printf("Started Garbage Collector\n");
}

int main() {
    pid_t g_pid;
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

    //Creating mtx for the shared memory
    sem_t *shm_mtx = sem_open(SHM_MUTEX_NAME, O_CREAT, 0666, 1);
    if (shm_mtx == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    // Creating the threads R and S
    pthread_t R, S;
    pthread_create(&R, NULL, R_main, (void *) SM);
    pthread_create(&S, NULL, S_main, (void *) SM);

    // Forking the process to create the Garbage Collector
    if ((g_pid = fork()) == 0) {
        // This is the Garbage Collector
        garbage_collector(SM);
        exit(0);
    }

    //Wait for the Garbage Collector to finish
    waitpid(g_pid, NULL, 0);
    printf("Garbage Collector finished\n");

    // Wait for the threads to finish
    pthread_join(R, NULL);
    printf("Thread R finished\n");
    pthread_join(S, NULL);
    printf("Thread S finished\n");

    // Unmap the shared memory
    munmap(SM, sizeof(struct msocket_t) * N);
    // Close the shared memory
    close(shmid);
    // Unlink the shared memory
    shm_unlink(SHM_NAME);

    // Close the semaphore
    sem_close(shm_mtx);
    // Unlink the semaphore
    sem_unlink(SHM_MUTEX_NAME);
    return 0;
}
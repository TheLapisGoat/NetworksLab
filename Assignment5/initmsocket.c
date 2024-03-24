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
#include <sys/time.h>
#include <sys/select.h>
#include "msocket.h"

int total_msg_sent = 0;

#define S_SLEEP T/2 > 1 ? T/2 : 1       //S_Sleep is the max of T/2 and 1

void * R_main(void * arg) {
    // This is the R thread
    printf("Started Thread R\n");
    struct msocket_t *SM = (struct msocket_t *) arg;
    // Getting the mutex for shared memory
    sem_t *shm_mtx = sem_open(SHM_MUTEX_NAME, O_CREAT, 0666, 1);
    // Getting the 25 mutexes for recv buffers
    sem_t *recv_mtx[25];
    for (int i = 0; i < N; i++) {
        char name[32];
        sprintf(name, "recv_mtx_%d", i);
        recv_mtx[i] = sem_open(name, O_CREAT, 0666, 1);
        if (recv_mtx[i] == SEM_FAILED) {
            perror("sem_open");
            exit(1);
        }
    }
    // Getting the 25 mutexes for send buffers
    sem_t *send_mtx[25];
    for (int i = 0; i < N; i++) {
        char name[32];
        sprintf(name, "send_mtx_%d", i);
        send_mtx[i] = sem_open(name, O_CREAT, 0666, 1);
        if (send_mtx[i] == SEM_FAILED) {
            perror("sem_open");
            exit(1);
        }
    }

    fd_set readfds;
    struct timeval timeout;
    while (1) {
        sem_wait(shm_mtx);
        FD_ZERO(&readfds);
        int max_fd = 0;
        for (int i = 0; i < N; i++) {
            if (SM[i].status == 1) {
                FD_SET(SM[i].udp_id, &readfds);
                max_fd = max_fd > SM[i].udp_id ? max_fd : SM[i].udp_id;
            }
        }
        timeout.tv_sec = T; timeout.tv_usec = 0;
        sem_post(shm_mtx);
        int ret = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        if (ret == -1) {
            perror("select");
            exit(1);
        }
        sem_wait(shm_mtx);
        for (int i = 0; i < N; i++) {
            sem_wait(recv_mtx[i]);
            if (SM[i].status == 1) {
                // Trying to move the base of the receive window forward (Done by seeing if it is filled, and trying to move that data to the receive buffer)
                while (SM[i].rwnd.filled[SM[i].rwnd.base] == 1) {
                    // Checking if the receive buffer has space
                    if (SM[i].recv_count < 5) {
                        // Receive buffer has space
                        // Moving the message to the receive buffer
                        memset(SM[i].recv_buffer[SM[i].recv_count], 0, 1024);
                        memcpy(SM[i].recv_buffer[SM[i].recv_count], SM[i].rwnd.msgs[SM[i].rwnd.base], SM[i].rwnd.msg_len[SM[i].rwnd.base]);
                        SM[i].recv_length[SM[i].recv_count] = SM[i].rwnd.msg_len[SM[i].rwnd.base];
                        SM[i].rwnd.filled[SM[i].rwnd.base] = 0;
                        SM[i].rwnd.base = (SM[i].rwnd.base + 1) % 16;
                        SM[i].recv_count++;
                    } else {
                        // Receive buffer has no space
                        break;
                    }
                }
            }
            if (SM[i].status == 1 && ret == 0) {
                // Timeout
                // Checking remaining rwnd size
                int remaining = (SM[i].rwnd.size - SM[i].rwnd.next_seq_no + SM[i].rwnd.base + 32) % 16;
                //Print rwnd size, next_seq_no, base
                // printf("Timeout for socket %d, rw_size: %d, next_seq_no: %d, base: %d, remaining: %d, nospace: %d\n", i, SM[i].rwnd.size, SM[i].rwnd.next_seq_no, SM[i].rwnd.base, remaining, SM[i].nospace);
                // fflush(stdout);
                if (SM[i].nospace == 1 && remaining > 0) {
                    // printf("Timeout for socket %d, remaining: %d\n", i, remaining);
                    // fflush(stdout);

                    // Send an ACK with the remaining rwnd size
                    char ack[6];
                    ack[0] = 20;
                    memcpy(ack + 1, &(remaining), 4);
                    if (SM[i].rwnd.next_seq_no != SM[i].rwnd.base || SM[i].rwnd.base != 0) {
                        // In-order message received
                        ack[5] = (SM[i].rwnd.next_seq_no - 1 + 16) % 16;
                    } else {
                        // No in-order message received yet
                        ack[5] = 20;
                    }
                    struct sockaddr_in dest_addr;
                    dest_addr.sin_family = AF_INET;
                    dest_addr.sin_port = htons(SM[i].opp_port);
                    dest_addr.sin_addr.s_addr = inet_addr(SM[i].opp_ip);
                    // printf("Timeout: Sending ACK from socket %d, for seq_no: %d\n", i, ack[5]);
                    // fflush(stdout);
                    sendto(SM[i].udp_id, ack, 6, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
                    total_msg_sent++;
                }
                sem_post(recv_mtx[i]);
                continue;
            }
            if (SM[i].status == 1 && FD_ISSET(SM[i].udp_id, &readfds)) {
                // Receiving the message
                char msg[1025];
                struct sockaddr_in src_addr;
                socklen_t addrlen = sizeof(src_addr);
                int n = recvfrom(SM[i].udp_id, msg, 1025, 0, (struct sockaddr *) &src_addr, &addrlen);
                if (n == -1) {
                    perror("recvfrom");
                    exit(1);
                }

                //Simulating msg drop
                if (dropMessage()) {
                    // Print the dropped msg and socket
                    printf("Dropped msg: %s, for socket %d\n", msg + 1, i);
                    fflush(stdout);
                    sem_post(recv_mtx[i]);
                    continue;
                }

                // Getting the sequence number
                int seq_no = msg[0];
                if (seq_no > 15) {
                    sem_wait(send_mtx[i]);
                    // Message is an ACK, Format: First Byte: Seq No (20, to represent and ACK), Next 4 Bytes: Recv Wnd Size, Next Byte: Last In-Order Seq No
                    int recv_wnd_size = *((int *) (msg + 1));
                    int last_in_order_seq_no = msg[5];
                    //Debug
                    // printf("R Thread: Received ACK for socket %d, recv_wnd_size: %d, last_in_order_seq_no: %d\n", i, recv_wnd_size, last_in_order_seq_no);
                    // fflush(stdout);
                    // Updating the send window size to the received window size
                    SM[i].swnd.size = recv_wnd_size;
                    // Moving the base of the send window to the last in-order seq no + 1 (Only if the last in-order seq no is valid, else ignore)
                    if (last_in_order_seq_no <= 15) {
                        // Moving the base of the send window to the last in-order seq no + 1
                        SM[i].swnd.base = (last_in_order_seq_no + 1) % 16;
                    }
                    sem_post(send_mtx[i]);
                } else {
                    // Message is a data message, Format: First Byte: Seq No, Next 1024 Bytes: Message
                    SM[i].nospace = 0;
                    char msg_data[1024];
                    memset(msg_data, 0, 1024);
                    memcpy(msg_data, msg + 1, n - 1);
                    //Debug
                    // printf("R Thread: Received the msg with seq no %d: %s\n", seq_no, msg_data);
                    // fflush(stdout);
                    // Checking if the message seq number is in the receive window
                    if ((seq_no - SM[i].rwnd.base + 16) % 16 >= SM[i].rwnd.size) {
                        // Message seq number is not in the receive window (Dropping Msg)
                        // printf("R Thread: (Not in rwnd) Dropping the msg: %s\n", msg_data);
                        // fflush(stdout);

                        // Sending an ACK for the last in-order message
                        char ack[6];
                        ack[0] = 20;
                        int remaining = (SM[i].rwnd.size - SM[i].rwnd.next_seq_no + SM[i].rwnd.base + 32) % 16;
                        memcpy(ack + 1, &(remaining), 4);
                        ack[5] = (SM[i].rwnd.next_seq_no - 1 + 16) % 16;

                        struct sockaddr_in dest_addr;
                        dest_addr.sin_family = AF_INET;
                        dest_addr.sin_port = htons(SM[i].opp_port);
                        dest_addr.sin_addr.s_addr = inet_addr(SM[i].opp_ip);
                        // printf("R Thread: (Not in rwnd) Sending ACK from socket %d, for seq_no: %d\n", i, ack[5]);
                        // fflush(stdout);
                        sendto(SM[i].udp_id, ack, 6, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
                        total_msg_sent++;

                        sem_post(recv_mtx[i]);
                        continue;
                    }
                    // Checking if the message is already received
                    if (SM[i].rwnd.filled[seq_no] == 1) {
                        // Message is already received (Dropping Msg)
                        // Sending an ACK for the last in-order message
                        char ack[6];
                        ack[0] = 20;
                        int remaining = (SM[i].rwnd.size - SM[i].rwnd.next_seq_no + SM[i].rwnd.base + 32) % 16;
                        memcpy(ack + 1, &(remaining), 4);
                        ack[5] = (SM[i].rwnd.next_seq_no - 1 + 16) % 16;

                        struct sockaddr_in dest_addr;
                        dest_addr.sin_family = AF_INET;
                        dest_addr.sin_port = htons(SM[i].opp_port);
                        dest_addr.sin_addr.s_addr = inet_addr(SM[i].opp_ip);
                        // printf("R Thread: (Already Received) Sending ACK from socket %d, for seq_no: %d\n", i, ack[5]);
                        // fflush(stdout);
                        sendto(SM[i].udp_id, ack, 6, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
                        total_msg_sent++;
                        sem_post(recv_mtx[i]);
                        continue;
                    }
                    // Checking if the message is in-order
                    if (seq_no == SM[i].rwnd.next_seq_no) {
                        // Message is in-order
                        // Storing the message in the rwnd buffer
                        memset(SM[i].rwnd.msgs[seq_no], 0, 1024);
                        memcpy(SM[i].rwnd.msgs[seq_no], msg_data, n - 1);
                        SM[i].rwnd.msg_len[seq_no] = n - 1;
                        SM[i].rwnd.filled[seq_no] = 1;
                        //Incrementing the next_seq_no to the next empty slot
                        while (SM[i].rwnd.filled[SM[i].rwnd.next_seq_no] == 1) {
                            SM[i].rwnd.next_seq_no = (SM[i].rwnd.next_seq_no + 1) % 16;
                        }

                        // Trying to move the base of the receive window forward (Done by seeing if it is filled, and trying to move that data to the receive buffer)
                        while (SM[i].rwnd.filled[SM[i].rwnd.base] == 1) {
                            // Checking if the receive buffer has space
                            if (SM[i].recv_count < 5) {
                                // printf("R Thread: recv_count: %d, seq_no: %d\n", i, SM[i].recv_count, seq_no);
                                // fflush(stdout);
                                // Receive buffer has space
                                // Moving the message to the receive buffer
                                memset(SM[i].recv_buffer[SM[i].recv_count], 0, 1024);
                                memcpy(SM[i].recv_buffer[SM[i].recv_count], SM[i].rwnd.msgs[SM[i].rwnd.base], SM[i].rwnd.msg_len[SM[i].rwnd.base]);
                                SM[i].recv_length[SM[i].recv_count] = SM[i].rwnd.msg_len[SM[i].rwnd.base];
                                SM[i].rwnd.filled[SM[i].rwnd.base] = 0;
                                SM[i].rwnd.base = (SM[i].rwnd.base + 1) % 16;
                                SM[i].recv_count++;
                            } else {
                                // Receive buffer has no space
                                break;
                            }
                        }

                        // Sending an ACK for this message
                        char ack[6];
                        ack[0] = 20;
                        int remaining = (SM[i].rwnd.size - SM[i].rwnd.next_seq_no + SM[i].rwnd.base + 32) % 16;
                        memcpy(ack + 1, &(remaining), 4);
                        ack[5] = (SM[i].rwnd.next_seq_no - 1 + 16) % 16;
                        struct sockaddr_in dest_addr;
                        dest_addr.sin_family = AF_INET;
                        dest_addr.sin_port = htons(SM[i].opp_port);
                        dest_addr.sin_addr.s_addr = inet_addr(SM[i].opp_ip);
                        // printf("R Thread: (In Order) Sending ACK from socket %d, for seq_no: %d\n", i, ack[5]);
                        // fflush(stdout);
                        sendto(SM[i].udp_id, ack, 6, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
                        total_msg_sent++;
                    } else {
                        // Message is out-of-order
                        // Storing the message in the rwnd buffer
                        memset(SM[i].rwnd.msgs[seq_no], 0, 1024);
                        memcpy(SM[i].rwnd.msgs[seq_no], msg_data, n - 1);
                        SM[i].rwnd.msg_len[seq_no] = n - 1;
                        SM[i].rwnd.filled[seq_no] = 1;

                        // Sending an ACK for the last in-order message
                        char ack[6];
                        ack[0] = 20;
                        int remaining = (SM[i].rwnd.size - SM[i].rwnd.next_seq_no + SM[i].rwnd.base + 32) % 16;
                        memcpy(ack + 1, &(remaining), 4);
                        ack[5] = (SM[i].rwnd.next_seq_no - 1 + 16) % 16;

                        struct sockaddr_in dest_addr;
                        dest_addr.sin_family = AF_INET;
                        dest_addr.sin_port = htons(SM[i].opp_port);
                        dest_addr.sin_addr.s_addr = inet_addr(SM[i].opp_ip);
                        // printf("R Thread: (Out of Order) Sending ACK from socket %d, for seq_no: %d\n", i, ack[5]);
                        // fflush(stdout);
                        sendto(SM[i].udp_id, ack, 6, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
                        total_msg_sent++;
                    }
                }
            }
            sem_post(recv_mtx[i]);
        }

        for (int i = 0; i < N; i++) {
            // Checking each socket for full rwnd
            sem_wait(recv_mtx[i]);
            if (SM[i].status == 1 && SM[i].nospace == 0) {
                int remaining = (SM[i].rwnd.size - SM[i].rwnd.next_seq_no + SM[i].rwnd.base + 32) % 16;
                // printf("R Thread (Second Loop): Socket %d, remaining: %d\n", i, remaining);
                // fflush(stdout);
                if (remaining == 0) {
                    // printf("R Thread (Second Loop): Socket %d, setting nospace\n", i);
                    SM[i].nospace = 1;

                    // Sending an ACK with the remaining rwnd size
                    char ack[6];
                    ack[0] = 20;
                    memcpy(ack + 1, &(remaining), 4);
                    ack[5] = (SM[i].rwnd.next_seq_no - 1 + 16) % 16;
                    
                    struct sockaddr_in dest_addr;
                    dest_addr.sin_family = AF_INET;
                    dest_addr.sin_port = htons(SM[i].opp_port);
                    dest_addr.sin_addr.s_addr = inet_addr(SM[i].opp_ip);
                    // printf("R Thread: (Full rwnd) Sending a ACK from socket %d, for seq_no: %d\n", i, ack[5]);
                    fflush(stdout);
                    sendto(SM[i].udp_id, ack, 6, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
                    total_msg_sent++;
                }
            }
            sem_post(recv_mtx[i]);
        }

        sem_post(shm_mtx);
    }

    pthread_exit(NULL);
}

void * S_main(void * arg) {
    // This is the S thread
    printf("Started Thread S\n");
    struct msocket_t *SM = (struct msocket_t *) arg;
    // Get the mutex for shared memory
    sem_t *shm_mtx = sem_open(SHM_MUTEX_NAME, O_CREAT, 0666, 1);
    if (shm_mtx == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }
    // Getting all 25 mutexes for send buffers
    sem_t *send_mtx[25];
    for (int i = 0; i < N; i++) {
        char name[32];
        sprintf(name, "send_mtx_%d", i);
        send_mtx[i] = sem_open(name, O_CREAT, 0666, 1);
        if (send_mtx[i] == SEM_FAILED) {
            perror("sem_open");
            exit(1);
        }
    }

    while (1) {
        sleep(S_SLEEP);
        // printf("S Thread: Woke up\n");
        // printf("Total Messages Sent: %d\n", total_msg_sent);
        fflush(stdout);

        sem_wait(shm_mtx);
        //Print the send free slot of socket 1
        // printf("S Thread: Send Free Slot of Socket 1: %d\n", SM[1].send_free_slot);
        //Checking for timeout
        for (int i = 0; i < N; i++) {
            sem_wait(send_mtx[i]);
            if (SM[i].status == 1) {
                int time_out_idx = -1;
                // Going through the window and checking for timeout
                for (int j = 0; j < (SM[i].swnd.next_seq_no - SM[i].swnd.base + 16) % 16; j++) {
                    struct timeval now;
                    gettimeofday(&now, NULL);
                    int cur_seq_no = (SM[i].swnd.base + j) % 16;
                    if (now.tv_sec - SM[i].swnd.timeouts[cur_seq_no].tv_sec > T) {
                        // Timeout
                        // printf("Timeout for socket %d, seq_no %d\n", i, cur_seq_no);
                        // fflush(stdout);
                        time_out_idx = cur_seq_no;
                        break;
                    }
                }
                if (time_out_idx != -1) {
                    // Resending all the messages from the timed out message to the end of the window
                    for (int j = 0; j < (SM[i].swnd.next_seq_no - time_out_idx + 16) % 16; j++) {
                        int cur_seq_no = (time_out_idx + j) % 16;
                        // Resending the message, adding cur_seq_no to the first byte of the message
                        char msg[1025];
                        msg[0] = cur_seq_no;
                        memset(msg + 1, 0, 1024);
                        memcpy(msg + 1, SM[i].swnd.msgs[cur_seq_no], SM[i].swnd.msg_len[cur_seq_no]);
                        struct sockaddr_in dest_addr;
                        dest_addr.sin_family = AF_INET;
                        dest_addr.sin_port = htons(SM[i].opp_port);
                        dest_addr.sin_addr.s_addr = inet_addr(SM[i].opp_ip);
                        //Debug
                        // printf("S Thread: Resending the msg: %s\n", msg + 1);
                        sendto(SM[i].udp_id, msg, 1 + SM[i].swnd.msg_len[cur_seq_no], 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
                        total_msg_sent++;

                        //Resetting the timeout
                        struct timeval now;
                        gettimeofday(&now, NULL);
                        SM[i].swnd.timeouts[cur_seq_no] = now;
                    }
                }
            }
            sem_post(send_mtx[i]);
        }
        sem_post(shm_mtx);

        sem_wait(shm_mtx);
        //Checking for messages in the send buffer of each socket
        for (int i = 0; i < N; i++) {
            sem_wait(send_mtx[i]);
            if (SM[i].status == 1) {
                int messages_moved = 0;
                // Moving messages from send buffer to the window and then sending them
                for (int j = 0; j < SM[i].send_free_slot; j++) {
                    if ((SM[i].swnd.next_seq_no - SM[i].swnd.base + 16) % 16 < SM[i].swnd.size) {
                        // printf("Seq No: %d, Base: %d", SM[i].swnd.next_seq_no, SM[i].swnd.base);
                        // Moving the message to the window
                        int cur_seq_no = SM[i].swnd.next_seq_no;
                        memset(SM[i].swnd.msgs[cur_seq_no], 0, 1024);
                        memcpy(SM[i].swnd.msgs[cur_seq_no], SM[i].send_buffer[j], SM[i].send_length[j]);
                        SM[i].swnd.msg_len[cur_seq_no] = SM[i].send_length[j];
                        SM[i].swnd.next_seq_no = (SM[i].swnd.next_seq_no + 1) % 16;
                        messages_moved++;
                        // Sending the message
                        char msg[1025];
                        memset(msg, 0, 1025);
                        msg[0] = cur_seq_no;
                        memcpy(msg + 1, SM[i].send_buffer[j], SM[i].send_length[j]);
                        struct sockaddr_in dest_addr;
                        dest_addr.sin_family = AF_INET;
                        dest_addr.sin_port = htons(SM[i].opp_port);
                        dest_addr.sin_addr.s_addr = inet_addr(SM[i].opp_ip);
                        // printf("S Thread: Sending the msg: %s\n", msg + 1);
                        sendto(SM[i].udp_id, msg, 1 + SM[i].send_length[j], 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
                        total_msg_sent++;
                        // Setting the timeout
                        struct timeval now;
                        gettimeofday(&now, NULL);
                        SM[i].swnd.timeouts[cur_seq_no] = now;
                    } else {
                        // Window is full
                        break;
                    }
                }
                if (messages_moved > 0) {
                    // Removing the messages from the send buffer
                    for (int j = 0; j < SM[i].send_free_slot - messages_moved; j++) {
                        memset(SM[i].send_buffer[j], 0, 1024);
                        memcpy(SM[i].send_buffer[j], SM[i].send_buffer[j + messages_moved], SM[i].send_length[j + messages_moved]);
                        SM[i].send_length[j] = SM[i].send_length[j + messages_moved];
                    }
                }
                SM[i].send_free_slot -= messages_moved;
            }
            sem_post(send_mtx[i]);
        }
        sem_post(shm_mtx);
    }
    pthread_exit(NULL);
}

void * G_main(void * arg) {
    // This is the G thread (Garbage Collector thread)
    printf("Started Thread G\n");
    struct msocket_t *SM = (struct msocket_t *) arg;

    // Getting the mutex for shared memory
    sem_t *shm_mtx = sem_open(SHM_MUTEX_NAME, O_CREAT, 0666, 1);
    if (shm_mtx == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    // Getting all 25 mutexes for send buffers
    sem_t *send_mtx[25];
    for (int i = 0; i < N; i++) {
        char name[32];
        sprintf(name, "send_mtx_%d", i);
        send_mtx[i] = sem_open(name, O_CREAT, 0666, 1);
        if (send_mtx[i] == SEM_FAILED) {
            perror("sem_open");
            exit(1);
        }
    }

    // Getting all 25 mutexes for recv buffers
    sem_t *recv_mtx[25];
    for (int i = 0; i < N; i++) {
        char name[32];
        sprintf(name, "recv_mtx_%d", i);
        recv_mtx[i] = sem_open(name, O_CREAT, 0666, 1);
        if (recv_mtx[i] == SEM_FAILED) {
            perror("sem_open");
            exit(1);
        }
    }

    while (1) {
        sleep(2 * T);
        // printf("G Thread: Woke up\n");
        fflush(stdout);

        sem_wait(shm_mtx);
        // Checking for each socket
        for (int i = 0; i < N; i++) {
            if (SM[i].status == 1) {
                //Checking if owning process is alive
                if (kill(SM[i].pid, 0) == -1) {
                    // Process is dead
                    sem_wait(send_mtx[i]);
                    sem_wait(recv_mtx[i]);
                    
                    printf("G Thread: Process for socket %d is dead\n", i);
                    fflush(stdout);
                    
                    //Closing socket
                    close(SM[i].udp_id);
                    
                    memset(SM + i, 0, sizeof(struct msocket_t));

                    sem_post(recv_mtx[i]);
                    sem_post(send_mtx[i]);
                }
            }
        }
        sem_post(shm_mtx);
    }

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
            SM[i].send_free_slot = 0;
            SM[i].swnd.size = 5;
            SM[i].swnd.base = 0;
            SM[i].swnd.next_seq_no = 0;
            SM[i].rwnd.size = 5;
            SM[i].rwnd.base = 0;
            SM[i].rwnd.next_seq_no = 0;

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
            // printf("BIND: IP: %s, Port: %d\n", src_ip, src_port);
            // fflush(stdout);

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

            memset(&(SM[sock_id]), 0, sizeof(struct msocket_t));

            sem_post(shm_mtx);
            sem_post(sock_creation_mtx);
            continue;
        }
    }

    return 0;
}
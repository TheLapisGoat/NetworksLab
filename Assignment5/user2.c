#include <stdio.h>
#include <arpa/inet.h>
#include "msocket.h"

#define IP_1 "127.0.0.1"
#define Port_1 8080
#define IP_2 "127.0.0.1"
#define Port_2 8081

int main () {
    // User 2 will receive a large file (100KB) from User 1: This file is the pdf of the Assignment Problem Statement
    int domain = AF_INET;
    int type = SOCK_MTP;
    int protocol = 0;

    // Creating a socket
    int socket = m_socket(domain, type, protocol);
    if (socket == -1) {
        printf("Error in creating the socket\n");
    } else {
        printf("Socket created successfully\n");
        printf("Socket ID: %d\n", socket);
    }

    // Bind call
    int ret = m_bind(socket, IP_2, Port_2, IP_1, Port_1);
    if (ret == -1) {
        printf("Error in binding the socket\n");
    } else {
        printf("Socket bound successfully\n");
    }

    // Receiving the file
    FILE *fp = fopen("ReceiverFiles/img.jpg", "wb");
    if (fp == NULL) {
        printf("Error in opening the file\n");
    } else {
        printf("File opened successfully\n");
    }

    int msg_count = 0;
    char buffer[1024];
    int n;
    while (1) {
        memset(buffer, 0, 1024);
        n = m_recvfrom(socket, buffer, 1024, 0, NULL, NULL);
        if (n > 0) {
            // Checking if the received message is the delimiter
            char delimiter[33] = "0123456789ABCDEF0123456789ABCDEF";
            if (memcmp(buffer, delimiter, 32) == 0) {
                printf("Delimiter received. Exiting\n");
                break;
            }
            fwrite(buffer, 1, n, fp);
            fflush(fp);
            msg_count++;
            printf("Message %d received successfully\n", msg_count);
        } else {
            perror("Error");
            sleep(3);
        }
    }

    printf("Waiting for 5 min, then closing the socket\n");
    fflush(stdout);
    sleep(300);

    //Close the socket
    ret = m_close(socket);
    if (ret == -1) {
        printf("Error in closing the socket\n");
    } else {
        printf("Socket closed successfully\n");
    }
    fclose(fp);
    return 0;
}
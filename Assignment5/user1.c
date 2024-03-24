#include <stdio.h>
#include <arpa/inet.h>
#include "msocket.h"

#define IP_1 "127.0.0.1"
#define Port_1 8080
#define IP_2 "127.0.0.1"
#define Port_2 8081

int main () {
    //User 1 will send a large file (100KB) to User 2: This file is the pdf of the Assignment Problem Statement
    int domain = AF_INET;
    int type = SOCK_MTP;
    int protocol = 0;

    // Creating a socket
    int socket = m_socket(domain, type, protocol);
    if (socket == -1) {
        printf("Error in creating the socket\n");
        return 0;
    } else {
        printf("Socket created successfully\n");
        printf("Socket ID: %d\n", socket);
    }

    //Bind call
    int ret = m_bind(socket, IP_1, Port_1, IP_2, Port_2);
    if (ret == -1) {
        printf("Error in binding the socket\n");
        return 0;
    } else {
        printf("Socket bound successfully\n");
    }

    //Opening the pdf file
    FILE *fp = fopen("SenderFiles/img.jpg", "rb");
    if (fp == NULL) {
        printf("Error in opening the file\n");
        return 0;
    } else {
        printf("File opened successfully\n");
    }

    //Sending the file
    int msg_count = 0;
    char buffer[1024];
    int n;
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(Port_2);
    dest_addr.sin_addr.s_addr = inet_addr(IP_2);
    while (1) {
        n = fread(buffer, 1, 1024, fp);
        if (n > 0) {
            int ret;
            //Sending the message
            while ((ret = m_sendto(socket, buffer, n, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr))) == -1) {
                perror("Error");
                sleep(1);
            }
            msg_count++;
            printf("Message %d sent successfully\n", msg_count);
            // printf("Message: %s\n", buffer);
        } else {
            // Sending a 32 byte delimiter to indicate the end of the file
            char delimiter[33] = "0123456789ABCDEF0123456789ABCDEF";
            int ret;
            while ((ret = m_sendto(socket, delimiter, 32, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr))) == -1) {
                perror("Error");
                sleep(1);
            }
            msg_count++;
            printf("EOF delimiter sent successfully\n");
            break;
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
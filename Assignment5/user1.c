#include <stdio.h>
#include <arpa/inet.h>
#include "msocket.h"

int main () {
    int domain = AF_INET;
    int type = SOCK_MTP;
    int protocol = 0;
    int socket = m_socket(domain, type, protocol);
    if (socket == -1) {
        printf("Error in creating the socket\n");
    } else {
        printf("Socket created successfully\n");
        //Print socket id
        printf("Socket ID: %d\n", socket);
    }

    //Bind call
    char src_ip[32] = "127.0.0.1";
    int src_port = 8080;
    char dst_ip[32] = "127.0.0.1";
    int dst_port = 8081;
    int ret = m_bind(socket, src_ip, src_port, dst_ip, dst_port);
    if (ret == -1) {
        printf("Error in binding the socket\n");
    } else {
        printf("Socket bound successfully\n");
    }

    //Send 11 messages
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dst_port);
    dest_addr.sin_addr.s_addr = inet_addr(dst_ip);
    char *msg = "Hello";
    for (int i = 0; i < 11; i++) {
        ssize_t ret = m_sendto(socket, msg, 5, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
        if (ret == -1) {
            printf("Error in sending the message\n");
            perror("Error");
        } else {
            printf("Message %d sent successfully\n", i);
        }
    }

    //Close the socket
    ret = m_close(socket);
    if (ret == -1) {
        printf("Error in closing the socket\n");
    } else {
        printf("Socket closed successfully\n");
    }

    //Create a new socket
    socket = m_socket(domain, type, protocol);
    if (socket == -1) {
        printf("Error in creating the socket\n");
    } else {
        printf("Socket created successfully\n");
        //Print socket id
        printf("Socket ID: %d\n", socket);
    }

    //Bind call
    strcpy(src_ip, "127.0.0.1");
    src_port = 8090;
    strcpy(dst_ip, "127.0.0.1");
    dst_port = 8091;
    ret = m_bind(socket, src_ip, src_port, dst_ip, dst_port);
    if (ret == -1) {
        printf("Error in binding the socket\n");
        perror("Error");
    } else {
        printf("Socket bound successfully\n");
    }
}
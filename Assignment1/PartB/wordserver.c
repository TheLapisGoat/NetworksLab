#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define SERVERPORT 20010    // SERVERPORT is the port number of the server

#define MAXLINE 1024        // MAXLINE is the maximum number of bytes that can be received at once

int main(int argc, char *argv[]) {
    int sockfd, err;                            // sockfd is the socket file descriptor and err is the error code
    struct sockaddr_in servaddr, cliaddr;       // servaddr is the server address and cliaddr is the client address
    int n = 0;                  // n is the number of bytes sent
    socklen_t len;          // len is the length of the client address
    char buffer[MAXLINE];   // buffer is the buffer for the message

    sockfd = socket(PF_INET, SOCK_DGRAM, 0);   // Creating socket file descriptor 
    if (sockfd < 0) {
        perror("socket creation failed");      // perror() prints the error message to stderr
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));     // memset() sets the memory to 0
    memset(&cliaddr, 0, sizeof(cliaddr));       // memset() sets the memory to 0

    servaddr.sin_family = AF_INET;              // AF_INET is the IPv4 protocol
    servaddr.sin_addr.s_addr = INADDR_ANY;      // INADDR_ANY is the IP address of the host
    servaddr.sin_port = htons(SERVERPORT);      // htons() converts the port number from host byte order to network byte order

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");                  // perror() prints the error message to stderr
        exit(EXIT_FAILURE);
    }

    printf("\nThe Server is Running\n");

    while (1) {
        memset(&cliaddr, 0, sizeof(cliaddr));   // Sets the client address to 0
        len = sizeof(cliaddr);
        n = recvfrom(sockfd, (char *) buffer, MAXLINE, 0, (struct sockaddr *) &cliaddr, &len);
        buffer[n] = '\0';

        FILE *file = fopen(buffer, "r");    // fopen() opens the file in read mode
        if (file == NULL) {
            // char errmsg[200] = "NOTFOUND ";
            // strcat(errmsg, buffer);
            sprintf(buffer, "NOTFOUND %s", buffer);
            sendto(sockfd, (const char *) buffer, strlen(buffer), 0, (const struct sockaddr *) &cliaddr, sizeof(cliaddr));
            continue;
        }
        
        fscanf(file, "%s", buffer);
        sendto(sockfd, (const char *) buffer, strlen(buffer), 0, (const struct sockaddr *) &cliaddr, sizeof(cliaddr));

        while (1) {
            n = recvfrom(sockfd, (char *) buffer, MAXLINE, 0, (struct sockaddr *) &cliaddr, &len);
            fscanf(file, "%s", buffer);
            sendto(sockfd, (const char *) buffer, strlen(buffer), 0, (const struct sockaddr *) &cliaddr, sizeof(cliaddr));
            if (strcmp(buffer, "END") == 0) {
                break;
            }
        }

        fclose(file);   // fclose() closes the file
    }

    // //Print client details
    // printf("Client IP: %s\n", inet_ntoa(cliaddr.sin_addr));
    // printf("Client Port: %d\n", ntohs(cliaddr.sin_port));

    close(sockfd);                            // close() closes the socket file descriptor
    return 0;
}

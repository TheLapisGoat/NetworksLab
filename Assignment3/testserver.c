#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVERPORT 20004

int main() {
    int sockfd, newsockfd;
    int clilen;
    int n;      //Number of bytes read
    struct sockaddr_in cli_addr, serv_addr;
    char buf[100];

    memset(buf, 0, sizeof(buf));
    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&cli_addr, 0, sizeof(cli_addr));

    //Opening a socket
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to create socket\n");
        exit(0);
    }

    //Setting up the server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    //Printing the IP address of the server
    printf("IP Address: %s\n", inet_ntoa(serv_addr.sin_addr));

    serv_addr.sin_port = htons(SERVERPORT);

    //Binding the socket to the server address
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("Unable to bind local address\n");
        exit(0);
    }

    //Listening for connections
    listen(sockfd, 5);

    while (1) {
        //Accepting a connection
        clilen = sizeof(cli_addr);
        if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) < 0) {
            perror("Unable to accept connection\n");
            exit(0);
        }
        
        //Forking a process
        if (fork() == 0) {      //Child Process
            //Closing the socket that is listening for connections in the child process
            close(sockfd);

            //Send a multiline 220 response (Say service ready in first line and then the IP address of the server in the second line, according to smtp protocol)
            char * response = "220-Service ready\r\n220\r\n";
            char * ip = inet_ntoa(serv_addr.sin_addr);
            char * response_buf = (char *) malloc(sizeof(char) * (strlen(response) + strlen(ip) + 1));
            sprintf(response_buf, response, ip);
            send(newsockfd, response_buf, strlen(response_buf), 0);

            while (1) {

            }

            //Closing the socket
            close(newsockfd);
            //Exiting the child process
            exit(0);
        }

        close(newsockfd);
    }
}
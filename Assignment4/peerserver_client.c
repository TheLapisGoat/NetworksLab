/*
Roll Number: 21CS10064
Name: Sourodeep Datta
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>

int user_number;

struct user_info {
    int user_id;
    char *ip;
    int port;
};

int max(int a, int b) {
    return a > b ? a : b;
}

struct user_info user_info_table[3] = {
    {1, "127.0.0.1", 50000},
    {2, "127.0.0.1", 50001},
    {3, "127.0.0.1", 50002}
};

int main(int argc, char * argv[]) {
    if (argc != 2) {
        printf("Specify which user you are (1, 2 or 3) as an argument\n");
        exit(0);
    }
    //Get user number from command line argument
    user_number = atoi(argv[1]);
    printf("Peer server started as User %d\n", user_number);

    //Get the port number of the user
    int my_port = user_info_table[user_number - 1].port;

    //Create a socket
    int sockfd;
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Unable to create socket\n");
    exit(0);
    }

    //Bind the socket to the user's port
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(my_port);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    //Bind the socket to the user's port
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("Unable to bind local address\n");
        exit(0);
    }
    
    //Listen for incoming connections
    listen(sockfd, 5);

    fd_set readfds;
    int newsockfds[3];
    newsockfds[0] = -1;
    newsockfds[1] = -1;
    newsockfds[2] = -1;

    //Set all bits of the readfds to 0
    FD_ZERO(&readfds);

    //Set the bit for the sockfd in the readfds
    FD_SET(sockfd, &readfds);

    //Set the bit for the stdin in the readfds
    FD_SET(STDIN_FILENO, &readfds);

    //Find the maximum file descriptor
    int maxfd = max(STDIN_FILENO, sockfd);

    struct timeval tv;

    while (1) {

        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        maxfd = max(sockfd, STDIN_FILENO);
        for (int i = 0; i < 3; i++) {
            if (newsockfds[i] > 0) {
                FD_SET(newsockfds[i], &readfds);
                maxfd = max(maxfd, newsockfds[i]);
            }
        }
        //Setting the timeout for the select
        tv.tv_sec = 300;
        tv.tv_usec = 0;

        int nready = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (nready < 0) {
            perror("select error\n");
            exit(0);
        }

        //If the select times out, then close all friend connections
        if (nready == 0) {
            for (int i = 0; i < 3; i++) {
                if (newsockfds[i] > 0) {
                    close(newsockfds[i]);
                    newsockfds[i] = -1;
                }
            }
            continue;
        }

        //If the sockfd is set in the readfds, then there is a new connection
        if (FD_ISSET(sockfd, &readfds)) {
            struct sockaddr_in client_addr;
            memset(&client_addr, 0, sizeof(client_addr));
            socklen_t client_addr_len = sizeof(client_addr);
            int newsockfd = accept(sockfd, (struct sockaddr *) &client_addr, &client_addr_len);
            if (newsockfd < 0) {
                perror("Unable to accept the connection\n");
                exit(0);
            }

            //Get port of the client
            int client_port = ntohs(client_addr.sin_port);

            //Get the data the client sent
            char buffer[300];
            memset(buffer, 0, 300);
            int n = recv(newsockfd, buffer, 300, 0);

            int user_id = buffer[5] - '0';

            printf("%s\n", buffer);
            fflush(stdout);

            //Store the newsockfd in the newsockfds array
            newsockfds[user_id - 1] = newsockfd;
        }

        //If the stdin is set in the readfds, then there is a new message from the user
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char buffer[300];
            memset(buffer, 0, 300);
            fgets(buffer, 300, stdin);
            buffer[strlen(buffer) - 1] = '\0';

            //Message is of the form: friendname/<msg>

            if (strcmp(buffer, "exit") == 0) {
                for (int i = 0; i < 3; i++) {
                    if (newsockfds[i] > 0) {
                        close(newsockfds[i]);
                    }
                }
                exit(0);
            }

            //Get the friendname: user_1, user_2 or user_3
            int friend_id = buffer[5] - '0';

            //Setting the user_id in the message
            buffer[5] = '0' + user_number;

            //Send the message to the friend
            //Check if the friend is connected
            if (newsockfds[friend_id - 1] > 0) {
                send(newsockfds[friend_id - 1], buffer, strlen(buffer), 0);
            } else {
                //Connecting to the friend
                int friend_port = user_info_table[friend_id - 1].port;
                int friend_sockfd;
                struct sockaddr_in friend_addr;
                memset(&friend_addr, 0, sizeof(friend_addr));

                if ((friend_sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
                    perror("Unable to create socket\n");
                    exit(0);
                }

                friend_addr.sin_family = AF_INET;
                friend_addr.sin_port = htons(friend_port);
                friend_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

                if (connect(friend_sockfd, (struct sockaddr *) &friend_addr, sizeof(friend_addr)) < 0) {
                    perror("Unable to connect to the friend\n");
                    exit(0);
                }

                newsockfds[friend_id - 1] = friend_sockfd;
                send(friend_sockfd, buffer, strlen(buffer), 0);
            }
        }

        //Check for data from client
        for (int i = 0; i < 3; i++) {
            if (newsockfds[i] > 0) {
                if (FD_ISSET(newsockfds[i], &readfds)) {
                    char buffer[300];
                    memset(buffer, 0, 300);
                    int n = recv(newsockfds[i], buffer, 300, 0);
                    if (n == 0) {
                        close(newsockfds[i]);
                        newsockfds[i] = -1;
                        continue;
                    }
                    printf("%s\n", buffer);
                    fflush(stdout);
                }
            }
        }
    }
}
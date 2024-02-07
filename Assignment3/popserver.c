#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

int sockfd, newsockfd;
int clilen;
int SERVERPORT;
struct sockaddr_in cli_addr, serv_addr;

char usernames[100][100];       //Temporarily limiting the number of users to 100
char passwords[100][100];  
int numUsers;
int recipient_exists;

void send_greeting();
void authorization();

int main(int argc, char * argv[]) {
    int n;      //Number of bytes read

    //Checking for the correct number of arguments
    if (argc != 2) {
        printf("Incorrect number of arguments\n");
        exit(0);
    }
    SERVERPORT = atoi(argv[1]);

    //Read user.txt for usernames and passwords, each pair stored in a line, with one or more spaces in between
    FILE * fp = fopen("user.txt", "r");
    if (fp == NULL) {
        printf("Unable to open user.txt\n");
        exit(0);
    }

    numUsers = 0;
    while (fscanf(fp, "%s %s", usernames[numUsers], passwords[numUsers]) != EOF) {
        numUsers++;
    }
    fclose(fp);

    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&cli_addr, 0, sizeof(cli_addr));

    //Opening a socket
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to create socket\n");
        exit(0);
    }

    //Setting up the server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVERPORT);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

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
            continue;
        }
        
        //Forking a process
        if (fork() == 0) {      //Child Process
            //Closing the socket that is listening for connections in the child process
            close(sockfd);

            send_greeting();        //Function that sends the greeting message to the client
            authorization();      //Function that handles the pop3 authorization with the client

            //Closing the socket that is connected to the client in the child process
            close(newsockfd);
            exit(0);
        }
    }       
}

void pop3_quit() {
    send(newsockfd, "+OK POP3 server signing off\r\n", sizeof("+OK POP3 server signing off\r\n"), 0);
    close(newsockfd);
    exit(0);
}

void send_greeting() {  //Function that sends the greeting message to the client
    char greeting[] = "+OK POP3 server ready\r\n";
    send(newsockfd, greeting, strlen(greeting), 0);
}

void authorization() {  //Function that handles the pop3 authorization with the client
    char buffer[1000];
    char complete_line[1000];
    char username[100];
    char password[100];
    memset(buffer, 0, sizeof(buffer));
    memset(complete_line, 0, sizeof(complete_line));
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));

    int state = 0;  //0: Expecting USER, 1: Expecting PASS

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        read(newsockfd, buffer, sizeof(buffer));

        //Check for full line
        strcat(complete_line, buffer);
        char * line_end = strstr(complete_line, "\r\n");
        if (line_end == NULL) {
            continue;
        } else {
            //Removing the \r\n from the end of the line
            *line_end = '\0';
        }

        switch (state) {
            case 0: {     //Expecting USER 
                if (strncmp(complete_line, "USER ", 5) == 0) {
                    sscanf(complete_line, "USER %s", username);

                    //Check if the username exists
                    for (int i = 0; i < numUsers; i++) {
                        if (strcmp(username, usernames[i]) == 0) {
                            state = 1;
                            break;
                        }
                    }

                    //Send response to client
                    if (state == 1) {
                        send(newsockfd, "+OK User Exists\r\n", sizeof("+OK User Exists\r\n"), 0);
                    } else {
                        send(newsockfd, "-ERR User does not exist\r\n", sizeof("-ERR User does not exist\r\n"), 0);
                    }

                } else if (strncmp(complete_line, "QUIT", 4) == 0) {
                    pop3_quit();
                } else {
                    //Invalid command
                    send(newsockfd, "-ERR Invalid command\r\n", sizeof("-ERR Invalid command\r\n"), 0);
                }
                break;
            }
            case 1: {    //Expecting PASS
                if (strncmp(complete_line, "PASS ", 5) == 0) {
                    sscanf(complete_line, "PASS %s", password);

                    //Check if the password is correct
                    for (int i = 0; i < numUsers; i++) {
                        if (strcmp(username, usernames[i]) == 0) {
                            if (strcmp(password, passwords[i]) == 0) {
                                send(newsockfd, "+OK User successfully logged in\r\n", sizeof("+OK User successfully logged in\r\n"), 0);
                                return;
                            }
                        }
                    }

                    //Send response to client
                    send(newsockfd, "-ERR Incorrect password\r\n", sizeof("-ERR Incorrect password\r\n"), 0);
                    state = 0;
                } else if (strncmp(complete_line, "QUIT", 4) == 0) {
                    pop3_quit();
                } else {
                    //Invalid command
                    send(newsockfd, "-ERR Invalid command\r\n", sizeof("-ERR Invalid command\r\n"), 0);
                    state = 0;
                }
                break;
            }
        }

        //Resetting the complete_line buffer
        memset(complete_line, 0, sizeof(complete_line));
    }
}
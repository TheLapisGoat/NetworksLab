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
char* authorization();

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
            char* mailbox_fname = authorization();      //Function that handles the pop3 authorization with the client
            FILE* mailbox = fopen(mailbox_fname, "r");
            if (mailbox_fname != NULL) {
                transaction(mailbox, mailbox_fname);      //Function that handles the pop3 transaction with the client
                update_pop3();      //Function that updates the pop3 server
            }
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

char* authorization() {  //Function that handles the pop3 authorization with the client
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

// TODO: Complete transaction

void transaction(FILE* mailbox, char* mailbox_fname) {  //Function that handles the pop3 transaction with the client
    char buffer[1000];
    char complete_line[1000];
    memset(buffer, 0, sizeof(buffer));
    memset(complete_line, 0, sizeof(complete_line));

    int* to_delete = (int*)malloc(1 * sizeof(int));
    int to_delete_count = 0;

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
        // Handle QUIT first
        if (strncmp(complete_line, "QUIT", 4) == 0) {
            // update the mailbox
            FILE* temp = fopen("temp.txt", "w");
            char line[1000];
            int count = 0;
            // TODO: wtf is this trash try to find a better way to do this
            while (fgets(line, sizeof(line), mailbox) != NULL) {
                if(strncmp(line, ".\r\n", 3) == 0) count++;
                // find if count is in to_delete
                for (int i = 0; i < to_delete_count; i++) {
                    if (to_delete[i] == count) {
                        while (fgets(line, sizeof(line), mailbox) != NULL) {
                            if (strncmp(line, ".\r\n", 3) == 0){
                                count++;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
            fclose(mailbox);
            mailbox = fopen(mailbox_fname, "w");
            fclose(temp);
            temp = fopen("temp.txt", "r");
            while (fgets(line, sizeof(line), temp) != NULL) {
                fprintf(mailbox, "%s", line);
            }
            fclose(temp);
            fclose(mailbox);
            free(to_delete);
            send(newsockfd, "+OK POP3 server signing off\r\n", sizeof("+OK POP3 server signing off\r\n"), 0);

        } 

        else if (strncmp(complete_line, "STAT", 4) == 0) {
            // return +OK followed by number of messages and total size
            int numMessages = 0;
            int totalSize = 0;
            char line[1000];
            // individual messages are separated by a .\r\n TODO: check if this is the case
            while (fgets(line, sizeof(line), mailbox) != NULL) {
                if (strncmp(line, ".\r\n", 3) == 0) numMessages++;
                totalSize += strlen(line);
            }
            printf("+OK %d %o\r\n", numMessages, totalSize);
        }
        
        else if (strncmp(complete_line, "LIST", 4) == 0) {
            int msg_num;
            if (strlen(complete_line) > 5) {
                sscanf(complete_line, "LIST %d", &msg_num);
                // return +OK followed by message number and size
                char line[1000];
                int size = 0;
                int count = 0;
                while (fgets(line, sizeof(line), mailbox) != NULL && count < msg_num) {
                    if (strncmp(line, ".\r\n", 3) == 0) count++;
                    if (count == msg_num - 1) size += strlen(line);
                }
                if (count == msg_num) printf("+OK %d %d\r\n", msg_num, size);
                else printf("-ERR no such message, only %d messages in maildrop\r\n", count);
            } else {
                // return +OK followed by message number and size for each message
                char line[1000];
                int size = 0, total_size = 0;
                int count = 0;
                char to_print[1000];
                while (fgets(line, sizeof(line), mailbox) != NULL) {
                    if (strncmp(line, ".\r\n", 3) == 0) {
                        sprintf(to_print, "+OK %d %d\r\n", count, size);
                        count++;
                        size = 0;
                    } else size += strlen(line);
                    total_size += strlen(line);
                }
                printf("+OK %d messages (%d octets)\r\n", count, total_size);
                printf(to_print); // TODO: check if this is the correct way to print
            }
        }
        else if (strncmp(complete_line, "RETR", 4) == 0) {
            int msg_num;
            if (strlen(complete_line) > 5) {
                sscanf(complete_line, "RETR %d", &msg_num);
                // return +OK followed by message number and size
                char line[1000];
                int size = 0;
                int count = 0;
                while (fgets(line, sizeof(line), mailbox) != NULL && count < msg_num) {
                    if (strncmp(line, ".\r\n", 3) == 0) count++;
                    if (count == msg_num - 1) size += strlen(line);
                }
                if (count == msg_num) {
                    printf("+OK %d %d\r\n", msg_num, size);
                    // return the message
                    while (fgets(line, sizeof(line), mailbox) != NULL && count < msg_num) {
                        if (strncmp(line, ".\r\n", 3) == 0) count++;
                        if (count == msg_num - 1) printf("%s", line); // TODO: check how to do bit stuffing for multiline messages
                    }
                }   
                else printf("-ERR no such message, only %d messages in maildrop\r\n", count);
            }
            else printf("-ERR no message number specified\r\n");
        }
    
        else if (strncmp(complete_line, "DELE", 4) == 0) {
            // need msg_num
            int msg_num;
            
            if (strlen(complete_line) > 5) {
                sscanf(complete_line, "DELE %d", &msg_num);
                // mark the message for deletion
                char line[1000];
                int count = 0;
                // first check if the message has been marked for deletion
                int i = 0;
                for (; i < to_delete_count; i++) {
                    if (to_delete[i] == msg_num) {
                        printf("-ERR message %d already deleted\r\n", msg_num);
                        break;
                    }
                }
                if (i == to_delete_count) continue;

                while (fgets(line, sizeof(line), mailbox) != NULL && count < msg_num) {
                    if (strncmp(line, ".\r\n", 3) == 0) count++;
                    if (count == msg_num - 1) {
                        to_delete = (int*)realloc(to_delete, (to_delete_count + 1) * sizeof(int));
                        to_delete[to_delete_count] = msg_num;
                        to_delete_count++;
                        break;
                    }
                }
                if (count != msg_num) printf("-ERR no such message, only %d messages in maildrop\r\n", count);
            }
            else printf("-ERR no message number specified\r\n");
        }
        else if (strncmp(complete_line, "NOOP", 4) == 0) {
            printf("+OK\r\n");
        }
        else if (strncmp(complete_line, "RSET", 4) == 0) {
            // unmark all messages for deletion
            to_delete = (int*)realloc(to_delete, 1 * sizeof(int));
            to_delete_count = 0;
            printf("+OK\r\n");
        }
        else send(newsockfd, "-ERR Invalid command\r\n", sizeof("-ERR Invalid command\r\n"), 0);

        //Resetting the complete_line buffer
        memset(complete_line, 0, sizeof(complete_line));
    }
}

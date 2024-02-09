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
#include <ctype.h>

int sockfd, newsockfd;
socklen_t clilen;
int SERVERPORT;
struct sockaddr_in cli_addr, serv_addr;

char usernames[100][100];       //Limiting the number of users to 100
char passwords[100][100];  
int numUsers;
int authorized;
char maildrop_fname[200];
int *idx_to_delete;
int to_delete_count;
int num_total_messages;

void send_greeting();
void authorization();
int get_total_message();
void transaction();
void pop3_quit();
void delete_mails();

int main(int argc, char * argv[]) {

    //Checking for the correct number of arguments
    if (argc != 2) {
        printf("Incorrect number of arguments\n");
        printf("Usage: ./popserver <port>\n");
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

            authorized = 0;
            authorization();      //Function that handles the pop3 authorization with the client

            num_total_messages = get_total_message();

            idx_to_delete = (int*) malloc(1 * sizeof(int));
            to_delete_count = 0;
            transaction();

            //Closing the socket that is connected to the client in the child process
            close(newsockfd);
            exit(0);
        }
    }       
}

void pop3_quit() {
    send(newsockfd, "+OK POP3 server signing off\r\n", strlen("+OK POP3 server signing off\r\n"), 0);   //Send response to client
    close(newsockfd);   //Close the socket that is connected to the client

    if (authorized) {
        //Update the mailbox
        delete_mails();
    }
    exit(0);
}

void delete_mails() {
    FILE * mailbox = fopen(maildrop_fname, "r+");
    int writeIndex = 0;     //Offset to write the next line
    int readIndex = 0;      //Offset to read the next line
    int msgNumber = 1;
    char line[1000]; // Adjust the size according to your maximum line length
    memset(line, 0, sizeof(line));

    while (fgets(line, sizeof(line), mailbox)) {
        int removeCurrentLine = 0;
        for (int i = 0; i < to_delete_count; i++) {
            if (msgNumber == idx_to_delete[i]) {
                removeCurrentLine = 1;
                break;
            }
        }

        if (strcmp(line, ".\n") == 0) {
            msgNumber++;
        }

        readIndex = ftell(mailbox);

        if (!removeCurrentLine) {
            fseek(mailbox, writeIndex, SEEK_SET);
            fputs(line, mailbox);
            writeIndex += strlen(line);
        }

        fseek(mailbox, readIndex, SEEK_SET);
    }

    fseek(mailbox, writeIndex, SEEK_SET);
    ftruncate(fileno(mailbox), writeIndex);
    fclose(mailbox);
}

void send_greeting() {  //Function that sends the greeting message to the client
    char greeting[] = "+OK POP3 server ready\r\n";
    send(newsockfd, greeting, strlen(greeting), 0);
}

void authorization() {  //Function that handles the pop3 authorization with the client
    char buffer[1000];
    char complete_line[1000];
    char * username;
    username = (char*) malloc(100 * sizeof(char));
    char password[100];
    int username_len = sizeof(username);
    int password_len = sizeof(password);
    int buffer_len = sizeof(buffer);
    int complete_line_len = sizeof(complete_line);
    memset(buffer, 0, buffer_len);
    memset(complete_line, 0, complete_line_len);
    memset(username, 0, username_len);
    memset(password, 0, password_len);

    int state = 0;  //0: Expecting USER, 1: Expecting PASS

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        recv(newsockfd, buffer, sizeof(buffer), 0);

        //Check for full line
        strcat(complete_line, buffer);
        char * line_end = strstr(complete_line, "\r\n");
        if (line_end == NULL) {
            continue;
        } else {
            //Removing the \r\n from the end of the line
            *line_end = '\0';
        }

        //Make the first 4 characters uppercase
        for (int i = 0; i < 4; i++) {
            complete_line[i] = toupper(complete_line[i]);
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
                        send(newsockfd, "+OK User Exists\r\n", strlen("+OK User Exists\r\n"), 0);
                    } else {
                        send(newsockfd, "-ERR User does not exist\r\n", strlen("-ERR User does not exist\r\n"), 0);
                    }

                } else if (strncmp(complete_line, "QUIT", 4) == 0) {
                    pop3_quit();
                } else {
                    //Invalid command
                    send(newsockfd, "-ERR Invalid command\r\n", strlen("-ERR Invalid command\r\n"), 0);
                }
                break;
            }
            case 1: {    //Expecting PASS
                if (strncmp(complete_line, "PASS ", 5) == 0) {
                    sscanf(complete_line, "PASS %s", password);

                    //Password flag
                    int pass_flag = 0;

                    //Check if the password is correct
                    for (int i = 0; i < numUsers; i++) {
                        if (strcmp(username, usernames[i]) == 0) {
                            if (strcmp(password, passwords[i]) == 0) {
                                pass_flag = 1;

                                //Opening the maildrop
                                sprintf(maildrop_fname, "%s/mymailbox", username);
                                //Testing to see if the maildrop can be opened
                                FILE * maildrop = fopen(maildrop_fname, "r");

                                if (maildrop == NULL) {
                                    send(newsockfd, "-ERR Unable to open maildrop\r\n", strlen("-ERR Unable to open maildrop\r\n"), 0); //Send response to client
                                } else {
                                    send(newsockfd, "+OK User successfully logged in\r\n", strlen("+OK User successfully logged in\r\n"), 0);   //Send response to client
                                    authorized = 1;     //User is authorized
                                    fclose(maildrop);
                                    return;
                                }
                            }
                        }
                    }

                    if (!pass_flag) {
                        send(newsockfd, "-ERR Incorrect password\r\n", strlen("-ERR Incorrect password\r\n"), 0);   //Send response to client
                    }
                    
                    state = 0;
                } else if (strncmp(complete_line, "QUIT", 4) == 0) {
                    pop3_quit();
                } else {
                    //Invalid command
                    send(newsockfd, "-ERR Invalid command\r\n", strlen("-ERR Invalid command\r\n"), 0);
                    state = 0;
                }
                break;
            }
        }

        //Resetting the complete_line buffer
        memset(complete_line, 0, sizeof(complete_line));
    }
}

int get_total_message() {
    FILE * maildrop = fopen(maildrop_fname, "r");
    char line[1000];
    memset(line, 0, sizeof(line));
    int count = 0;
    while (fgets(line, sizeof(line), maildrop) != NULL) {
        if(strncmp(line, ".\n", 2) == 0) count++;
    }
    fclose(maildrop);
    return count;
}

void transaction() {  //Function that handles the pop3 transaction with the client
    char buffer[1000];
    char complete_line[1000];
    memset(buffer, 0, sizeof(buffer));
    memset(complete_line, 0, sizeof(complete_line));

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        recv(newsockfd, buffer, sizeof(buffer), 0);

        //Check for full line
        strcat(complete_line, buffer);
        char * line_end = strstr(complete_line, "\r\n");
        if (line_end == NULL) {
            continue;
        } else {
            //Removing the \r\n from the end of the line
            *line_end = '\0';
        }

        //Make the first 4 characters uppercase
        for (int i = 0; i < 4; i++) complete_line[i] = toupper(complete_line[i]);

        // Handle QUIT first
        if (strncmp(complete_line, "QUIT", 4) == 0) {
            pop3_quit();
        } else if (strncmp(complete_line, "STAT", 4) == 0) {
            // return +OK followed by number of messages and total size
            int msg_size = 0;
            int count = 0;
            int numMessages = 0;
            int totalSize = 0;
            char line[1000];
            memset(line, 0, sizeof(line));
            char response[1000];
            memset(response, 0, sizeof(response));

            //Getting a FILE* to the maildrop
            FILE * maildrop = fopen(maildrop_fname, "r");

            //Reading the maildrop line by line. Individual mails are separated by a ".\n"
            while (fgets(line, sizeof(line), maildrop) != NULL) {
                if (strncmp(line, ".\n", 2) == 0) {
                    count++;

                    //Checking if the message is marked for deletion
                    int valid_msg = 1;
                    for (int i = 0; i < to_delete_count; i++) {
                        if (idx_to_delete[i] == count) {
                            valid_msg = 0;
                            break;
                        }
                    }
                    if (valid_msg) {
                        numMessages++;
                        totalSize += msg_size;
                    }
                    msg_size = 0;
                } else {
                    msg_size += strlen(line);
                }
            }
            
            fclose(maildrop);   //Closing the maildrop

            sprintf(response, "+OK %d %d\r\n", numMessages, totalSize);     //Creating the response

            send(newsockfd, response, strlen(response), 0);     //Sending the response to the client
        } else if (strncmp(complete_line, "LIST", 4) == 0) {
            if (strlen(complete_line) > 5) {    //LIST <msg_num>: return +OK followed by message number and size
                int msg_num;
                sscanf(complete_line, "LIST %d", &msg_num); //Extracting the message number from the command
                // printf("msg_num: %d\n", msg_num);   //Debug                
                //Checking if the message number does not refer to a message marked for deletion
                int valid_msg = 1;
                for (int i = 0; i < to_delete_count; i++) {
                    if (idx_to_delete[i] == msg_num) {
                        send(newsockfd, "-ERR no such message\r\n", strlen("-ERR no such message\r\n"), 0);
                        valid_msg = 0;
                        break;
                    }
                }

                if (!valid_msg) {
                    memset(complete_line, 0, sizeof(complete_line));
                    continue;
                }

                //Getting a FILE* to the maildrop
                FILE * maildrop = fopen(maildrop_fname, "r");

                char line[1000];        //Buffer to store the line read from the maildrop
                memset(line, 0, sizeof(line));
                int size = 0;           //Variable to store the size of the message
                int count = 0;          //Variable to store the number of messages read so far
                int msg_found = 0;      //Flag to check if the message has been found
                
                //Reading the maildrop line by line. Individual mails are separated by a ".\n"
                while (fgets(line, sizeof(line), maildrop) != NULL) {
                    if (strncmp(line, ".\n", 2) == 0) {
                        count++;
                        if (count == msg_num) {
                            msg_found = 1;
                            char response[1000];    //Buffer to store the response. Will return the scan listing of the message
                            memset(response, 0, sizeof(response));
                            sprintf(response, "+OK %d %d\r\n", msg_num, size);     //Creating the response
                            send(newsockfd, response, strlen(response), 0);     //Sending the response to the client
                            break;
                        } else {
                            size = 0;   //Resetting the size
                        }
                    }
                    else size += strlen(line);
                }

                fclose(maildrop);   //Closing the maildrop

                if (!msg_found) {
                    send(newsockfd, "-ERR no such message\r\n", strlen("-ERR no such message\r\n"), 0);
                }
            } else {  //LIST: return +OK followed by message number and size for each message
                send(newsockfd, "+OK\r\n", strlen("+OK\r\n"), 0);     //Sending the response to the client

                //Getting a FILE* to the maildrop
                FILE * maildrop = fopen(maildrop_fname, "r");

                char line[1000];        //Buffer to store the line read from the maildrop
                memset(line, 0, sizeof(line));
                int size = 0;           //Variable to store the size of the message
                int count = 0;          //Variable to store the number of messages read so far
                
                //Reading the maildrop line by line. Individual mails are separated by a ".\n"
                while (fgets(line, sizeof(line), maildrop) != NULL) {
                    if (strncmp(line, ".\n", 2) == 0) {
                        count++;    //Incrementing the count of messages

                        //Checking if the message is marked for deletion
                        int valid_msg = 1;
                        for (int i = 0; i < to_delete_count; i++) {
                            if (idx_to_delete[i] == count) {
                                valid_msg = 0;
                                break;
                            }
                        }
                        if (valid_msg) {
                            char response[1000];    //Buffer to store the response. Will return the scan listing of the message
                            memset(response, 0, sizeof(response));
                            sprintf(response, "%d %d\r\n", count, size);     //Creating the response
                            send(newsockfd, response, strlen(response), 0);     //Sending the response to the client
                        } 
                        size = 0;   //Resetting the size
                    } else size += strlen(line);
                }

                fclose(maildrop);   //Closing the maildrop

                //Sending the end of list marker
                send(newsockfd, ".\r\n", strlen(".\r\n"), 0);
            }
        } else if (strncmp(complete_line, "RETR", 4) == 0) {
            int msg_num;
            
            if (strlen(complete_line) < 5) {
                // printf("-ERR no message number specified\r\n");
                memset(complete_line, 0, sizeof(complete_line));
                continue;
            }
            sscanf(complete_line, "RETR %d", &msg_num); //Extracting the message number from the command
            //Checking if the message number does not refer to a message marked for deletion
            int valid_msg = 1;
            for (int i = 0; i < to_delete_count; i++) {
                if (idx_to_delete[i] == msg_num) {
                    send(newsockfd, "-ERR no such message\r\n", strlen("-ERR no such message\r\n"), 0);
                    valid_msg = 0;
                    break;
                }
            }
            if (!valid_msg) {
                memset(complete_line, 0, sizeof(complete_line));
                continue;
            }
            //Getting a FILE* to the maildrop
            FILE * maildrop = fopen(maildrop_fname, "r");

            char line[1000];        //Buffer to store a line read from the maildrop
            char message[5000];     //Upper limit of message is 4000 bytes, assignment: You can assume that no single line in the message body has more than 80 characters and the maximum no. of lines in the message is 50.
            memset(message, 0, sizeof(message));
            memset(line, 0, sizeof(line));
            int count = 0;          //Variable to store the number of messages read so far
            int msg_found = 0;      //Flag to check if the message has been found

            //Reading the maildrop line by line. Individual mails are separated by a ".\n"
            while (fgets(line, sizeof(line), maildrop) != NULL) {
                if (strncmp(line, ".\n", 2) == 0) {
                    count++;
                    if (count == msg_num) {
                        msg_found = 1;
                        send(newsockfd, "+OK message follows\r\n", strlen("+OK message follows\r\n"), 0);     //Sending the response to the client

                        //Tokenizing the message wrt '\n' and sending each line appended with '\r\n' to the client
                        char * token = strtok(message, "\n");
                        while (token != NULL) {
                            char response[1000];
                            memset(response, 0, sizeof(response));
                            strcpy(response, token);
                            strcat(response, "\r\n");
                            send(newsockfd, response, strlen(response), 0);
                            token = strtok(NULL, "\n");
                        }
                        //Sending the end of message marker
                        send(newsockfd, ".\r\n", strlen(".\r\n"), 0);
                        break;
                    } else {
                        memset(message, 0, sizeof(message));    //Resetting the message
                    }
                } else {
                    strcat(message, line);  //Appending the line to the message
                }
            }
            fclose(maildrop);   //Closing the maildrop
            if (!msg_found) {
                send(newsockfd, "-ERR no such message\r\n", strlen("-ERR no such message\r\n"), 0);
            }
        } else if (strncmp(complete_line, "DELE", 4) == 0) {
            int msg_num;
            
            if (strlen(complete_line) < 5) {
                // printf("-ERR no message number specified\r\n");
                memset(complete_line, 0, sizeof(complete_line));
                continue;
            }
            sscanf(complete_line, "DELE %d", &msg_num); //Extracting the message number from the command
            int valid_msg = 1;
            //Checking if the message number does not refer to a message marked for deletion
            for (int i = 0; i < to_delete_count; i++) {
                if (idx_to_delete[i] == msg_num) {
                    send(newsockfd, "-ERR no such message\r\n", strlen("-ERR no such message\r\n"), 0);
                    valid_msg = 0;
                    break;
                }
            }
            //Checking if the message number is valid
            if (msg_num > num_total_messages || msg_num < 1) {
                send(newsockfd, "-ERR no such message\r\n", strlen("-ERR no such message\r\n"), 0);
                valid_msg = 0;
            }

            if (!valid_msg) {
                memset(complete_line, 0, sizeof(complete_line));
                continue;
            }

            //Adding the message number to the list of messages to be deleted. Here msg_num is 1-indexed.
            idx_to_delete = (int*) realloc(idx_to_delete, (to_delete_count + 1) * sizeof(int));
            idx_to_delete[to_delete_count] = msg_num;
            to_delete_count++;

            send(newsockfd, "+OK message deleted\r\n", strlen("+OK message deleted\r\n"), 0);
        } else if (strncmp(complete_line, "NOOP", 4) == 0) {
            send(newsockfd, "+OK\r\n", strlen("+OK\r\n"), 0);
        } else if (strncmp(complete_line, "RSET", 4) == 0) {
            //Unmark all messages marked for deletion
            idx_to_delete = (int*) realloc(idx_to_delete, 1 * sizeof(int));
            to_delete_count = 0;
            send(newsockfd, "+OK\r\n", strlen("+OK\r\n"), 0);
        } else send(newsockfd, "-ERR Invalid command\r\n", strlen("-ERR Invalid command\r\n"), 0);

        //Resetting the complete_line buffer
        memset(complete_line, 0, sizeof(complete_line));
    }
}

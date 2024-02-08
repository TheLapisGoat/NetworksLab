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

int state;  //0: Waiting for a command, 1: Expected to give a reply
int commd_state;    //0: Waiting for a HELO command, 1: Waiting for a MAIL command, 2: Waiting for a RCPT command, 3: Waiting for a DATA command

char username_sender[80];   //Upper limit of username (local part) is 64 bytes, rfc 5321, pg 63
char domain_sender[300];           //Upper limit of domain is 255 bytes, rfc 5321, pg 63
char username_recipient[80];   //Upper limit of username (local part) is 64 bytes, rfc 5321, pg 63
char domain_recipient[300];           //Upper limit of domain is 255 bytes, rfc 5321, pg 63
char subject[100];           //Upper limit of subject is 50 bytes, assignment
char message[5000];          //Upper limit of message is 4000 bytes, assignment: You can assume that no single line in the message body has more than 80 characters and the maximum no. of lines in the message is 50.

void transaction();             //Function that handles the smtp transaction with the client
void get_command();             //Function that reads a command from the socket
void send_reply();              //Function that sends a reply to the client
void write_to_socket(char * message);   //Function that writes a message to the socket
void get_mail_message();             //Function that reads the mail essage from the socket
void send_mail();               //Function that sends the mail to the recipient
void append_mail();             //Function that appends the mail to the mymailbox file in the user's directory


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

            transaction();      //Function that handles the smtp transaction with the client

            //Closing the socket that is connected to the client in the child process
            close(newsockfd);
            exit(0);
        }
    }       
}

void write_to_socket(char * message) {
    int n = write(newsockfd, message, strlen(message));
    if (n < 0) {
        perror("Unable to write to socket\n");
        exit(0);
    }
}

void transaction() {

    //Sending the 220 message
    char message[512] = "220 <iitkgp.edu> Service ready\r\n";
    write_to_socket(message);

    state = 0;
    commd_state = 0;

    while (1) {
        if (state == 0) {   //Waiting for a command
            get_command();
        } else if (state == 1) {    //Expected to give a reply
            send_reply();
        }
    }
}

void get_command() {
    // The maximum total length of a command line including the command word and the <CRLF> is 512 octets. rfc 5321, pg 63
    char command_line[600];
    memset(command_line, 0, sizeof(command_line));

    //Reading the command from the socket till a <CRLF> is encountered
    while (1) {
        char temp_buff[600];
        memset(temp_buff, 0, sizeof(temp_buff));
        int n = recv(newsockfd, temp_buff, sizeof(temp_buff), 0);      //Reading from the socket
        if (n < 0) {
            perror("Unable to read from socket\n");
            exit(0);
        }

        //Append the buffer to the command buffer
        strcat(command_line, temp_buff);

        //Check for <CRLF> in the buffer
        char *commd_end = strstr(command_line, "\r\n");
        if (commd_end != NULL) {    //Break if <CRLF> is found
            break;
        }
    }

    //Getting the command word
    char command[5];
    memset(command, 0, sizeof(command));
    memcpy(command, command_line, 4);

    //Shifting the command_line buffer to the left by 4 bytes
    memmove(command_line, command_line + 4, strlen(command_line) - 4);

    //Set command to upper case
    for (int i = 0; i < 4; i++) {
        if (command[i] >= 'a' && command[i] <= 'z') {
            command[i] = command[i] - 32;
        }
    }

    if (strcmp(command, "HELO") == 0) {
        if (commd_state == 0) {
            // printf("HELO command received\n");      //For debugging
            state = 1;
        } else {
            printf("HELO command received in an unexpected state\n");
            exit(0);
        }
    } else if (strcmp(command, "MAIL") == 0) {          // MAIL: rfc 821, pg 20; Syntax, rfc 821, pg 29: MAIL <SP> FROM:<reverse-path> <CRLF>
        if (commd_state == 1) {
            // printf("MAIL command received\n");      //For debugging

            //From is of form: MAIL FROM:<username_sender@domain_sender>\r\n
            //Extracting the username_sender and domain_sender
            char * username_start = strstr(command_line, "<");
            char * username_end = strstr(command_line, "@");
            char * domain_start = strstr(command_line, "@");
            char * domain_end = strstr(command_line, ">");
            memcpy(username_sender, username_start + 1, username_end - username_start - 1);
            memcpy(domain_sender, domain_start + 1, domain_end - domain_start - 1);

            state = 1;
        } else {
            printf("MAIL command received in an unexpected state\n");
            exit(0);
        }
    } else if (strcmp(command, "RCPT") == 0) {
        if (commd_state == 2) {
            // printf("RCPT command received\n");      //For debugging

            //To is of form: RCPT TO:<username_recipient@domain_recipient>\r\n
            //Extracting the username_recipient and domain_recipient
            char * username_start = strstr(command_line, "<");
            char * username_end = strstr(command_line, "@");
            char * domain_start = strstr(command_line, "@");
            char * domain_end = strstr(command_line, ">");
            memcpy(username_recipient, username_start + 1, username_end - username_start - 1);
            memcpy(domain_recipient, domain_start + 1, domain_end - domain_start - 1);

            //Checking if the recipient exists
            recipient_exists = 0;
            for (int i = 0; i < numUsers; i++) {
                if (strcmp(username_recipient, usernames[i]) == 0) {
                    recipient_exists = 1;
                    break;
                }
            }

            state = 1;
        } else {
            printf("RCPT command received in an unexpected state\n");
            exit(0);
        }
    } else if (strcmp(command, "DATA") == 0) {
        if (commd_state == 3) {
            // printf("DATA command received\n");      //For debugging

            //Sending a 354 response
            char message[512] = "354 Enter Mail; end with <CRLF>.<CRLF>\r\n";
            write_to_socket(message);

            get_mail_message();     //Function that reads the mail message from the socket
            
            state = 1;
        } else {
            printf("DATA command received in an unexpected state\n");
            exit(0);
        }
    } else if (strcmp(command, "QUIT") == 0) {
        // printf("QUIT command received\n");      //For debugging

        //Sending a 221 response
        char message[512] = "221 <iitkgp.edu> Service closing transmission channel\r\n";
        write_to_socket(message);

        //Closing the socket that is connected to the client in the child process
        close(newsockfd);
        exit(0);
    } else {
        printf("Invalid command received\n");
        exit(0);
    }
}

void get_mail_message() {
    char text_line[1024];       //Buffer to store a full line of text.  The maximum total length of a text line including <CRLF> (not counting the leading dot duplicated for transparency) is 1000 octets. rfc 5321, pg 63
    char buffer[1024];          //Buffer to store a part of a line of text
    memset(text_line, 0, sizeof(text_line));     //Setting the buffer to 0
    memset(buffer, 0, sizeof(buffer));     //Setting the buffer to 0
    memset(message, 0, sizeof(message));     //Setting the buffer to 0

    /* Transparency: rfc 5321, pg 62: Without some provision for data transparency, the character sequence "<CRLF>.<CRLF>" ends the mail text and cannot be sent by the user. Here the first <CRLF> sequence is actually the terminator of the previous line.
    When a line of mail text is received by the SMTP server, it checks the line.  If the line is composed of a single period, it is treated as the end of mail indicator.  If the first character is a period and there are other characters on the line, the first character is deleted. */

    //Reading the message from the socket till a .<CRLF> is encountered
    int continue_reading = 1;

    while (1) {
        int bytes_read = recv(newsockfd, buffer, sizeof(buffer), 0);      //Reading from the socket
        buffer[bytes_read] = '\0';      //Setting the last byte to 0

        //Append the buffer to the text line
        strcat(text_line, buffer);

        char * line_end = strstr(text_line, "\r\n");     //Finding the <CRLF>

        if (line_end != NULL) {     //If the <CRLF> is found
            //Move text_line back to buffer
            strcpy(buffer, text_line);
            memset(text_line, 0, sizeof(text_line));
            line_end = strstr(buffer, "\r\n");     //Finding the <CRLF> in buffer again
        }

        while (line_end != NULL) {      //If the <CRLF> is found
            //Append the buffer to the text line till the <CRLF> (including the <CRLF>)
            int line_len = line_end - buffer + 2;
            strncat(text_line, buffer, line_len);

            //Moving the buffer to the left by line_len bytes
            memmove(buffer, buffer + line_len, sizeof(buffer) - line_len);

            //Check if the line is a single period
            if (strcmp(text_line, ".\r\n") == 0) {
                continue_reading = 0;
                break;
            }
            //Check if the line starts with a period
            if (text_line[0] == '.') {
                //Remove the period
                memmove(text_line, text_line + 1, strlen(text_line) - 1);
            }

            //Replace the <CRLF> with a <LF>
            text_line[strlen(text_line) - 2] = '\n';
            text_line[strlen(text_line) - 1] = '\0';

            //Append the line to the message
            strcat(message, text_line);
            
            //Resetting the text line
            memset(text_line, 0, sizeof(text_line));

            line_end = strstr(buffer, "\r\n");     //Finding the <CRLF>
        }

        if (continue_reading == 0) {    //If the message has ended
            break;
        }
    }
}

void send_reply() {
    switch (commd_state) {
        case 0: {   //Has received a HELO command
            char message[512] = "250 OK Hello <iitkgp.edu>\r\n";
            write_to_socket(message);
            state = 0;
            commd_state = 1;
            break;
        }
        case 1: {   //Has received a MAIL command
            //Returning a 250 OK with message username_sender@domain_sender
            char message[512];
            memset(message, 0, sizeof(message));
            strcat(message, "250 OK Sender <");
            strcat(message, username_sender);
            strcat(message, "@");
            strcat(message, domain_sender);
            strcat(message, ">\r\n");
            write_to_socket(message);

            state = 0;
            commd_state = 2;
            break;
        }
        case 2: {   //Has received a RCPT command
            //Returning a 250 OK with message username_recipient@domain_recipient

            //If recipient does not exist
            if (recipient_exists == 0) {    //If the recipient does not exist
                char message[512] = "550 No such user\r\n";
                write_to_socket(message);

                state = 0;
                commd_state = 2;
                break;
            }

            char message[512];
            memset(message, 0, sizeof(message));
            strcat(message, "250 OK Recipient <");
            strcat(message, username_recipient);
            strcat(message, "@");
            strcat(message, domain_recipient);
            strcat(message, ">\r\n");
            write_to_socket(message);

            state = 0;
            commd_state = 3;
            break;
        }
        case 3: {   //Has received a DATA command
            //Returning a 250 OK
            char message[512] = "250 OK Message accepted for delivery\r\n";
            write_to_socket(message);

            //Append the mail to the mymailbox file in the user's directory
            append_mail();

            state = 0;
            commd_state = 0;
            break;
        }
    }
}

void append_mail() {
    //Opening the mymailbox file in the user's directory
    char filename[100];
    memset(filename, 0, sizeof(filename));
    strcat(filename, username_recipient);
    strcat(filename, "/mymailbox");

    FILE * fp = fopen(filename, "a");
    if (fp == NULL) {
        printf("Unable to open mymailbox file\n");
        exit(0);
    }

    //Count the number of lines in the file
    int num_lines = 0;

    //Appending the mail to the file char by char
    for (int i = 0; i < strlen(message); i++) {
        fputc(message[i], fp);
        if (message[i] == '\n') {
            num_lines++;
        }

        //If the number of lines is 3, add the line "Received: <time at which received, in date : hour : minute>"
        if (num_lines == 3) {
            //Getting the current time
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);

            //Appending the line to the file
            fprintf(fp, "Received: %d-%02d-%02d:%02d:%02d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
            num_lines++;
        }
    }
    //Separation .
    fputc('.', fp);
    fputc('\n', fp);

    fclose(fp);
}
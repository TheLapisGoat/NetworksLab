#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <zconf.h>
#include <arpa/inet.h>
#include <ctype.h>

int smtp_state;      //0: Expecting a response from the server, 1: Expected to send a command to the server
int pop3_state;      //0: Expecting a response from the server, 1: Expected to send a command to the server
int commd_state;    //STMP-    0: Send HELO, 1: Send MAIL FROM, 2: Send RCPT TO, 3: Send DATA, -1: Send QUIT
                    //POP3-    0: Expecting server greeting 1: Send USER, 2: Send PASS, 3: Send STAT, 4: Send LIST, 5: Send RETR, 6: Send DELE, 7: Send QUIT
int sockfd;                                                    //Socket file descriptor

//Following variables are for Option 1: Manage Mail
char username[100];         //The argument of a command can be atmost 40 characters long, rfc 1939, pg 3
char password[100];         //The argument of a command can be atmost 40 characters long, rfc 1939, pg 3 
int mails_count;             //The maximum number of messages in a maildrop
int maildrop_size;              //Size of the maildrop in octets, rfc 1939, pg 6
int * mail_indices;             //Array to store the indices of the mails
int pop3_mail_idx;              //Index of the mail in the maildrop
int received_mail_size;         //Size of the mail in octets, rfc 1939, pg 7
char pop3_username_sender[100];   //Upper limit of username (local part) is 64 bytes, rfc 5321, pg 63
char pop3_domain_sender[300];           //Upper limit of domain is 255 bytes, rfc 5321, pg 63
char pop3_username_recipient[100];   //Upper limit of username (local part) is 64 bytes, rfc 5321, pg 63
char pop3_domain_recipient[300];           //Upper limit of domain is 255 bytes, rfc 5321, pg 63
char pop3_subject[100];           //Upper limit of subject is 50 bytes, assignment
char pop3_timestamp[200];           //Timestamp of the mail
char pop3_message[5000];          //Upper limit of message is 4000 bytes, assignment: You can assume that no single line in the message body has more than 80 characters and the maximum no. of lines in the message is 50.


//Following variables are for Option 2: Send Mail
char username_sender[80];   //Upper limit of username (local part) is 64 bytes, rfc 5321, pg 63
char domain_sender[300];           //Upper limit of domain is 255 bytes, rfc 5321, pg 63
char username_recipient[80];   //Upper limit of username (local part) is 64 bytes, rfc 5321, pg 63
char domain_recipient[300];           //Upper limit of domain is 255 bytes, rfc 5321, pg 63
char subject[100];           //Upper limit of subject is 50 bytes, assignment
char message[5000];          //Upper limit of message is 4000 bytes, assignment: You can assume that no single line in the message body has more than 80 characters and the maximum no. of lines in the message is 50.


void send_mail(char * server_ip, int smtp_port);                //Function to send mail
void get_smtp_response();                                       //Function to process the response from the server.
void send_smtp_command();                                       //Function to send the command to the server
int get_user_mail_input();                                      //Gets input from user for the mail. Returns 0 if the input is valid, 1 otherwise
void send_HELO();                                               //Function to send HELO
void send_MAIL();                                               //Function to send MAIL
void send_RCPT();                                               //Function to send RCPT
void send_DATA();                                               //Function to send DATA
void send_DATA_lines();                                         //Function to send DATA lines
void send_QUIT();                                               //Function to send QUIT
void manage_mail(char * server_ip, int pop3_port);              //Function to manage mail
int get_pop3_greeted();                                         //Function to get the server greeting
int get_authenticated();                                        //Function to authenticate the user
int display_pop3_menu();                                        //Function to display the menu
int get_pop3_response();                                        //Function to process the response from the server.
void send_pop3_command();                                       //Function to send the command to the server
void pop3_quit();                                               //Function to send QUIT

char *strstrip(char *s) {         //Helper function to strip the string of leading and trailing whitespaces
        size_t size;
        char *end;

        size = strlen(s);

        if (!size)
                return s;

        end = s + size - 1;
        while (end >= s && isspace(*end))
                end--;
        *(end + 1) = '\0';

        while (*s && isspace(*s))
                s++;

        return s;
}

int main(int argc, char * argv[]) {
    //Doing an argument check
    if (argc < 4) {
        printf("Insufficient Arguments Provided\n");
        printf("Usage: ./client <server-ip> <smtp-port> <pop3-port>\n");
        exit(0);
    } else if (argc > 4) {
        printf("Too Many Arguments Provided\n");
        printf("Usage: ./client <server-ip> <smtp-port> <pop3-port>\n");
        exit(0);
    }

    char * server_ip = argv[1];         //IP Address of the MailServer machine
    int smtp_port = atoi(argv[2]);      //Port number of the SMTP server
    int pop3_port = atoi(argv[3]);      //Port number of the POP3 server

    memset(username, 0, sizeof(username));     //Setting the buffer to 0
    memset(password, 0, sizeof(password));     //Setting the buffer to 0

    printf("Enter your username: ");
    fgets(username, sizeof(username), stdin);
    strstrip(username);     //Stripping the username of leading and trailing whitespaces
    printf("Enter your password: ");
    fgets(password, sizeof(password), stdin);
    strstrip(password);     //Stripping the password of leading and trailing whitespaces

    while (1) {
        //Displaying the menu
        printf("Enter the number corresponding to the action you want to perform:\n");
        printf("1. Manage Mail\n");
        printf("2. Send Mail\n");
        printf("3. Quit\n");

        int choice;                     //Variable to store the choice of the user
        char temp_choice[10];           //Buffer to store the choice of the user
        memset(temp_choice, 0, sizeof(temp_choice));     //Setting the buffer to 0
        fgets(temp_choice, sizeof(temp_choice), stdin);      //Getting the choice of the user
        strstrip(temp_choice);     //Stripping the choice of leading and trailing whitespaces
        choice = atoi(temp_choice);     //Converting the choice to an integer

        switch (choice) {               //Switch case to perform the action
            case 1: {   //Manage Mail
                manage_mail(server_ip, pop3_port);
                break;
            }
            case 2: {       //Send Mail
                send_mail(server_ip, smtp_port);
                break;
            }
            case 3: {
                exit(0);                //Exiting the program
            }
            default:
                printf("Invalid Choice\n");
        }
    }
}

void send_mail(char * server_ip, int smtp_port) {
    struct sockaddr_in serv_addr;                                   //Server address

    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {           //Creating a socket
        perror("Unable to create socket\n");
        return;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));                       //Setting the server address
    serv_addr.sin_family = AF_INET;                                 //Setting the family to IPv4
    serv_addr.sin_port = htons(smtp_port);                          //Setting the port number
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);   

    if ((connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) {     //Connecting to the server
        perror("Unable to connect to server\n");
        return;
    }

    smtp_state = 0;
    commd_state = 0;

    while (1) {
        if (smtp_state == 0) {                                      //Expecting a response from the server
            get_smtp_response();                              //Get the response
        } else if (smtp_state == 1) {
            send_smtp_command();                              //Send the command
        } else if (smtp_state == -1) {
            break;                                                  //Exiting send_mail()
        } else {
            printf("Unknown smtp state: %d\n", smtp_state);
            return;
        }
    }
}

void get_smtp_response() {                         //Function to process the response from the server.
    /* Definition of a line:  Lines consist of zero or more data characters terminated by the sequence ASCII character "CR" (hex value 0D) followed immediately by ASCII character "LF" (hex value 0A).
    SMTP client MUST NOT send CR or LF unless sending <CRLF> as line terminator. RFC 5321, pg 13 */
    /* All SMTP responses are of 2 forms (RFC 5321, pg 49):
    1. Single Line: <3-digit-code> <SP> <message> <CRLF>
    2. Multi Line: <3-digit-code> <Hyphen> <line>                //I'm assuming by line, they mean the above definition of a line  //Update: RFC 5321 pg 63 has an example of the multiline response, which follows what I've written here.
    Ended by the line: <3-digit-code> <SP> <message> <CRLF>     //servers SHOULD send the <SP> if subsequent text is not sent, but clients MUST be prepared for it to be omitted.
    */
    /*
    The maximum total length of a reply line including the reply code and the <CRLF> is 512 octets. RFC 5321, pg 63
    */

    //Each of the reply lines may not come in a single packet, so we need to keep reading until we get a <CRLF> at the end of the line.

    int response_code = 0;      //Variable to store the response code
    int continue_reading = 1;

    char complete_line[2048];          //Buffer to store a complete line of text (1001 characters is the limit of a text line in an email, rfc 821, Pg 43)
    char buffer[2048];                 //Buffer to store the data (1001 characters is the limit of a text line in an email, rfc 821, Pg 43)
    memset(buffer, 0, sizeof(buffer));     //Setting the buffer to 0
    memset(complete_line, 0, sizeof(complete_line));     //Setting the buffer to 0

    while (1) {
        recv(sockfd, buffer, sizeof(buffer), 0);      //Reading from the socket

        char * line_end = strstr(buffer, "\r\n");     //Finding the <CRLF>

        if (line_end == NULL) {     //If the <CRLF> is not found
            strcat(complete_line, buffer);      //Append the buffer to the complete line
            //Looking for the <CRLF> in complete_line
            line_end = strstr(complete_line, "\r\n");     //Finding the <CRLF>
            if (line_end != NULL) {      //If the <CRLF> is found
                //Moving complete_line back to buffer
                memmove(buffer, complete_line, sizeof(complete_line));
                memset(complete_line, 0, sizeof(complete_line));     //Setting the buffer to 0
                line_end = strstr(buffer, "\r\n");     //Finding the <CRLF> in buffer again
            } else {
                memset(buffer, 0, sizeof(buffer));     //Setting the buffer to 0
                continue;       //Continue reading
            }
        }

        while (line_end != NULL) {      //If the <CRLF> is found
            strncat(complete_line, buffer, line_end - buffer);      //Append the buffer to the complete line
            memmove(buffer, line_end + 2, sizeof(buffer) - (line_end - buffer) - 2);    //Move the remaining data to the start of the buffer

            if (complete_line[3] == '-') {      //If the line is a multi line response
                memset(complete_line, 0, sizeof(complete_line));     //Setting the buffer to 0
                line_end = strstr(buffer, "\r\n");     //Finding the next <CRLF>
                continue;       //Continue reading
            } else {        //If the line is a single line response or it is the last line of a multi line response
                response_code = (complete_line[0] - '0') * 100 + (complete_line[1] - '0') * 10 + (complete_line[2] - '0');  //Calculating the response code
                continue_reading = 0;       //We have read the full message
                break;      //Break out of the loop
            }
        }

        if (!continue_reading) {        //If we have read the full message
            break;      //Break out of the loop
        }
    }

    // //Removing the <CRLF> from the end of the line
    // complete_line[strlen(complete_line) - 1] = '\0';
    // complete_line[strlen(complete_line) - 1] = '\0';

    switch (response_code) {    //Switch case to process the response code
        case 220: {     //Service Ready
            // printf("Service Ready\n");              //For debugging purposes
            smtp_state = 1;     //Expecting a command from the client
            commd_state = 0;    //Send HELO
            break;
        }
        case 221: {     //Service closing transmission channel
            // printf("Service closing transmission channel\n");              //For debugging purposes
            close(sockfd);      //Closing the socket
            smtp_state = -1;     //Exiting send_mail()
            break;
        }
        case 250: {     //Requested mail action okay, completed
            // printf("Requested mail action okay, completed\n");              //For debugging purposes
            smtp_state = 1;     //Expecting a command from the client

            if (commd_state == 0) {             //On a successful HELO, clear the buffers (rfc 821, pg 19)
                memset(username_sender, 0, sizeof(username_sender));     //Setting the buffer to 0
                memset(domain_sender, 0, sizeof(domain_sender));     //Setting the buffer to 0
                memset(username_recipient, 0, sizeof(username_recipient));     //Setting the buffer to 0
                memset(domain_recipient, 0, sizeof(domain_recipient));     //Setting the buffer to 0
                memset(subject, 0, sizeof(subject));     //Setting the buffer to 0
                int ret = get_user_mail_input();
                if (ret == 0) {
                    commd_state = 1;    //Send MAIL FROM
                } else {
                    printf("Incorrect format\n");
                    commd_state = -1;    //Send QUIT (Emergency Exit)
                }
            } else if (commd_state == 1) {      //On a successful MAIL FROM
                commd_state = 2;    //Send RCPT TO
            } else if (commd_state == 2) {
                commd_state = 3;    //Send DATA
            } else if (commd_state == 4) {
                //Now that OK has been received, the rest of the mail is in the hands of the server. rfc 5321, pg 37
                printf("Mail sent successfully\n");
                commd_state = -1;    //Send QUIT
            }
            break;
        }
        case 354: {     //Start mail input; end with <CRLF>.<CRLF>
            // printf("Start mail input; end with <CRLF>.<CRLF>\n");              //For debugging purposes
            smtp_state = 1;     //Expecting a command from the client
            commd_state = 4;    //Send DATA lines
            break;
        }
        case 550: {     //Requested action not taken: mailbox unavailable. We can do 2 things here, either QUIT or continue transmitting the mail. Both are correct decisions afaik. Assignment tells us to print the error, so that is what I'll do.
            // printf("Requested action not taken: mailbox unavailable\n");              //For debugging purposes
            printf("Error in sending mail: %s\n", complete_line);              //Printing the received error
            smtp_state = 1;     //Expecting a command from the client
            commd_state = -1;    //Send QUIT
            break;
        }
        default: {
            printf("Unknown response code: %d\n", response_code);
            smtp_state = 1;
            commd_state = -1;    //Send QUIT
        }
    }
}

void send_smtp_command() {      //Function to send the command to the server
    switch (commd_state) {
        case 0: {       //Send HELO
            // printf("Sending HELO\n");
            // fflush(stdout);
            send_HELO();
            break;
        }
        case 1: {       //Send MAIL
            // printf("Sending MAIL\n");
            // fflush(stdout);
            send_MAIL();
            break;
        }
        case 2: {       //Send RCPT
            // printf("Sending RCPT\n");
            // fflush(stdout);
            send_RCPT();
            break;
        }
        case 3: {       //Send DATA
            // printf("Sending DATA\n");
            // fflush(stdout);
            send_DATA();
            break;
        }
        case 4: {       //Send DATA lines
            // printf("Sending DATA lines\n");
            // fflush(stdout);
            send_DATA_lines();
            break;
        }
        case -1: {      //Send QUIT
            // printf("Sending QUIT\n");
            // fflush(stdout);
            send_QUIT();
            break;
        }
        default: {
            printf("Unknown command state: %d\n", commd_state);
            smtp_state = 1;
            commd_state = -1;    //Send QUIT
        }
    }
}

int get_user_mail_input() {         //Gets input from user for the mail. Returns 0 if the input is valid, 1 otherwise
    char buffer[512];       //Buffer to store the user input, set to have a higher size than any individual input
    memset(buffer, 0, sizeof(buffer));     //Setting the buffer to 0
    printf("Enter the mail in the pre-determined format:\n");

    //Getting "From: <username>@<domain name>"
    fgets(buffer, sizeof(buffer), stdin);
    strstrip(buffer);     //Stripping the input of leading and trailing whitespaces
    if (sscanf(buffer, "From: %255[^@]@%255[^\n]", username_sender, domain_sender) != 2) {          //Assignment: Wherever a space character is shown, one or more spaces can be there
        // printf("Invalid 'From' format\n");          //For debugging purposes
        return 1;
    }
    strstrip(username_sender);     //Stripping the sender's username of leading and trailing whitespaces
    strstrip(domain_sender);     //Stripping the sender's domain of leading and trailing whitespaces
    if (strlen(username_sender) == 0 || strlen(domain_sender) == 0) {
        // printf("Invalid 'From' format\n");          //For debugging purposes
        return 1;
    }

    //Getting "To: <username>@<domain name>"
    fgets(buffer, sizeof(buffer), stdin);
    strstrip(buffer);     //Stripping the input of leading and trailing whitespaces
    if (sscanf(buffer, "To: %255[^@]@%255[^\n]", username_recipient, domain_recipient) != 2) {
        // printf("Invalid 'To' format\n");          //For debugging purposes
        return 1;
    }
    strstrip(username_recipient);     //Stripping the recipient's username of leading and trailing whitespaces
    strstrip(domain_recipient);     //Stripping the recipient's domain of leading and trailing whitespaces
    if (strlen(username_recipient) == 0 || strlen(domain_recipient) == 0) {
        // printf("Invalid 'To' format\n");          //For debugging purposes
        return 1;
    }

    //Getting "Subject: <subject string, max 50 characters>"
    fgets(buffer, sizeof(buffer), stdin);
    strstrip(buffer);     //Stripping the input of leading and trailing whitespaces
    if (sscanf(buffer, "Subject: %100[^\n]", subject) != 1) {
        // printf("Invalid 'Subject' format\n");          //For debugging purposes
        return 1;
    }
    //For the moment, I will not strip the subject or the message. The addresses have to be stripped, because spaces aren't allowed in smtp, but idk about subject and message. Will check
    
    if (strlen(subject) > 50) {             //Checking length of subject string
        // printf("Invalid 'Subject' format (Too long)\n");          //For debugging purposes
        return 1;
    }

    //Getting "<Message body – one or more lines, terminated by a final line with only a fullstop character>"
    strcpy(message, "");        //Setting the message to an empty string
    int line_count = 0;
    while (1) {
        fgets(buffer, sizeof(buffer), stdin);
        if (strcmp(buffer, ".\n") == 0) {     //If the line is a fullstop
            break;      //Break out of the loop
        }

        if (strlen(buffer) > 80) {             //Checking length of message line
            // printf("Invalid 'Message' format (Line too long)\n");          //For debugging purposes
            return 1;
        }

        strcat(message, buffer);        //Append the buffer to the message

        line_count++;
        if (line_count > 50) {          //Checking the number of lines in the message
            // printf("Invalid 'Message' format (Too many lines)\n");          //For debugging purposes
            return 1;
        }
    }

    return 0;
}

void send_HELO() {     //Function to send HELO
    /* HELO: rfc 821, pg 18; Syntax, rfc 821, pg 29: HELO <SP> <domain> <CRLF>*/
    char * hello = "HELO iitkgp.edu\r\n";      //Command to send
    send(sockfd, hello, strlen(hello), 0);      //Sending the command
    smtp_state = 0;     //Expecting a response from the server
}

void send_MAIL() {     //Function to send MAIL
    /* MAIL: rfc 821, pg 20; Syntax, rfc 821, pg 29: MAIL <SP> FROM:<reverse-path> <CRLF>*/
    char mail[1024];        //Buffer to store the command.  The maximum total length of a command line including the command word and the <CRLF> is 512 octets. rfc 5321, pg 63
    memset(mail, 0, sizeof(mail));     //Setting the buffer to 0

    sprintf(mail, "MAIL FROM:<%s@%s>\r\n", username_sender, domain_sender);      //Command to send
    send(sockfd, mail, strlen(mail), 0);      //Sending the command

    smtp_state = 0;     //Expecting a response from the server
}

void send_RCPT() {     //Function to send RCPT
    /* RCPT: rfc 821, pg 20; Syntax, rfc 821, pg 29: RCPT <SP> TO:<forward-path> <CRLF>*/
    char rcpt[1024];        //Buffer to store the command.  The maximum total length of a command line including the command word and the <CRLF> is 512 octets. rfc 5321, pg 63
    memset(rcpt, 0, sizeof(rcpt));     //Setting the buffer to 0

    sprintf(rcpt, "RCPT TO:<%s@%s>\r\n", username_recipient, domain_recipient);      //Command to send
    send(sockfd, rcpt, strlen(rcpt), 0);      //Sending the command

    smtp_state = 0;     //Expecting a response from the server
}

void send_DATA() {     //Function to send DATA
    /* DATA: rfc 5321, pg 36; Syntax, rfc 5321, pg 29: DATA <CRLF>
    An explanation of how the data is actually sent is in send_DATA_lines() */
    char * data = "DATA\r\n";      //Command to send
    send(sockfd, data, strlen(data), 0);      //Sending the command

    smtp_state = 0;     //Expecting a response from the server
}

void send_DATA_lines() {    //Function to send DATA lines
    /* DATA: rfc 5321, pg 36
    The receiver normally sends a 354 response to DATA, and then treats the lines (strings ending in <CRLF> sequences)
    The mail data may contain any of the 128 ASCII character codes, although experience has indicated that use of control characters other than SP, HT, CR, and LF may cause problems and SHOULD be avoided when possible.
    The mail data are terminated by a line containing only a period, that is, the character sequence "<CRLF>.<CRLF>", where the first <CRLF> is actually the terminator of the previous line */
    char text_line[1024];       //Buffer to store the line of text.  The maximum total length of a text line including <CRLF> (not counting the leading dot duplicated for transparency) is 1000 octets. rfc 5321, pg 63
    memset(text_line, 0, sizeof(text_line));     //Setting the buffer to 0

    // message variable consists of multiple lines of text, each line ending with a '\n'. Extract each line one at a time, remove the '\n' and append the <CRLF> to the end of the line. This is send to the server.
    /* Transparency: rfc 5321, pg 62: Without some provision for data transparency, the character sequence "<CRLF>.<CRLF>" ends the mail text and cannot be sent by the user.
    Before sending a line of mail text, the SMTP client checks the first character of the line.  If it is a period, one additional period is inserted at the beginning of the line. */
    
    //Sending From: <username>@<domain name>
    strcpy(text_line, "From: ");
    strcat(text_line, username_sender);
    strcat(text_line, "@");
    strcat(text_line, domain_sender);
    strcat(text_line, "\r\n");
    send(sockfd, text_line, strlen(text_line), 0);      //Sending the line
    // printf("%s", text_line);              //For debugging purposes

    //Sending To: <username>@<domain name>
    strcpy(text_line, "To: ");
    strcat(text_line, username_recipient);
    strcat(text_line, "@");
    strcat(text_line, domain_recipient);
    strcat(text_line, "\r\n");
    send(sockfd, text_line, strlen(text_line), 0);      //Sending the line

    //Sending Subject: <subject string>
    strcpy(text_line, "Subject: ");
    strcat(text_line, subject);
    strcat(text_line, "\r\n");
    send(sockfd, text_line, strlen(text_line), 0);      //Sending the line

    //Sending the message
    memset(text_line, 0, sizeof(text_line));
    char * line = strtok(message, "\n");        //Extracting the first line

    //Sending the lines
    while (line != NULL) {      //While there are still lines left
        strcpy(text_line, line);
        if (text_line[0] == '.') {       //If the line starts with a '.'
            memmove(text_line + 1, text_line, strlen(text_line));     //Move the line to the right by 1 character
            text_line[0] = '.';      //Insert a '.' at the start of the line
        }

        strcat(text_line, "\r\n");       //Append the <CRLF> to the end of the line

        send(sockfd, text_line, strlen(text_line), 0);      //Sending the line
        memset(text_line, 0, sizeof(text_line));     //Setting the buffer to 0

        line = strtok(NULL, "\n");        //Extracting the next line
    }

    //Sending the <CRLF>.<CRLF> line. Note: As mentioned above, the first <CRLF> is actually the terminator of the previous line, so we don't need to append it.
    char * end = ".\r\n";
    send(sockfd, end, strlen(end), 0);      //Sending the line

    smtp_state = 0;     //Expecting a response from the server
}

void send_QUIT() {
    /* QUIT: rfc 5321, pg 40; Syntax, rfc 5321, pg 40: QUIT <CRLF>*/
    char * quit = "QUIT\r\n";      //Command to send
    send(sockfd, quit, strlen(quit), 0);      //Sending the command

    smtp_state = 0;     //Expecting a response from the server
}

void manage_mail(char * server_ip, int pop3_port) {
    struct sockaddr_in serv_addr;                                   //Server address

    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {           //Creating a socket
        perror("Unable to create socket\n");
        return;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));                       //Setting the server address
    serv_addr.sin_family = AF_INET;                                 //Setting the family to IPv4
    serv_addr.sin_port = htons(pop3_port);                          //Setting the port number
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);   

    if ((connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) {     //Connecting to the server
        perror("Unable to connect to server\n");
        return;
    }

    pop3_state = 0;
    commd_state = 0;

    //Get the server greeting
    int greeted = get_pop3_greeted();
    if (!greeted) {
        // printf("Server did not greet\n"); fflush(stdout);             //debug
        pop3_quit();
        return;
    }

    //Do Authentication
    int authenticated = get_authenticated();
    if (!authenticated) {
        // printf("Authentication failed\n"); fflush(stdout);             //debug
        pop3_quit();
        return;
    }

    mail_indices = (int *) malloc(sizeof(int) * 1);      //Allocating memory for the array of mail indices

    while (1) {
        int displayed_pop3_menu = display_pop3_menu();    //Display the menu
        if (!displayed_pop3_menu) {
            // printf("Error in displaying the menu\n"); fflush(stdout);             //debug
            pop3_quit();
            return;
        }

        int choice = -1;
        while (1) {
            printf("Enter mail no. to see (-1 to go back): ");  //Display the prompt

            char temp_choice[10];           //Buffer to store the choice of the user
            memset(temp_choice, 0, sizeof(temp_choice));     //Setting the buffer to 0
            fgets(temp_choice, sizeof(temp_choice), stdin);      //Getting the choice of the user
            strstrip(temp_choice);     //Stripping the choice of leading and trailing whitespaces
            choice = atoi(temp_choice);     //Converting the choice to an integer

            if (choice == -1) {
                pop3_quit();
                return;
            }

            //Check if the choice is a valid mail number
            int valid_choice = 0;
            for (int i = 0; i < mails_count; i++) {
                if (mail_indices[i] == choice) {
                    valid_choice = 1;
                    break;
                }
            }

            if (valid_choice) {
                break;
            } else {
                printf("Mail no. out of range, give again\n");
            }
        }

        //Get the mail
        commd_state = 5;    //Send RETR
        pop3_mail_idx = choice;  //Set the mail index
        send_pop3_command();
        int ret = get_pop3_response();
        if (ret != 1) {
            // printf("Error in getting the mail\n"); fflush(stdout);             //debug
            pop3_quit();
            return;
        }

        //Display the mail
        char display_buffer[1024];
        memset(display_buffer, 0, sizeof(display_buffer));     //Setting the buffer to 0

        //Displaying From
        strcpy(display_buffer, "From: ");
        strcat(display_buffer, pop3_username_sender);
        strcat(display_buffer, "@");
        strcat(display_buffer, pop3_domain_sender);
        strcat(display_buffer, "\n");
        fputs(display_buffer, stdout);

        //Displaying To
        strcpy(display_buffer, "To: ");
        strcat(display_buffer, pop3_username_recipient);
        strcat(display_buffer, "@");
        strcat(display_buffer, pop3_domain_recipient);
        strcat(display_buffer, "\n");
        fputs(display_buffer, stdout);

        //Displaying Subject
        strcpy(display_buffer, "Subject: ");
        strcat(display_buffer, pop3_subject);
        strcat(display_buffer, "\n");
        fputs(display_buffer, stdout);

        //Displaying Timestamp
        strcpy(display_buffer, "Received: ");
        strcat(display_buffer, pop3_timestamp);
        strcat(display_buffer, "\n");
        fputs(display_buffer, stdout);

        //Displaying Message
        fputs(pop3_message, stdout);

        //Get Char
        char next_choice = getchar();
        getchar();

        if (next_choice == 'd') {       //Delete the mail
            commd_state = 6;    //Send DELE
            pop3_mail_idx = choice;  //Set the mail index
            send_pop3_command();
            ret = get_pop3_response();
            if (ret != 1) {
                printf("Error in deleting the mail\n"); fflush(stdout);
                continue;
            }
            printf("Deleted Mail No. %d\n", choice);
        }
    }
}

int get_pop3_greeted() {
    commd_state = 0;    //Expecting server greeting
    return get_pop3_response();
}

int get_authenticated() {
    commd_state = 1;    //Send USER
    send_pop3_command();
    int ret = get_pop3_response();
    if (ret != 1) {
        return 0;
    }

    commd_state = 2;    //Send PASS
    send_pop3_command();
    ret = get_pop3_response();
    return ret;
}

int display_pop3_menu() { //Menu of form: Sl. No. <Sender’s email id> <When received, in date : hour : minute> <Subject>
    //First, get the number of messages in the maildrop
    commd_state = 3;    //Send STAT
    send_pop3_command();
    int ret = get_pop3_response();
    if (ret != 1) {
        return 0;
    }

    //Next, updating the mail_indices array using LIST
    //First, resetting mail_indices
    mail_indices = (int *) realloc(mail_indices, sizeof(int) * mails_count);      //Allocating memory for the array of mail indices
    memset(mail_indices, 0, sizeof(int) * mails_count);     //Setting the buffer to 0

    //Now, getting the full Scan Listing, and using it to update mail_indices
    commd_state = 4;    //Send LIST
    pop3_mail_idx = 0;  //Set the mail index
    send_pop3_command();
    ret = get_pop3_response();
    if (ret != 1) {
        return 0;
    }

    //Creating aligned table headers of the given format
    //Printing the top of the table
    for (int i = 0; i < 183; i++) {
        printf("-");
    }
    printf("\n");
    printf("|%-8s|%-20s%-40s|%-10s%-40s|%-25s%-35s|\n", "Sl. No.", "", "Sender's email id", "", "Received: date : hour : minute", "", "Subject");
    for (int i = 0; i < 183; i++) {
        printf("-");
    }
    printf("\n");

    //Then, for each mail, get the mail details and display them
    for (int i = 0; i < mails_count; i++) {
        commd_state = 5;    //Send RETR
        pop3_mail_idx = mail_indices[i];  //Set the mail index
        send_pop3_command();
        ret = get_pop3_response();
        if (ret != 1) {
            return 0;   //Error in getting the mail
        }

        //Display the mail
        char full_email[600];
        sprintf(full_email, "%s@%s", pop3_username_sender, pop3_domain_sender);
        printf("|%-8d|%-60s|%-50s|%-60s|\n", mail_indices[i], full_email, pop3_timestamp, pop3_subject);
    }

    //If there are no mails, print a message
    if (mails_count == 0) {
        printf("|%-181s|\n", "No mails in the maildrop");
    }
    //Printing the bottom of the table
    for (int i = 0; i < 183; i++) {
        printf("-");
    }
    printf("\n");
    return 1;
}

void pop3_quit() {
    commd_state = 7;    //Send QUIT
    send_pop3_command();
    int successful_quit = get_pop3_response();
    if (!successful_quit) {
        printf("Error in quitting: Some mails marked for deletion may not have been deleted\n"); fflush(stdout);
    }
    close(sockfd);
}


int get_pop3_response() {
    /*  rfc 1939, pg 3 
    All responses are terminated by a CRLF pair. Responses may be up to 512 characters long, including the terminating CRLF. 
    There are currently two status indicators: positive ("+OK") and negative ("-ERR"). Servers MUST send the "+OK" and "-ERR" in upper case.
    Multiline Reponses:
    After sending the first line of the response and a CRLF, any additional lines are sent, each terminated by a CRLF pair. When all lines of the response have been sent, a final line is sent, consisting of a termination octet (decimal code 046, ".") and a CRLF pair.
    Byte Stuffing:
    If any line of the multi-line response begins with the termination octet, the line is "byte-stuffed" by pre-pending the termination octet to that line of the response.
    When examining a multi-line response, the client checks to see if the line begins with the termination octet. If so and if octets other than CRLF follow, the first octet of the line (the termination octet) is stripped away. 
    If so and if CRLF immediately follows the termination character, then the response from the POP server is ended and the line containing ".CRLF" is not considered part of the multi-line response.
    */

    int response_code = 0;      //Variable to store the response code, 1: Positive response, 0: Negative response

    char complete_line[1050];          //Buffer to store a complete line of text (512 characters is the limit of a response, rfc 1939, Pg 3)
    char buffer[1024];                 //Buffer to store the data (512 characters is the limit of a response, rfc 1939, Pg 3)
    memset(buffer, 0, sizeof(buffer));     //Setting the buffer to 0
    memset(complete_line, 0, sizeof(complete_line));     //Setting the buffer to 0

    //Read one line, this will be the first line of the response, containing the response code
    while (1) {
        int bytes_read = recv(sockfd, buffer, sizeof(buffer), 0);      //Reading from the socket
        buffer[bytes_read] = '\0';      //Null terminating the buffer

        //Append the buffer to the complete line
        strcat(complete_line, buffer);

        char * line_end = strstr(complete_line, "\r\n");     //Finding the <CRLF>

        if (line_end == NULL) {     //If the <CRLF> is not found
            memset(buffer, 0, sizeof(buffer));     //Setting the buffer to 0
            continue;       //Continue reading
        } else {
            memset(buffer, 0, sizeof(buffer));     //Setting the buffer to 0
            //Move the data to the right of the CRLF to the start of the buffer
            memmove(buffer, line_end + 2, strlen(line_end) - 2);

            //Remove the CRLF from the end of the line
            complete_line[strlen(complete_line) - 2] = '\0';
            
            if (complete_line[0] == '+') {      //If the response is a positive response
                response_code = 1;      //Positive response
            } else {
                response_code = 0;      //Negative response
            }
            break;
        }
    }

    switch (commd_state) {
        case 0: {      //Expecting a server greeting
            if (response_code == 1) {     //If the response is a positive response
                // printf("Server Greeting: %s\n", complete_line); fflush(stdout);              //debug
                return 1;       //We have been greeted
            } else {     //If the response is a negative response
                // printf("Error in server greeting: %s\n", complete_line); fflush(stdout);              //debug
                return 0;       //We have not been greeted
            }
            break;
        }
        case 1: {       //Expecting a response to USER
            if (response_code == 1) {     //If the response is a positive response
                // printf("User Accepted: %s\n", complete_line); fflush(stdout);              //debug
                return 1;
            } else {     //If the response is a negative response
                // printf("Error in user: %s\n", complete_line); fflush(stdout);              //debug
                return 0;
            }
            break;
        }
        case 2: {       //Expecting a response to PASS
            if (response_code == 1) {     //If the response is a positive response
                // printf("Password Accepted: %s\n", complete_line); fflush(stdout);              //debug
                return 1;
            } else {     //If the response is a negative response
                // printf("Error in password: %s\n", complete_line); fflush(stdout);              //debug
                return 0;
            }
            break;
        }
        case 3: {       //Expecting a response to STAT, of the form +OK nn mm, rfc 1939, pg 6
            if (response_code == 1) {     //If the response is a positive response
                // printf("STAT: %s\n", complete_line); fflush(stdout);              //debug

                //Parsing the response
                char * token = strtok(complete_line, " ");        //Extracting the first token (+OK)
                token = strtok(NULL, " ");        //Extracting the second token (nn)
                mails_count = atoi(token);        //Converting the token to an integer
                // printf("Number of mails: %d\n", mails_count); fflush(stdout);              //debug

                token = strtok(NULL, " ");        //Extracting the third token (mm)
                maildrop_size = atoi(token);        //Converting the token to an integer
                // printf("Size of mails: %d\n", maildrop_size); fflush(stdout);              //debug

                return 1;
            } else {     //If the response is a negative response
                // printf("Error in STAT: %s\n", complete_line); fflush(stdout);              //debug
                return 0;
            }
            break;
        }
        case 4: {       //Expecting a response to LIST, of the form +OK nn mm, rfc 1939, pg 6
            if (pop3_mail_idx == 0) {   //If the mail index is 0, then the response will be multiline, with each line of the form: nn mm
                if (response_code == 1) {     //If the response is a positive response
                    //Getting the indices of the mails
                    int line_counter = 0;
                    int stop_reading = 0;
                    memset(complete_line, 0, sizeof(complete_line));     //Setting the buffer to 0
                    do {
                        strcat(complete_line, buffer);      //Append the buffer to the complete line
                        char * line_end = strstr(complete_line, "\r\n");     //Finding the <CRLF>

                        if (line_end != NULL) {
                            //Move back the complete_line to buffer
                            strcpy(buffer, complete_line);
                            memset(complete_line, 0, sizeof(complete_line));     //Setting the buffer to 0
                            line_end = strstr(buffer, "\r\n");     //Finding the <CRLF> again
                        }

                        while (line_end != NULL) {      //If the <CRLF> is found
                            line_counter++;
                            strncat(complete_line, buffer, line_end - buffer);      //Append the buffer to the complete line (excluding the <CRLF>)
                            memmove(buffer, line_end + 2, sizeof(buffer) - (line_end - buffer) - 2);    //Move the remaining data to the start of the buffer

                            //Checking if the line is the last line of the mail
                            if (strcmp(complete_line, ".") == 0) {     //If the line is a fullstop
                                stop_reading = 1;       //We have read the full message
                                break;      //Break out of the loop
                            }

                            //Getting the mail index
                            char * token = strtok(complete_line, " ");        //Extracting the first token (nn)
                            mail_indices[line_counter - 1] = atoi(token);        //Converting the token to an integer

                            // printf("Received: %s", complete_line); fflush(stdout);              //debug

                            memset(complete_line, 0, sizeof(complete_line));     //Setting the buffer to 0
                            line_end = strstr(buffer, "\r\n");     //Finding the next <CRLF>
                        }

                        if (stop_reading) {     //If we have read the full message
                            break;      //Break out of the loop
                        } else {
                            int bytes_read = recv(sockfd, buffer, sizeof(buffer), 0);      //Reading from the socket
                            buffer[bytes_read] = '\0';      //Null terminating the buffer
                        }

                    } while (1);

                    return 1;
                } else {     //If the response is a negative response
                    // printf("Error in LIST: %s\n", complete_line); fflush(stdout);              //debug
                    return 0;
                }
            } else {    //If the mail index is not 0, then the response will be single line, corresponding to the mail index
                if (response_code == 1) {     //If the response is a positive response
                    // printf("LIST: %s\n", complete_line); fflush(stdout);              //debug

                    //Parsing the response
                    char * token = strtok(complete_line, " ");        //Extracting the first token (+OK)
                    token = strtok(NULL, " ");        //Extracting the second token (nn)
                    token = strtok(NULL, " ");        //Extracting the third token (mm)
                    received_mail_size = atoi(token);        //Converting the token to an integer

                    // printf("Size of mail %d is %d\n", pop3_mail_idx, received_mail_size); fflush(stdout);              //debug

                    return 1;
                } else {     //If the response is a negative response
                    // printf("Error in LIST: %s\n", complete_line); fflush(stdout);              //debug
                    return 0;
                }
            }
        }
        case 5: {       //Expecting a response to RETR, of the form +OK followed by a multiline message, rfc 1939, pg 8
            if (response_code == 1) {     //If the response is a positive response
                // printf("RETR: %s\n", complete_line); fflush(stdout);              //debug

                //Getting the rest of the mail
                int line_counter = 0;
                int stop_reading = 0;
                memset(complete_line, 0, sizeof(complete_line));     //Setting the buffer to 0
                memset(pop3_username_sender, 0, sizeof(pop3_username_sender));     //Setting the buffer to 0
                memset(pop3_domain_sender, 0, sizeof(pop3_domain_sender));     //Setting the buffer to 0
                memset(pop3_username_recipient, 0, sizeof(pop3_username_recipient));     //Setting the buffer to 0
                memset(pop3_domain_recipient, 0, sizeof(pop3_domain_recipient));     //Setting the buffer to 0
                memset(pop3_subject, 0, sizeof(pop3_subject));     //Setting the buffer to 0
                memset(pop3_timestamp, 0, sizeof(pop3_timestamp));     //Setting the buffer to 0
                memset(pop3_message, 0, sizeof(pop3_message));     //Setting the buffer to 0
                do {
                    strcat(complete_line, buffer);      //Append the buffer to the complete line
                    char * line_end = strstr(complete_line, "\r\n");     //Finding the <CRLF>

                    if (line_end != NULL) {
                        //Move back the complete_line to buffer
                        strcpy(buffer, complete_line);
                        memset(complete_line, 0, sizeof(complete_line));     //Setting the buffer to 0
                        line_end = strstr(buffer, "\r\n");     //Finding the <CRLF> again
                    }

                    while (line_end != NULL) {      //If the <CRLF> is found
                        line_counter++;
                        strncat(complete_line, buffer, line_end - buffer);      //Append the buffer to the complete line (excluding the <CRLF>)
                        memmove(buffer, line_end + 2, sizeof(buffer) - (line_end - buffer) - 2);    //Move the remaining data to the start of the buffer

                        //Checking if the line is the last line of the mail
                        if (strcmp(complete_line, ".") == 0) {     //If the line is a fullstop
                            stop_reading = 1;       //We have read the full message
                            break;      //Break out of the loop
                        }

                        //Adding a '\n' to the end of the line
                        strcat(complete_line, "\n");

                        if (line_counter == 1) {    //This is the From line
                            sscanf(complete_line, "From: %255[^@]@%255[^\n]", pop3_username_sender, pop3_domain_sender);          //Extracting the sender's username and domain
                        } else if (line_counter == 2) {    //This is the To line
                            sscanf(complete_line, "To: %255[^@]@%255[^\n]", pop3_username_recipient, pop3_domain_recipient);          //Extracting the recipient's username and domain
                        } else if (line_counter == 3) {    //This is the Subject line
                            sscanf(complete_line, "Subject: %100[^\n]", pop3_subject);          //Extracting the subject
                        } else if (line_counter == 4) {    //This is the Timestamp line
                            sscanf(complete_line, "Received: %100[^\n]", pop3_timestamp);          //Extracting the timestamp
                        } else if (line_counter >= 5) {    //This is the Message line
                            strcat(pop3_message, complete_line);          //Extracting the message and appending it to the message buffer
                        }

                        // printf("Received: %s", complete_line); fflush(stdout);              //debug

                        memset(complete_line, 0, sizeof(complete_line));     //Setting the buffer to 0
                        line_end = strstr(buffer, "\r\n");     //Finding the next <CRLF>
                    }

                    if (stop_reading) {     //If we have read the full message
                        break;      //Break out of the loop
                    } else {
                        int bytes_read = recv(sockfd, buffer, sizeof(buffer), 0);      //Reading from the socket
                        buffer[bytes_read] = '\0';      //Null terminating the buffer
                    }

                } while (1);

                return 1;
            } else {     //If the response is a negative response
                // printf("Error in RETR: %s\n", complete_line); fflush(stdout);              //debug
                return 0;
            }
            break;
        }
        case 6: {       //Expecting a response to DELE, of the form +OK, rfc 1939, pg 9
            if (response_code == 1) {     //If the response is a positive response
                // printf("DELE: %s\n", complete_line); fflush(stdout);              //debug
                return 1;
            } else {     //If the response is a negative response
                // printf("Error in DELE: %s\n", complete_line); fflush(stdout);              //debug
                return 0;
            }
            break;
        }
        case 7: {       //Expecting a response to QUIT, of the form +OK, rfc 1939, pg 10
            if (response_code == 1) {     //If the response is a positive response
                // printf("QUIT: %s\n", complete_line); fflush(stdout);              //debug
                return 1;
            } else {     //If the response is a negative response
                // printf("Error in QUIT: %s\n", complete_line); fflush(stdout);              //debug
                return 0;
            }
            break;
        }
    }
    return response_code;
}

void send_pop3_command() {
    /* rfc 1939, pg 3
    All commands are terminated by a CRLF pair. Keywords and arguments consist of printable ASCII characters. 
    Keywords and arguments are each separated by a single SPACE character. Keywords are three or four characters long. 
    Each argument may be up to 40 characters long.*/
    switch (commd_state) {
        case 1: {       //Send USER, rfc 1939, pg 13
            // printf("Sending USER\n"); fflush(stdout);             //debug

            char pop3_msg[300];        //Buffer to store the command
            memset(pop3_msg, 0, sizeof(pop3_msg));     //Setting the buffer to 0

            sprintf(pop3_msg, "USER %s\r\n", username);      //Command to send
            send(sockfd, pop3_msg, strlen(pop3_msg), 0);      //Sending the command

            break;
        }
        case 2: {       //Send PASS, rfc 1939, pg 14
            char pop3_msg[300];        //Buffer to store the command
            memset(pop3_msg, 0, sizeof(pop3_msg));     //Setting the buffer to 0

            sprintf(pop3_msg, "PASS %s\r\n", password);      //Command to send
            send(sockfd, pop3_msg, strlen(pop3_msg), 0);      //Sending the command

            break;
        }
        case 3: {       //Send STAT, rfc 1939, pg 6
            char pop3_msg[300];        //Buffer to store the command
            
            sprintf(pop3_msg, "STAT\r\n");      //Command to send
            send(sockfd, pop3_msg, strlen(pop3_msg), 0);      //Sending the command

            break;
        }
        case 4: {       //Send LIST, rfc 1939, pg 7
            char pop3_msg[300];        //Buffer to store the command
            
            if (pop3_mail_idx == 0) {       //If the mail index is 0
                sprintf(pop3_msg, "LIST\r\n");      //Command to send
            } else {        //If the mail index is not 0
                sprintf(pop3_msg, "LIST %d\r\n", pop3_mail_idx);      //Command to send
            }
            send(sockfd, pop3_msg, strlen(pop3_msg), 0);      //Sending the command

            break;
        }
        case 5: {       //Send RETR, rfc 1939, pg 8
            char pop3_msg[300];        //Buffer to store the command
            
            sprintf(pop3_msg, "RETR %d\r\n", pop3_mail_idx);      //Command to send

            // printf("pop3_msg: %s\n", pop3_msg); fflush(stdout);             //debug

            send(sockfd, pop3_msg, strlen(pop3_msg), 0);      //Sending the command

            // printf("Sent RETR\n"); fflush(stdout);             //debug

            break;
        }
        case 6: {       //Send DELE, rfc 1939, pg 9
            char pop3_msg[300];        //Buffer to store the command
            
            sprintf(pop3_msg, "DELE %d\r\n", pop3_mail_idx);      //Command to send

            send(sockfd, pop3_msg, strlen(pop3_msg), 0);      //Sending the command

            break;
        }
        case 7: {       //Send QUIT, rfc 1939, pg 10
            char pop3_msg[300];        //Buffer to store the command
            
            sprintf(pop3_msg, "QUIT\r\n");      //Command to send
            send(sockfd, pop3_msg, strlen(pop3_msg), 0);      //Sending the command

            break;
        }
    }
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <zconf.h>
#include <arpa/inet.h>
#include <ctype.h>

int smtp_state;      //0: Expecting a response from the server, 1: Expected to send a command to the server
int commd_state;     //0: Send HELO, 1: Send MAIL FROM, 2: Send RCPT TO, 3: Send DATA, -1: Send QUIT
int sockfd ;                                                    //Socket file descriptor
char username[80];         


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
        exit(0);
    } else if (argc > 4) {
        printf("Too Many Arguments Provided\n");
        exit(0);
    }

    char * server_ip = argv[1];         //IP Address of the MailServer machine
    int smtp_port = atoi(argv[2]);      //Port number of the SMTP server
    int pop3_port = atoi(argv[3]);      //Port number of the POP3 server

    char password[260];         //Don't know the upper limit of password, nor do I know where we will be using it.

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
            case 1: {
                //Manage Mail
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

    char complete_line[1024];          //Buffer to store a complete line of text (1001 characters is the limit of a text line in an email, rfc 821, Pg 43)
    char buffer[1024];                 //Buffer to store the data (1001 characters is the limit of a text line in an email, rfc 821, Pg 43)
    memset(buffer, 0, sizeof(buffer));     //Setting the buffer to 0
    memset(complete_line, 0, sizeof(complete_line));     //Setting the buffer to 0

    while (1) {
        int bytes_read = recv(sockfd, buffer, sizeof(buffer), 0);      //Reading from the socket

        char * line_end = strstr(buffer, "\r\n");     //Finding the <CRLF>

        if (line_end == NULL) {     //If the <CRLF> is not found
            strcat(complete_line, buffer);      //Append the buffer to the complete line
            memset(buffer, 0, sizeof(buffer));     //Setting the buffer to 0
            continue;       //Continue reading
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
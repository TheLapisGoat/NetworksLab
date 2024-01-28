#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

int smtp_state;      //0: Expecting a response from the server, 1: Expected to send a command to the server
int sockfd ;                                                    //Socket file descriptor

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

    char username[260];         //Upper limit of a useful address is 254 characters, https://www.rfc-editor.org/errata/eid1690#:~:text=It%20should%20say%3A,total%20length%20of%20320%20characters.
    char password[260];         //Don't know the upper limit of password, nor do I know where we will be using it.

    printf("Enter your username: ");
    scanf("%s", username);
    printf("Enter your password: ");
    scanf("%s", password);

    while (1) {
        //Displaying the menu
        printf("Enter the number corresponding to the action you want to perform:\n");
        printf("1. Manage Mail\n");
        printf("2. Send Mail\n");
        printf("3. Quit\n");

        int choice;                     //Variable to store the choice of the user
        scanf("%d", &choice);           //Taking the choice of the user

        switch (choice) {               //Switch case to perform the action
            case 1: {
                //Manage Mail
                break;
            }
            case 2: {       //Send Mail
                smtp_state = 0;
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
        exit(0);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));                       //Setting the server address
    serv_addr.sin_family = AF_INET;                                 //Setting the family to IPv4
    serv_addr.sin_port = htons(smtp_port);                          //Setting the port number
    inet_aton(server_ip, &serv_addr.sin_addr);                      //Setting the IP address

    if ((connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) {     //Connecting to the server
        perror("Unable to connect to server\n");
        exit(0);
    }

    while (1) {
        if (smtp_state == 0) {                                      //Expecting a response from the server
            get_smtp_response();                              //Get the response
        } else if (smtp_state == 1) {

        }
    }
}

void get_smtp_response() {                         //Function to process the response from the server.
    /* Definition of a line:  Lines consist of zero or more data characters terminated by the sequence ASCII character "CR" (hex value 0D) followed immediately by ASCII character "LF" (hex value 0A).
    SMTP client MUST NOT send CR or LF unless sending <CRLF> as line terminator. RFC 5321, pg 13 */
    /* All SMTP responses are of 2 forms (RFC 5321, pg 49):
    1. Single Line: <3-digit-code> <SP> <message> <CRLF>
    2. Multi Line: <3-digit-code> <Hyphen> <line>                //I'm assuming by line, they mean the above definition of a line
    Ended by the line: <3-digit-code> <SP> <message> <CRLF>     //servers SHOULD send the <SP> if subsequent text is not sent, but clients MUST be prepared for it to be omitted.
    */
    /*
    The maximum total length of a reply line including the reply code and the <CRLF> is 512 octets. RFC 5321, pg 62
    */

    //Each of the reply lines may not come in a single packet, so we need to keep reading until we get a <CRLF> at the end of the line.

    int response_code = 0;      //Variable to store the response code
    int continue_reading = 1;

    char complete_line[1024];          //Buffer to store a complete line of text (1001 characters is the limit of a text line in an email, rfc 821, Pg 43)
    char buffer[1024];                 //Buffer to store the data (1001 characters is the limit of a text line in an email, rfc 821, Pg 43)
    memset(buffer, 0, sizeof(buffer));     //Setting the buffer to 0
    memset(complete_line, 0, sizeof(complete_line));     //Setting the buffer to 0

    while (1) {
        int bytes_read = read(sockfd, buffer, sizeof(buffer));     //Reading the data from the socket

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
                break;      //Break out of the loop
            }
        }

        if (!continue_reading) {        //If we have read the full message
            break;      //Break out of the loop
        }
    }

}
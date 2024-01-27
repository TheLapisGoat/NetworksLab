#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

int smtp_state;      //0: Expecting a response from the server, 1: Expected to send a command to the server

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
    int sockfd ;                                                    //Socket file descriptor
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

    char buffer[1024];                                              //Buffer to store the data (1001 characters is the limit of a text line in an email, Pg 43)

    while (1) {
        if (smtp_state == 0) {       //Expecting a response from the server
            

        } else if (smtp_state == 1) {

        }
    }
}
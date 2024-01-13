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

            //Creating a temporary file to store the file that will be received from the client
            char dec_filename[100];
            inet_ntop(AF_INET, &cli_addr.sin_addr, dec_filename, 100);
            sprintf(dec_filename, "%s.%d.txt", dec_filename, ntohs(cli_addr.sin_port));
            int dec_fd;
            dec_fd = open(dec_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);  //0644 is the permission for the file, where the owner can read / write and all others can only read.

            //Receiving data from the client
            int k = 0;
            int neg = -1;
            while ((n = recv(newsockfd, buf, 99, 0)) > 0) {
                buf[n] = '\0';  //The last character of the message will be '\0'
                //The first few characters of a message will be a part of k if they are digits.
                int i;
                for (i = 0; i < n; i++) {
                    if (neg == -1) {
                        if (buf[i] == '-') {
                            neg = 1;
                            continue;
                        } else {
                            neg = 0;
                        }
                    }
                    if (buf[i] >= '0' && buf[i] <= '9') {
                        k = k * 10 + (buf[i] - '0');
                    } else {
                        break;
                    }
                }
                //The rest of the message will be part of the file, and will be written to the temporary file.
                write(dec_fd, &buf[i], n - i);
                if (buf[n - 1] == '\n') {
                    break;
                }
            }
            if (neg == 1) {
                k = -k;
            }
            close(dec_fd);

            //Encrypting the file
            dec_fd = open(dec_filename, O_RDONLY);  //Opening the file in read-only mode
            char enc_filename[100];
            sprintf(enc_filename, "%s.enc", dec_filename);
            int enc_fd = open(enc_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);    //Opening the encrypted file in write-only mode

            memset(buf, 0, sizeof(buf));
            while ((n = read(dec_fd, buf, 100)) > 0) {
                for (int i = 0; i < n; i++) {
                    if (buf[i] >= 'a' && buf[i] <= 'z') {
                        buf[i] = (buf[i] - 'a' + k) % 26 + 'a';
                    } else if (buf[i] >= 'A' && buf[i] <= 'Z') {
                        buf[i] = (buf[i] - 'A' + k) % 26 + 'A';
                    }
                }
                write(enc_fd, buf, n);
                memset(buf, 0, sizeof(buf));
            }
            close(dec_fd);
            close(enc_fd);

            //Sending the encrypted file to the client
            enc_fd = open(enc_filename, O_RDONLY);
            memset(buf, 0, sizeof(buf));
            while ((n = read(enc_fd, buf, 99)) > 0) {
                buf[n] = '\0';
                send(newsockfd, buf, n, 0);
                memset(buf, 0, sizeof(buf));
            }
            close(enc_fd);

            //Deleting the temporary files
            remove(dec_filename);
            remove(enc_filename);

            //Closing the socket
            close(newsockfd);
            //Exiting the child process
            exit(0);
        }

        close(newsockfd);
    }
}
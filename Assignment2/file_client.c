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
    char buf[100], dec_filename[100];   //Buffer to store the file contents and the file name
    int fd;             //File descriptor
    int k;              //k for Caesar cipher
    int n;              //Number of bytes read
    int sockfd;         //Socket file descriptor
    struct sockaddr_in serv_addr;
    
    while (1) {
        memset(buf, 0, sizeof(buf));
        memset(dec_filename, 0, sizeof(dec_filename));
        memset(&serv_addr, 0, sizeof(serv_addr));
        //Getting the file name and looking for it in the client's directory
        printf("Enter the file name:\n");
        do {
            scanf("%s", dec_filename);
            fd = open(dec_filename, O_RDONLY);
            if (fd == -1) {
                printf("File not found. Please enter a valid file name:\n");
            } else {
                break;
            }
        } while (1);

        //Getting the value of k
        printf("Enter the value of k:\n");
        scanf("%d", &k);

        //Opening a socket
        if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Unable to create socket\n");
            exit(0);
        }

        //Setting up the server address
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(SERVERPORT);
        inet_aton("127.0.0.1", &serv_addr.sin_addr); 

        //Connecting to the server
        if ((connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) {
            perror("Unable to connect to server\n");
            exit(0);
        }

        //Sending k to the server
        sprintf(buf, "%d", k);
        send(sockfd, buf, strlen(buf), 0);

        //Sending the file to the server
        memset(buf, 0, sizeof(buf));
        while ((n = read(fd, buf, 99)) > 0) {
            buf[n] = '\0';
            send(sockfd, buf, strlen(buf), 0);
            memset(buf, 0, sizeof(buf));
        }
        close(fd);

        //Creating the file to store the encrypted file to be received from the server
        char enc_filename[100];
        sprintf(enc_filename, "%s.enc", dec_filename);
        int enc_fd = open(enc_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        //Receiving the encrypted file from the server and writing it to the file
        memset(buf, 0, sizeof(buf));
        while ((n = recv(sockfd, buf, 99, 0)) > 0) {
            buf[n] = '\0';
            write(enc_fd, buf, strlen(buf));
            memset(buf, 0, sizeof(buf));
            if (buf[n - 1] == '\n') {
                write(enc_fd, "\n", 1);
                break;
            }
        }
        close(enc_fd);

        printf("The file %s has been encrypted and stored in %s\n", dec_filename, enc_filename);
        //Closing the socket
        close(sockfd);
    }

    return 0;
}
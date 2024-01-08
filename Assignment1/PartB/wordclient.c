#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>


#define SERVERPORT 20010
#define CLIENTPORT 20011

#define MAXLINE 1024

int main(int argc, char *argv[]) {
    //Requires the file name of the file needed from server.
    if (argc < 3) {
        fprintf(stderr, "Error: Missing Arguments\n");
        exit(EXIT_FAILURE);
    } else if (argc > 3) {
        fprintf(stderr, "Error: Invalid Number of Arguments\n");
        exit(EXIT_FAILURE);
    } else if (strcmp(argv[1], argv[2]) == 0) {
        fprintf(stderr, "Error: Same File Name\n");
        exit(EXIT_FAILURE);
    }

    char *filename = argv[1];       // filename is the name of the file needed from server
    char *newfilename = argv[2];    // newfilename is the name of the file created by the client
    char buffer[MAXLINE];        // buffer is the buffer for the message
    int sockfd, err;        // sockfd is the socket file descriptor and err is the error code
    struct sockaddr_in servaddr, cliaddr;    // servaddr is the server address and cliaddr is the client address
    int n;      // n is the number of bytes received
    socklen_t len;     // len is the length of the server address
    
    sockfd = socket(PF_INET, SOCK_DGRAM, 0);     // Creating socket file descriptor
    if (sockfd < 0) {                          
        perror("socket creation failed");       // perror() prints the error message to stderr
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));     // memset() sets the memory to 0 

    servaddr.sin_family = AF_INET;      // AF_INET is the IPv4 protocol
    servaddr.sin_port = htons(SERVERPORT);    // htons() converts the port number from host byte order to network byte order
    servaddr.sin_addr.s_addr = INADDR_ANY;      // INADDR_ANY is the IP address of the host

    cliaddr.sin_family = AF_INET;       // AF_INET is the IPv4 protocol
    cliaddr.sin_port = htons(20011);     // htons() converts the port number from host byte order to network byte order
    cliaddr.sin_addr.s_addr = INADDR_ANY;    // INADDR_ANY is the IP address of the host

    // Bind the socket with the client address
    if (bind(sockfd, (const struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0) {
        perror("bind failed");     // perror() prints the error message to stderr
        exit(EXIT_FAILURE);
    }

    err = sendto(sockfd, (const char *) filename, strlen(filename), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));    // sendto() sends the filename to the server
    if (err < 0) {
        perror("Failed to send filename");        // perror() prints the error message to stderr
        exit(EXIT_FAILURE);
    }

    n = recvfrom(sockfd, (char *) buffer, MAXLINE, 0, NULL, NULL);
    buffer[n] = '\0';

    if (strcmp((const char *) buffer, "HELLO") != 0) {         // If file is not found, exit
        fprintf(stderr, "File %s Not Found\n", filename);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    //Create a new file with a different name from the file requested from server
    FILE *fp = fopen(newfilename, "w");    // fopen() creates a new file and opens it in write mode 
    fprintf(fp, "%s\n", buffer);    // fprintf() writes the contents of buffer to the file (In this case, "HELLO")

    int count = 1;
    while (1) {
        sprintf(buffer, "WORD%d", count);
        sendto(sockfd, (const char *) buffer, strlen(buffer), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));        // sendto() sends the WORDi message to the server
        n = recvfrom(sockfd, (char *) buffer, MAXLINE, 0, NULL, NULL);
        buffer[n] = '\0';

        //If the received word is END, exit
        if (strcmp((const char *) buffer, "END") == 0) {
            fprintf(fp, "%s", buffer);
            break;
        }

        //Write the received word to file
        fprintf(fp, "%s\n", buffer);

        count++;
    }

    fclose(fp);
    close(sockfd);      // close() closes the socket file descriptor
    return 0;
}
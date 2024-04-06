#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <pthread.h>

#define DROP_PROBABILITY 0.2

int dropmessage(float prob) {
    float random = (float)rand() / (float)RAND_MAX;
    if (random < prob) {
        return 1;
    }
    return 0;
}

int raw_socket;

void set_bit(char * byte_array, int bit_position) {
    byte_array[bit_position / 8] |= 1 << (7 - bit_position % 8);
}

void clear_bit(char * byte_array, int bit_position) {
    byte_array[bit_position / 8] &= ~(1 << (7 - bit_position % 8));
}

int get_bit(char * byte_array, int bit_position) {
    return (byte_array[bit_position / 8] >> (7 - bit_position % 8)) & 1;
}

void * tmain(void * args) {
    char *** t_args = (char ***) args;
    unsigned char number_of_queries = *(t_args[1][0]);
    unsigned short ID;
    memcpy(&ID, t_args[1][1], sizeof(short));
    unsigned char third_byte = *(t_args[1][2]);
    char ** queries = t_args[0];
    struct iphdr *ip_hdr = (struct iphdr *) t_args[1][3];

    //Creating Response Packet: First 16 bits is ID, next 1 bit is msg type, and next 3 bits is number of queries, and next 33 bits per response (1 bit for valid bit, 32 bits for response data)
    unsigned int response_len = 16 + 1 + 3 + 33 * number_of_queries;
    response_len = (response_len + 7) / 8;
    char *response_packet = (char *) malloc(response_len);
    memset(response_packet, 0, response_len);

    *((unsigned short *)response_packet) = htons(ID);
    response_packet[2] = third_byte;
    set_bit(response_packet, 16);

    int bit_position = 20;

    for (int i = 0; i < number_of_queries; i++) {
        //Getting ip address of the query
        struct hostent *host = gethostbyname(queries[i]);
        if (host == NULL) {
            //Setting valid bit to 0
            clear_bit(response_packet, bit_position);
            bit_position += 33;
        } else {
            //Setting valid bit to 1
            set_bit(response_packet, bit_position);
            bit_position++;
            //Setting response data
            unsigned int ip_addr = ntohl(*((unsigned int *)host->h_addr_list[0]));
            for (int j = 0; j < 32; j++) {
                int cur_bit = get_bit((char *)&ip_addr, j);
                if (cur_bit) {
                    set_bit(response_packet, bit_position);
                } else {
                    clear_bit(response_packet, bit_position);
                }
                bit_position++;
            }
        }
    }

    //Sending the response packet using the raw socket
    char response_buffer[ETH_FRAME_LEN];
    memset(response_buffer, 0, ETH_FRAME_LEN);

    struct ethhdr *response_eth_hdr = (struct ethhdr *)response_buffer;
    struct iphdr *response_ip_hdr = (struct iphdr *)(response_buffer + sizeof(struct ethhdr));

    response_eth_hdr->h_proto = htons(ETH_P_IP);
    response_ip_hdr->version = 4;
    response_ip_hdr->ihl = 5;
    response_ip_hdr->tos = 0;
    response_ip_hdr->tot_len = htons(sizeof(struct iphdr) + response_len);
    response_ip_hdr->id = htons(0);
    response_ip_hdr->frag_off = 0;
    response_ip_hdr->ttl = 64;
    response_ip_hdr->protocol = 254;
    response_ip_hdr->check = 0;
    response_ip_hdr->saddr = inet_addr("127.0.0.1");
    response_ip_hdr->daddr = ip_hdr->saddr;

    memcpy(response_buffer + sizeof(struct ethhdr) + sizeof(struct iphdr), response_packet, response_len);

    struct sockaddr_ll dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sll_family = AF_PACKET;
    dest_addr.sll_protocol = htons(ETH_P_ALL);
    dest_addr.sll_ifindex = if_nametoindex("wlan0");
    if (sendto(raw_socket, response_buffer, sizeof(struct ethhdr) + sizeof(struct iphdr) + response_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        perror("Packet send failed");
    }

    // printf("Sent response packet\n");
}

int main() {
    // Open a raw socket
    raw_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (raw_socket < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // Bind to wlan0 interface
    struct sockaddr_ll saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sll_family = AF_PACKET;
    saddr.sll_protocol = htons(ETH_P_ALL);
    saddr.sll_ifindex = if_nametoindex("wlan0");
    if (bind(raw_socket, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    // Setting ip_address to loopback address
    struct sockaddr_in *ip_address = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    ip_address->sin_addr.s_addr = inet_addr("127.0.0.1");

    // Receive and process Ethernet frames
    char buffer[ETH_FRAME_LEN];
    struct ethhdr *eth_hdr;
    struct iphdr *ip_hdr;

    while (1) {
        ssize_t bytes_received = recvfrom(raw_socket, buffer, sizeof(buffer), 0, NULL, NULL);
        if (bytes_received < 0) {
            perror("Packet receive failed");
            continue;
        }

        eth_hdr = (struct ethhdr *)buffer;

        // Check if the Ethernet frame contains an IP packet
        if (ntohs(eth_hdr->h_proto) != ETH_P_IP) {
            continue;
        }

        ip_hdr = (struct iphdr *)(buffer + sizeof(struct ethhdr));

        // Check if the destination IP address matches the IP address of the wlan0 interface
        if (ip_hdr->daddr != ip_address->sin_addr.s_addr) {
            continue;
        }

        // //Print ip_hdr->daddr
        // printf("Destination IP address: %s\n", inet_ntoa(*(struct in_addr *)&ip_hdr->daddr));

        // Check if the protocol field is 254
        if (ip_hdr->protocol != 254) {
            continue;
        }

        //Now getting the simDNS packet
        char *simDNS_packet = buffer + sizeof(struct ethhdr) + sizeof(struct iphdr);

        // printf("Received simDNS packet: %s\n", simDNS_packet);

        //First 16 bits is ID
        unsigned short ID = ntohs(*(unsigned short *)simDNS_packet);

        //Next bit is Message Type, with following 3 bits being Number of Queries
        unsigned char third_byte = *(simDNS_packet + 2);
        unsigned char message_type = (third_byte & 0x80) >> 7;

        if (message_type != 0) {
            continue;
        }

        //Simulating Message drop
        if (dropmessage(DROP_PROBABILITY)) {
            printf("Dropped a packet\n");
            continue;
        }

        unsigned char number_of_queries = (third_byte & 0x70) >> 4;

        // printf("ID: %hu\n", ID);
        // printf("Message Type: %hhu\n", message_type);
        // printf("Number of Queries: %hhu\n", number_of_queries);

        char ** queries = (char **) malloc(number_of_queries * sizeof(char *));
        int bit_position = 20;

        //Reading queries
        for (int i = 0; i < number_of_queries; i++) {
            //First 4 bytes are query length
            unsigned int query_length = 0;
            for (int j = 0; j < 32; j++) {
                int cur_bit = get_bit(simDNS_packet, j + bit_position);
                if (cur_bit) {
                    set_bit((char *)&query_length, j);
                } else {
                    clear_bit((char *)&query_length, j);
                }
            }
            query_length = ntohl(query_length);
            bit_position += 32;

            //Next query_length bytes are query (domain name)
            queries[i] = (char *) malloc(query_length + 1);
            for (int j = 0; j < query_length * 8; j++) {
                int cur_bit = get_bit(simDNS_packet, j + bit_position);
                if (cur_bit) {
                    set_bit(queries[i], j);
                } else {
                    clear_bit(queries[i], j);
                }
            }
            bit_position += query_length * 8;

            // printf("Query %d: %s\n", i + 1, queries[i]);
        }

        //Creating args for thread
        char *** t_args = (char ***) malloc(2 * sizeof(char **));
        t_args[0] = queries;
        t_args[1] = (char **) malloc(4 * sizeof(char *));
        t_args[1][0] = (char *) malloc(sizeof(char));
        t_args[1][1] = (char *) malloc(sizeof(short));
        t_args[1][2] = (char *) malloc(sizeof(char));
        t_args[1][3] = (char *) malloc(sizeof(struct iphdr));

        *(t_args[1][0]) = number_of_queries;
        memcpy(t_args[1][1], &ID, sizeof(short));
        *(t_args[1][2]) = third_byte;
        memcpy(t_args[1][3], ip_hdr, sizeof(struct iphdr));

        //Creating a detached thread to send the response packet
        pthread_t thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&thread, &attr, tmain, (void *) t_args);
    }

    close(raw_socket);
    return 0;
}
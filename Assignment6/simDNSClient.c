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
#include <sys/select.h>
#include <sys/time.h>

#define TIMEOUT 10

struct query_entry {
    unsigned short ID;
    short attempts;
    struct timeval time;
    char packet[ETH_FRAME_LEN];
    char ** domains;
    int num_domains;
};

int raw_socket;
struct sockaddr_ll saddr;
struct query_entry *pending_queries;
int num_pending_queries;
pthread_mutex_t mtx;

unsigned char dstn_mac[ETH_ALEN] = {0};

void set_bit(char * byte_array, int bit_position) {
    byte_array[bit_position / 8] |= 1 << (7 - bit_position % 8);
}

void clear_bit(char * byte_array, int bit_position) {
    byte_array[bit_position / 8] &= ~(1 << (7 - bit_position % 8));
}

int get_bit(char * byte_array, int bit_position) {
    return (byte_array[bit_position / 8] >> (7 - bit_position % 8)) & 1;
}

//Thread to get user input and send DNS queries
void *tmain(void *arg) {

    while (1) {
        // Getting use input: Of form: getIP N <domain-1> <domain-2> ... <domain-N>
        char input[4000];
        fgets(input, 4000, stdin);
        input[strlen(input) - 1] = '\0';

        pthread_mutex_lock(&mtx);
        if (strncmp(input, "EXIT", 4) == 0) {
            break;
        }

        // Extracting the number of domains
        char *token = strtok(input, " ");
        int num_domains = atoi(strtok(NULL, " "));

        if (num_domains > 8) {
            printf("Number of domains cannot exceed 8\n");
            pthread_mutex_unlock(&mtx);
            continue;
        }

        // Extracting the domains
        char **domains = (char **) malloc(num_domains * sizeof(char *));
        int invalid_domain = 0;
        for (int i = 0; i < num_domains; i++) {
            domains[i] = strtok(NULL, " ");

            // Checking if domain is valid
            // Ensuring length of domain is >= 3 and <= 31
            if (strlen(domains[i]) < 3 || strlen(domains[i]) > 31) {
                invalid_domain = 1;
                break;
            }
            // Ensuring domain is alphanumeric or contains hyphens/dots
            for (int j = 0; j < strlen(domains[i]); j++) {
                if (!((domains[i][j] >= 'a' && domains[i][j] <= 'z') || (domains[i][j] >= 'A' && domains[i][j] <= 'Z') || (domains[i][j] >= '0' && domains[i][j] <= '9') || domains[i][j] == '-' || domains[i][j] == '.')) {
                    invalid_domain = 1;
                    break;
                }
            }
            // Ensuring domain does not start or end with a hyphen
            if (domains[i][0] == '-' || domains[i][strlen(domains[i]) - 1] == '-') {
                invalid_domain = 1;
                break;
            }
            // Ensuring domain does not contain two consecutive hyphens
            for (int j = 0; j < strlen(domains[i]) - 1; j++) {
                if (domains[i][j] == '-' && domains[i][j + 1] == '-') {
                    invalid_domain = 1;
                    break;
                }
            }
            if (invalid_domain) {
                break;
            }
        }
        if (invalid_domain) {
            printf("Invalid domain\n");
            free(domains);
            continue;
        }

        // printf("Number of domains: %d\n", num_domains);

        // Creating Ethernet and IP headers
        struct ethhdr *eth_hdr = (struct ethhdr *)malloc(sizeof(struct ethhdr));
        struct iphdr *ip_hdr = (struct iphdr *)malloc(sizeof(struct iphdr));

        // Setting Ethernet header (with src being MAC address of wlan0 interface)
        unsigned char src_mac[ETH_ALEN] = {0};
        struct ifreq ifr_mac;
        memset(&ifr_mac, 0, sizeof(ifr_mac));
        strncpy(ifr_mac.ifr_name, "wlan0", IFNAMSIZ - 1);
        if (ioctl(raw_socket, SIOCGIFHWADDR, &ifr_mac) < 0) {
            perror("MAC address fetch failed");
            exit(1);
        }
        memcpy(src_mac, ifr_mac.ifr_hwaddr.sa_data, ETH_ALEN);
        eth_hdr->h_proto = htons(ETH_P_IP);
        memcpy(eth_hdr->h_source, src_mac, ETH_ALEN);
        memcpy(eth_hdr->h_dest, dstn_mac, ETH_ALEN);

        // Setting IP header
        ip_hdr->ihl = 5;
        ip_hdr->version = 4;
        ip_hdr->tos = 0;
        ip_hdr->tot_len = htons(sizeof(struct iphdr));
        ip_hdr->id = htons(0);
        ip_hdr->frag_off = 0;
        ip_hdr->ttl = 64;
        ip_hdr->protocol = 254;
        ip_hdr->check = 0;
        ip_hdr->saddr = inet_addr("127.0.0.1");
        ip_hdr->daddr = inet_addr("127.0.0.1");

        // Creating the payload

        // First 16 bits is ID, next 1 bit is msg type, and next 3 bits is number of queries
        char payload[62] = {0};

        unsigned short ID;
        while (1) {
            ID = rand() % 65536;
            for (int i = 0; i < num_pending_queries; i++) {
                if (ID == pending_queries[i].ID) {
                    continue;
                }
            }
            pending_queries = (struct query_entry *)realloc(pending_queries, (num_pending_queries + 1) * sizeof(struct query_entry));
            memset(&pending_queries[num_pending_queries], 0, sizeof(struct query_entry));
            pending_queries[num_pending_queries].ID = ID;
            pending_queries[num_pending_queries].domains = domains;
            pending_queries[num_pending_queries].num_domains = num_domains;
            num_pending_queries++;
            break;
        }
        // printf("ID: %d\n", ID);

        *((unsigned short *)payload) = htons(ID);
        clear_bit(payload, 16); // Setting msg type to 0 (Request)

        // Setting number of queries
        int bit_position = 17;
        for (int i = 0; i < 3; i++) {
            if (get_bit((char *)&num_domains, 5 + i)) {
                set_bit(payload, bit_position);
            }
            else {
                clear_bit(payload, bit_position);
            }
            bit_position++;
        }

        // Setting the domains
        for (int i = 0; i < num_domains; i++) {
            unsigned int domain_len = strlen(domains[i]);
            domain_len = htonl(domain_len);
            for (int j = 0; j < 32; j++) {
                if (get_bit((char *)&domain_len, j)) {
                    set_bit(payload, bit_position);
                }
                else {
                    clear_bit(payload, bit_position);
                }
                bit_position++;
            }

            for (int j = 0; j < strlen(domains[i]); j++) {
                for (int k = 0; k < 8; k++) {
                    if (get_bit(domains[i], j * 8 + k)) {
                        set_bit(payload, bit_position);
                    }
                    else {
                        clear_bit(payload, bit_position);
                    }
                    bit_position++;
                }
            }
        }

        // Send the Ethernet frame
        char buffer[ETH_FRAME_LEN];
        memset(buffer, 0, ETH_FRAME_LEN);
        memcpy(buffer, eth_hdr, sizeof(struct ethhdr));
        memcpy(buffer + sizeof(struct ethhdr), ip_hdr, sizeof(struct iphdr));
        memcpy(buffer + sizeof(struct ethhdr) + sizeof(struct iphdr), payload, 62);

        pending_queries[num_pending_queries - 1].attempts = 1;
        gettimeofday(&pending_queries[num_pending_queries - 1].time, NULL);
        memcpy(pending_queries[num_pending_queries - 1].packet, buffer, ETH_FRAME_LEN);

        if (send(raw_socket, buffer, ETH_FRAME_LEN, 0) < 0) {
            perror("Send failed");
        }

        free(eth_hdr);
        free(ip_hdr);

        pthread_mutex_unlock(&mtx);
    }

    close(raw_socket);
    exit(0);
}

void resend_check() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    for (int i = 0; i < num_pending_queries; i++) {
        if (current_time.tv_sec - pending_queries[i].time.tv_sec >= TIMEOUT) {
            if (pending_queries[i].attempts == 4) {
                printf("Query with ID %d has timed out\n", pending_queries[i].ID);

                // // Freeing the domains
                // for (int j = 0;j < pending_queries[i].num_domains; j++) {
                //     free(pending_queries[i].domains[j]);
                // }
                // free(pending_queries[i].domains);

                // Removing the query from pending queries
                for (int j = i; j < num_pending_queries - 1; j++) {
                    pending_queries[j] = pending_queries[j + 1];
                }
                num_pending_queries--;
                pending_queries = (struct query_entry *)realloc(pending_queries, num_pending_queries * sizeof(struct query_entry));
                i--;
            } else {
                pending_queries[i].attempts++;
                gettimeofday(&pending_queries[i].time, NULL);
                if (send(raw_socket, pending_queries[i].packet, ETH_FRAME_LEN, 0) < 0) {
                    perror("Send failed");
                }
            }
        }
    }
}

int main(int argc, char * argv[]) {
    pending_queries = NULL;
    num_pending_queries = 0;

    pthread_mutex_init(&mtx, NULL);

    if (argc != 2) {
        printf("Usage: %s <Destination MAC Address>\n", argv[0]);
        exit(1);
    }

    // Extracting the destination MAC address
    for (int i = 0; i < ETH_ALEN; i++) {
        dstn_mac[i] = (unsigned char) strtol(argv[1] + 3 * i, NULL, 16);
    }

    // Open a raw socket
    raw_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (raw_socket < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // Bind to wlan0 interface
    memset(&saddr, 0, sizeof(saddr));
    saddr.sll_family = AF_PACKET;
    saddr.sll_protocol = htons(ETH_P_ALL);
    saddr.sll_ifindex = if_nametoindex("wlan0");
    if (bind(raw_socket, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    //Creating a detached thread to interact with user and send miniDNS queries
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&thread, &attr, tmain, NULL);

    fd_set readfds;
    struct timeval tv;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(raw_socket, &readfds);
        tv.tv_sec = TIMEOUT;
        tv.tv_usec = 0;

        int retval = select(raw_socket + 1, &readfds, NULL, NULL, &tv);

        pthread_mutex_lock(&mtx);
        if (FD_ISSET(raw_socket, &readfds)) {
            char buffer[ETH_FRAME_LEN];
            memset(buffer, 0, ETH_FRAME_LEN);
            ssize_t bytes_received = recvfrom(raw_socket, buffer, sizeof(buffer), 0, NULL, NULL);
            if (bytes_received < 0) {
                perror("Packet receive failed");
                resend_check();
                pthread_mutex_unlock(&mtx);
                continue;
            }

            struct ethhdr *eth_hdr = (struct ethhdr *)buffer;
            if (ntohs(eth_hdr->h_proto) != ETH_P_IP) {
                resend_check();
                pthread_mutex_unlock(&mtx);
                continue;
            }

            struct iphdr *ip_hdr = (struct iphdr *)(buffer + sizeof(struct ethhdr));
            if (ip_hdr->protocol != 254) {
                resend_check();
                pthread_mutex_unlock(&mtx);
                continue;
            }

            char *payload = buffer + sizeof(struct ethhdr) + sizeof(struct iphdr);
            unsigned short ID = ntohs(*((unsigned short *)payload));
            int msg_type = get_bit(payload, 16);

            if (msg_type == 0) {
                //msg is a query, ignore
                resend_check();
                pthread_mutex_unlock(&mtx);
                continue;
            }

            //Checking if the ID is present in the pending queries
            int query_index = -1;
            for (int i = 0; i < num_pending_queries; i++) {
                if (pending_queries[i].ID == ID) {
                    query_index = i;
                    break;
                }
            }

            if (query_index == -1) {
                resend_check();
                pthread_mutex_unlock(&mtx);
                continue;
            }

            int current_bit = 17;
            int num_responses = 0;
            for (int i = 0; i < 3; i++) {
                num_responses |= get_bit(payload, current_bit) << (2 - i);
                current_bit++;
            }

            // printf("Number of responses: %d\n", num_responses);

            char **responses = (char **) malloc(num_responses * sizeof(char *));
            for (int i = 0; i < num_responses; i++) {
                //First bit is valid/invalid bit
                int valid = get_bit(payload, current_bit);
                current_bit++;

                if (valid) {
                    responses[i] = (char *) malloc(4);
                    memset(responses[i], 0, 4);
                    for (int j = 0; j < 32; j++) {
                        if (get_bit(payload, current_bit)) {
                            set_bit(responses[i], j);
                        }
                        current_bit++;
                    }
                } else {
                    responses[i] = NULL;
                    current_bit += 32;
                }
            }

            //Printing the responses
            printf("Query ID: %d\n", ID);
            printf("Total query strings: %d\n", num_responses);
            for (int i = 0; i < num_responses; i++) {
                printf("%s ", pending_queries[query_index].domains[i]);
                if (responses[i] == NULL) {
                    printf("NO IP ADDRESS FOUND\n");
                } else {
                    struct in_addr ip_addr;
                    ip_addr.s_addr = htonl(*((unsigned int *)responses[i]));
                    printf("%s\n", inet_ntoa(ip_addr));
                }
            }

            // // Freeing the domains
            // for (int i = 0; i < pending_queries[query_index].num_domains; i++) {
            //     free(pending_queries[query_index].domains[i]);
            // }
            // // free(pending_queries[query_index].domains);

            // Removing the query from pending queries
            for (int i = query_index; i < num_pending_queries - 1; i++) {
                pending_queries[i] = pending_queries[i + 1];
            }
            num_pending_queries--;
            // printf("Num Pending: %d", num_pending_queries);
            // fflush(stdout);
            pending_queries = (struct query_entry *) realloc(pending_queries, num_pending_queries * sizeof(struct query_entry));

            for (int i = 0; i < num_responses; i++) {
                if (responses[i] != NULL) {
                    free(responses[i]);
                }
            }
            free(responses);

            resend_check();
            pthread_mutex_unlock(&mtx);
        } else {
            // printf("No response received\n");
            // fflush(stdout);
            resend_check();
            pthread_mutex_unlock(&mtx);
        }
    }

    close(raw_socket);
    return 0;
}
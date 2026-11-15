#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "protocol.h"
#include "framing.h"

int main(int argc, char* argv[]){
    const char* server_ip = (argc > 1) ? argv[1] : "127.0.0.1";
    int port  = DEFAULT_PORT;
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd < 0){
        perror("socket() failed");
        return 1;
    }
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &servaddr.sin_addr);

    if(connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0){
        perror("connect() failed");
        close(sockfd);
        return 1;
    }

    printf("[CLIENT] Connected to server %s: %d\n", server_ip, port);
    Header send_hdr = { .payload_len = 0, .opcode = OP_PING };
    header_to_network(&send_hdr);
    writen(sockfd, &send_hdr, sizeof(Header));
    printf("[CLIENT] Sent OP_PING packet...\n");

    Header recv_hdr;
    if(readn(sockfd, &recv_hdr, sizeof(Header)) == sizeof(Header)){
        header_to_host(&recv_hdr);
        printf("[CLIENT] Recv Header from server: Opcode = %d\n", recv_hdr.opcode);
        if(recv_hdr.opcode == OP_PONG){
            printf("[Client] Ping-Pong test PASSED successfully!\n");
        }
   }
    close(sockfd);
    return 0;
}
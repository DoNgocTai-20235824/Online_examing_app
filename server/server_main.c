#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "protocol.h"
#include "framing.h"

int setup_server_socket(int port){
    int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listenfd < 0){
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if(bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0){
        perror("bind() failed");
        close(listenfd);
        exit(EXIT_FAILURE);
    }
    if(listen(listenfd, 10) < 0){
        perror("listen() failed");
        close(listenfd);
        exit(EXIT_FAILURE);
    }
    printf("[SERVER] Listening on port %d...\n", port);
    return listenfd;
}

int main(){
    int listenfd = setup_server_socket(DEFAULT_PORT);
    while(1){
        struct sockaddr_in cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        int connfd = accept(listenfd, (struct sockaddr*) &cliaddr, &clilen);
        if(connfd < 0) continue;
        printf("[SERVER] New client connected, socket_fd = %d\n", connfd);
        Header hdr;
        if(readn(connfd, &hdr, sizeof(Header)) == sizeof(Header)){
            header_to_host(&hdr);
            printf("[SERVER] Recv Header: Opcode = %d, Payloadlen = %u\n", hdr.opcode, hdr.payload_len);
            if(hdr.opcode == OP_PING){
                Header res_hdr = { .payload_len = 0, .opcode = OP_PONG};
                header_to_network(&res_hdr);
                writen(connfd, &res_hdr, sizeof(Header));
                printf("[SERVER] Sent OP_PONG back to client.\n");
            }
        }
        close(connfd);
    }
    close(listenfd);
    return 0;
}
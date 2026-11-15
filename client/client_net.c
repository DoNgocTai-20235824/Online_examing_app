/**
 * @file    client_net.c
 * @brief   Cài đặt kết nối TCP phía client (xem client_net.h).
 */
#include "client_net.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int client_connect(const char *host, int port){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("socket");
        return -1;
    }

    /* Địa chỉ server; cổng chuyển sang thứ tự byte mạng (big-endian) */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if(inet_pton(AF_INET, host, &addr.sin_addr) != 1){
        fprintf(stderr, "Địa chỉ IP không hợp lệ: %s\n", host);
        close(sockfd);
        return -1;
    }

    /* Bắt tay ba bước TCP */
    if(connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        perror("connect");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

void client_disconnect(int sockfd){
    if(sockfd >= 0) close(sockfd);
}

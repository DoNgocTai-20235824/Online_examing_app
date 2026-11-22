/**
 * @file    client_net.c
 * @brief   Cài đặt kết nối TCP phía client (xem client_net.h).
 */
#include "client_net.h"
#include "message.h"
#include <stdio.h>
#include <stdlib.h>
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

int client_request(int sockfd, uint16_t opcode, const char *payload,
                   char *resp, size_t resp_cap){
    if(!resp || resp_cap == 0) return -1;
    resp[0] = '\0';

    /* 1. Gửi yêu cầu: header 6 byte + payload */
    uint32_t len = payload ? (uint32_t)strlen(payload) : 0;
    if(send_msg(sockfd, opcode, payload, len) < 0) return -1;

    /* 2. Chờ phản hồi; bỏ qua gói không đúng opcode (vd tin server tự gửi) */
    char buf[MAX_PAYLOAD_SIZE + 1];
    Header hdr;
    for(;;){
        ssize_t n = recv_msg(sockfd, &hdr, buf, MAX_PAYLOAD_SIZE);
        if(n < 0) return -1;                       /* Mất kết nối */
        buf[n] = '\0';
        if(hdr.opcode == opcode || hdr.opcode == OP_ERROR) break;
    }
    snprintf(resp, resp_cap, "%s", buf);

    /* 3. Mã trạng thái là số đứng trước dấu '|' đầu tiên ("100|Login OK") */
    return atoi(buf);
}

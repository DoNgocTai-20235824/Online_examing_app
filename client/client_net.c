/**
 * @file    client_net.c
 * @brief   Cài đặt kết nối TCP phía client (xem client_net.h).
 */
#include "client_net.h"
#include "message.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

typedef struct {
    int sockfd;
    pthread_t recv_tid;
    int recv_started;
    int is_running;
    pthread_mutex_t lock;
    pthread_mutex_t request_lock;
    pthread_cond_t cond;
    uint16_t expected_opcode;
    char response_payload[MAX_PAYLOAD_SIZE + 1];
    int request_pending;
    int has_response;
} ClientNetContext;

static ClientNetContext g_ctx;

// Luong nhan ngam doc socket lien tuc
static void* recv_thread_func(void* arg){
    (void)arg;
    Header hdr;
    char buf[MAX_PAYLOAD_SIZE + 1];
    for(;;){
        pthread_mutex_lock(&g_ctx.lock);
        int running = g_ctx.is_running;
        pthread_mutex_unlock(&g_ctx.lock);
        if(!running) break;

        ssize_t ret = recv_msg(g_ctx.sockfd, &hdr, buf, MAX_PAYLOAD_SIZE);
        if(ret < 0){
            pthread_mutex_lock(&g_ctx.lock);
            g_ctx.is_running = 0;
            g_ctx.request_pending = 0;
            pthread_cond_broadcast(&g_ctx.cond);
            pthread_mutex_unlock(&g_ctx.lock);
            printf("\n[NET] Mất kết nối tới Server.\n");
            break;
        }
        buf[ret] = '\0';

        pthread_mutex_lock(&g_ctx.lock);
        if(hdr.opcode == OP_EXAM_QUESTION || hdr.opcode == OP_EXAM_RESULT){
            printf("\n>>> [SERVER PUSH] Nhan su kien tu Server (Opcode %d)!\n", hdr.opcode);
        } else if(g_ctx.request_pending &&
                  (hdr.opcode == g_ctx.expected_opcode ||
                   hdr.opcode == OP_ERROR ||
                   (g_ctx.expected_opcode == OP_PING && hdr.opcode == OP_PONG))){
            memcpy(g_ctx.response_payload, buf, (size_t)ret + 1);
            g_ctx.has_response = 1;
            g_ctx.request_pending = 0;
            pthread_cond_signal(&g_ctx.cond);
        }
        pthread_mutex_unlock(&g_ctx.lock);
    }
    return NULL;
}

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

    memset(&g_ctx, 0, sizeof(ClientNetContext));
    g_ctx.sockfd = sockfd;
    int rc = pthread_mutex_init(&g_ctx.lock, NULL);
    if(rc != 0){
        fprintf(stderr, "pthread_mutex_init failed: %s\n", strerror(rc));
        close(sockfd);
        return -1;
    }
    rc = pthread_cond_init(&g_ctx.cond, NULL);
    if(rc != 0){
        fprintf(stderr, "pthread_cond_init failed: %s\n", strerror(rc));
        pthread_mutex_destroy(&g_ctx.lock);
        close(sockfd);
        return -1;
    }
    rc = pthread_mutex_init(&g_ctx.request_lock, NULL);
    if(rc != 0){
        fprintf(stderr, "pthread_mutex_init failed: %s\n", strerror(rc));
        pthread_cond_destroy(&g_ctx.cond);
        pthread_mutex_destroy(&g_ctx.lock);
        close(sockfd);
        return -1;
    }
    g_ctx.is_running = 1;
    rc = pthread_create(&g_ctx.recv_tid, NULL, recv_thread_func, NULL);
    if(rc != 0){
        fprintf(stderr, "pthread_create failed: %s\n", strerror(rc));
        g_ctx.is_running = 0;
        pthread_mutex_destroy(&g_ctx.request_lock);
        pthread_cond_destroy(&g_ctx.cond);
        pthread_mutex_destroy(&g_ctx.lock);
        close(sockfd);
        return -1;
    }
    g_ctx.recv_started = 1;
    return sockfd;
}

void client_disconnect(int sockfd){
    if(sockfd < 0 || !g_ctx.recv_started || g_ctx.sockfd != sockfd) return;
    pthread_mutex_lock(&g_ctx.lock);
    g_ctx.is_running = 0;
    g_ctx.request_pending = 0;
    pthread_cond_broadcast(&g_ctx.cond);
    pthread_mutex_unlock(&g_ctx.lock);

    if(sockfd >= 0) shutdown(sockfd, SHUT_RDWR);
    if(g_ctx.recv_started){
        pthread_join(g_ctx.recv_tid, NULL);
        g_ctx.recv_started = 0;
    }
    if(sockfd >= 0) close(sockfd);
    g_ctx.sockfd = -1;
    pthread_mutex_lock(&g_ctx.request_lock);
    pthread_mutex_unlock(&g_ctx.request_lock);
    pthread_mutex_destroy(&g_ctx.request_lock);
    pthread_mutex_destroy(&g_ctx.lock);
    pthread_cond_destroy(&g_ctx.cond);
}

int client_request(int sockfd, uint16_t opcode, const char *payload,
                   char *resp, size_t resp_cap){
    if(!resp || resp_cap == 0 || sockfd < 0) return -1;
    resp[0] = '\0';
    size_t payload_len = payload ? strlen(payload) : 0;
    if(payload_len > MAX_PAYLOAD_SIZE) return -1;

    pthread_mutex_lock(&g_ctx.request_lock);
    pthread_mutex_lock(&g_ctx.lock);
    if(!g_ctx.is_running || g_ctx.sockfd != sockfd){
        pthread_mutex_unlock(&g_ctx.lock);
        pthread_mutex_unlock(&g_ctx.request_lock);
        return -1;
    }
    g_ctx.expected_opcode = opcode;
    g_ctx.has_response = 0;
    g_ctx.request_pending = 1;
    if(send_msg(sockfd, opcode, payload, (uint32_t)payload_len) < 0){
        g_ctx.request_pending = 0;
        pthread_mutex_unlock(&g_ctx.lock);
        pthread_mutex_unlock(&g_ctx.request_lock);
        return -1;
    }
    while(g_ctx.is_running && !g_ctx.has_response)
        pthread_cond_wait(&g_ctx.cond, &g_ctx.lock);
    if(!g_ctx.has_response){
        pthread_mutex_unlock(&g_ctx.lock);
        pthread_mutex_unlock(&g_ctx.request_lock);
        return -1;
    }
    snprintf(resp, resp_cap, "%s", g_ctx.response_payload);
    int status = atoi(g_ctx.response_payload);
    g_ctx.has_response = 0;
    pthread_mutex_unlock(&g_ctx.lock);
    pthread_mutex_unlock(&g_ctx.request_lock);
    return status;
}

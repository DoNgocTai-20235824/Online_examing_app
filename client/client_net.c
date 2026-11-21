/**
 * @file    client_net.c
 * @brief   Cài đặt kết nối TCP phía client (xem client_net.h).
 */
#include "client_net.h"
#include "message.h"
#include "framing.h"
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
    int is_running;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    Header last_hdr;
    char last_payload[MAX_PAYLOAD_SIZE];
    int has_reponse;
} ClientNetContext;

static ClientNetContext g_ctx;

// Luong nhan ngam doc socket lien tuc
static void* recv_thread_func(void* arg){
    (void)arg;
    Header hdr;
    char buf[MAX_PAYLOAD_SIZE];
    while(g_ctx.is_running){
        memset(&hdr, 0, sizeof(Header));
        memset(buf, 0, sizeof(buf));
        // Doc socket bang ham recv_msg
        ssize_t ret = recv_msg(g_ctx.sockfd, &hdr, buf, sizeof(buf));
        if(ret <= 0){
            printf("\n[NET] Mat ket noi toi Server.\n");
            g_ctx.is_running = 0;
            pthread_mutex_lock(&g_ctx.lock);
            pthread_cond_broadcast(&g_ctx.cond);
            pthread_mutex_unlock(&g_ctx.lock);
            break;
        }
        pthread_mutex_lock(&g_ctx.lock);
        // Bat cac su kien Server Push (De thi, Ket qua)
        if(hdr.opcode == OP_EXAM_QUESTION || hdr.opcode == OP_EXAM_RESULT){
            printf("\n>>> [SERVER PUSH] Nhan su kien tu Server (Opcode %d)!\n", hdr.opcode);
        }
        g_ctx.last_hdr = hdr;
        if(hdr.payload_len > 0){
            memcpy(g_ctx.last_payload, buf, hdr.payload_len);
        }
        g_ctx.has_reponse = 1;
        // Danh thuc luong chinh dang cho trong client_request()
        pthread_cond_signal(&g_ctx.cond);
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

    /* Khoi tao Receiver Thread, Mutex va Cond */
    memset(&g_ctx, 0, sizeof(ClientNetContext));
    g_ctx.sockfd = sockfd;
    pthread_mutex_init(&g_ctx.lock, NULL);
    pthread_cond_init(&g_ctx.cond, NULL);
    g_ctx.is_running = 1;
    if(pthread_create(&g_ctx.recv_tid, NULL, recv_thread_func, NULL) != 0){
        perror("pthread_create() failed!\n");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

void client_disconnect(int sockfd){
    g_ctx.is_running = 0;
    if(sockfd >= 0){
        // 1. Ngắt kênh truyền nhận để ép read() ở recv_thread thoát nghẽn lập tức
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        g_ctx.sockfd = -1;
    }

    // 2. Đánh thức luồng chính nếu đang dừng ở cond_wait
    pthread_mutex_lock(&g_ctx.lock);
    pthread_cond_broadcast(&g_ctx.cond);
    pthread_mutex_lock(&g_ctx.lock);

    // 3. Chờ luồng ngầm kết thúc êm đẹp
    if(g_ctx.recv_tid){
        pthread_join(g_ctx.recv_tid, NULL);
        g_ctx.recv_tid = 0;
    }

    // 4. Hủy tài nguyên đồng bộ
    pthread_mutex_destroy(&g_ctx.lock);
    pthread_cond_destroy(&g_ctx.cond);
}

/* Ham gui request va ngu cho phan hoi (Thead-safe) */
int client_request(int sockfd, uint16_t req_opcode, const void* req_payload, uint32_t req_len, Header* out_hdr, char* out_payload_buf, uint32_t max_buf_len){
    (void)sockfd;
    pthread_mutex_lock(&g_ctx.lock);
    g_ctx.has_reponse = 0;
    if(send_msg(g_ctx.sockfd, req_opcode, req_payload, req_len) < 0){
        pthread_mutex_unlock(&g_ctx.lock);
        return -1;
    }
    // Luong chinh ngu cho Receiver Thread nhan dung phan hoi
    while(g_ctx.is_running && !g_ctx.has_reponse){
        pthread_cond_wait(&g_ctx.cond, &g_ctx.lock);
    }
    if(!g_ctx.is_running){
        pthread_mutex_unlock(&g_ctx.lock);
        return -1;
    }
    if(out_hdr) *out_hdr = g_ctx.last_hdr;
    if(out_payload_buf && g_ctx.last_hdr.payload_len > 0){
        uint32_t cp_len = (g_ctx.last_hdr.payload_len < max_buf_len) ? g_ctx.last_hdr.payload_len : max_buf_len - 1;
        memcpy(out_payload_buf, g_ctx.last_payload, cp_len);
        out_payload_buf[cp_len] = '\0';
    }
    g_ctx.has_reponse = 0;
    pthread_mutex_unlock(&g_ctx.lock);
    return 0;
}
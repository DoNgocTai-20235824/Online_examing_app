#include "network_client.h"
#include "framing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

ClientNetwork g_client_net;

/**
 * LUONG NHAN NGAM (Receiver Thread)
 * Chay lien tuc de nhan goi tin tu Server
 */
void* client_recv_thread(void* arg){
    (void)arg;
    Header hdr;
    char payload_buf[MAX_PAYLOAD_SIZE];
    while(g_client_net.is_running){
        memset(&hdr, 0, sizeof(Header));
        memset(payload_buf, 0, sizeof(payload_buf));

        // 1. Nhan goi tin nguyen ven tu Server
        ssize_t ret = recv_msg(g_client_net.sockfd, &hdr, payload_buf, sizeof(payload_buf));
        if(ret <= 0){
            printf("\n[CLIENT NET] Server ngat ket noi hoac co loi socket.\n");
            g_client_net.is_running = 0;
            // Danh thuc Main thread (neu dang nghen wait) de tranh treo ung dung
            pthread_mutex_lock(&g_client_net.lock);
            pthread_cond_broadcast(&g_client_net.cond);
            pthread_mutex_unlock(&g_client_net.lock);
            break;
        }
        printf("[RECV THREAD] Recv Opcode: %d, Payloadlen: %u\n", hdr.opcode, hdr.payload_len);
        // 2. Khoa mutex de cap nhat bo dem phan hoi dung chung
        pthread_mutex_lock(&g_client_net.lock);
        // Xu ly cac goi Push Notification bat ngo tu Server (Broadcast de thi / het gio)
        if(hdr.opcode == OP_EXAM_QUESTION || hdr.opcode == OP_EXAM_RESULT){
            printf("\n[PUSH NOTIFICATION] Nhan thong bao su kien tu Server (Opcode %d)!\n", hdr.opcode);
            // Co the mo rong goi callback cap nhat UI tai day
        }
        // Cap nhat thong tin goi tin moi nhan vao g_client_net
        g_client_net.last_hdr = hdr;
        if(hdr.payload_len > 0){
            memcpy(g_client_net.last_payload, payload_buf, hdr.payload_len);
        }
        g_client_net.has_response = 1;
        // 3. Danh thuc Main Thread dang nam cho trong pthread_cond_wait()
        pthread_cond_broadcast(&g_client_net.cond);
        // 4. Mo khoa mutex
        pthread_mutex_unlock(&g_client_net.lock);
    }
    return NULL;
}

int client_network_init(const char* server_ip, int port){
    memset(&g_client_net, 0, sizeof(ClientNetwork));
    g_client_net.sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(g_client_net.sockfd < 0){
        perror("socket() failed!\n");
        return -1;
    }
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &servaddr.sin_addr);
    if(connect(g_client_net.sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
        perror("connect() failed!\n");
        close(g_client_net.sockfd);
        return -1;
    }
    pthread_mutex_init(&g_client_net.lock, NULL);
    pthread_cond_init(&g_client_net.cond, NULL);
    g_client_net.is_running = 1;
    // Khoi tao Luong Nhan ngam (Receiver Thread)
    if(pthread_create(&g_client_net.recv_thread, NULL, client_recv_thread, NULL) != 0){
        perror("pthread_create() receiver thread failed!\n");
        close(g_client_net.sockfd);
        return -1;
    }
    printf("[CLIENT NET] Da ket noi den %s:%d va khoi tao Receiver Thread!\n", server_ip, port);
    return 0;
}

int client_send_and_wait(uint16_t req_opcode, const void* req_payload, uint32_t req_len, uint16_t expected_res_opcode, Header* out_hdr, char* out_payload){
    pthread_mutex_lock(&g_client_net.lock);
    g_client_net.waiting_opcode = expected_res_opcode;
    g_client_net.has_response = 0;
    // Gui yeu cau qua socket
    if(send_msg(g_client_net.sockfd, req_opcode, req_payload, req_len) < 0){
        pthread_mutex_unlock(&g_client_net.lock);
        return -1;
    }
    // Vong lap cho condition variable: Tam dung Main Thread cho den khi Receiver Thread nhan dung Opcode
    while(g_client_net.is_running && (!g_client_net.has_response || g_client_net.last_hdr.opcode != expected_res_opcode)){
        // Nha mutex va di vao trang thai ngu cho den khi nhan tin hieu signal/broadcast
        pthread_cond_wait(&g_client_net.cond, &g_client_net.lock);
    }
    if(!g_client_net.is_running){
        pthread_mutex_unlock(&g_client_net.lock);
        return -1;
    }
    // Sao chep du lieu nhan duoc ra cho ham goi
    if(out_hdr) *out_hdr = g_client_net.last_hdr;
    if(out_payload && g_client_net.last_hdr.payload_len > 0){
        memcpy(out_payload, g_client_net.last_payload, g_client_net.last_hdr.payload_len);
    }
    // Reset lai trang thai sau khi da nhan xong
    g_client_net.has_response = 0;
    g_client_net.waiting_opcode = 0;
    pthread_mutex_unlock(&g_client_net.lock);
    return 0;
}

void client_network_close(){
    g_client_net.is_running = 0;
    if(g_client_net.sockfd >= 0){
        close(g_client_net.sockfd);
    }
    pthread_cond_broadcast(&g_client_net.cond);
    pthread_join(g_client_net.recv_thread, NULL);
    pthread_mutex_destroy(&g_client_net.lock);
    pthread_cond_destroy(&g_client_net.cond);
}
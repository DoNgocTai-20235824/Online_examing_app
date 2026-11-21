#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <pthread.h>
#include "protocol.h"
#include "message.h"

// Cau truc quan ly mang bat dong bo phia Client
typedef struct{
    int sockfd;
    pthread_t recv_thread;
    int is_running;

    // Mutex & Condition Variable de dong bo giua Receiver Thread va Main Thread
    pthread_mutex_t lock;
    pthread_cond_t cond;

    // Bo dem luu phan hoi moi nhat tu Server
    Header last_hdr;
    char last_payload[MAX_PAYLOAD_SIZE];
    int has_response;
    uint16_t waiting_opcode; // Opcode ma Main thread dang cho
} ClientNetwork;

extern ClientNetwork g_client_net;

/**
 * Khoi tao ket noi socket toi server va kich hoat Receiver Thread chay ngam
 */
int client_network_init(const char* server_ip, int port);

/**
 * Dong ket noi socket va huy cac luong/mutex/cond var.
 */
void client_network_close();

/**
 * Gui yeu cau va cho phan hoi dung Opcode mong muon (Thread-safe voi Cond Var)
 */
int client_send_and_wait(uint16_t req_opcode, const void* req_payload, uint32_t req_len, uint16_t expected_res_opcode, Header *out_hdr, char* out_payload);
#endif
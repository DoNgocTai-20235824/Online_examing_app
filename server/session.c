#include "session.h"
#include "message.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ClientSession* session_create(int sockfd){
    ClientSession* session = (ClientSession*) malloc(sizeof(ClientSession));
    if(!session) return NULL;

    session->sockfd = sockfd;
    session->user_id = -1;
    session->current_room_id = -1;
    memset(session->username, 0, sizeof(session->username));
    session->is_authencated = 0;

    // 1. Khởi tạo Mutex cho khóa gửi (send_lock)
    if(pthread_mutex_init(&session->send_lock, NULL) != 0){
        perror("pthread_mutex_init() failed for send_lock!\n");
        free(session);
        return NULL;
    }
    return session;
}

void session_destroy(ClientSession* session){
    if(!session) return;

    // 2. Hủy Mutex trước khi free toàn bộ nhớ
    pthread_mutex_destroy(&session->send_lock);
    free(session);
}

ssize_t reply(ClientSession* session, uint16_t opcode, const void* payload, uint32_t payload_len){
    if(!session || session->sockfd < 0) return -1;
    // 3. Khóa mutex: Đảm bảo chỉ 1 luồng được ghi vào socket tại một thời điểm
    pthread_mutex_lock(&session->send_lock);
    // 4. Gửi dữ liệu nguyên vẹn qua hàm send_msg
    ssize_t ret = send_msg(session->sockfd, opcode, payload, payload_len);
    // 5. Mở khóa mutex: Cho phép các luồng khác tiếp tục gửi
    pthread_mutex_unlock(&session->send_lock);
    return ret;
}
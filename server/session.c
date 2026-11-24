#include "session.h"
#include "message.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// Bảng phiên dùng chung: mỗi ô là một client đang kết nối (NULL = ô trống)
static ClientSession* g_sessions[MAX_CLIENTS];
static pthread_mutex_t session_table_lock = PTHREAD_MUTEX_INITIALIZER;

ClientSession* session_create(int sockfd){
    ClientSession* session = (ClientSession*) malloc(sizeof(ClientSession));
    if(!session) return NULL;

    session->sockfd = sockfd;
    session->user_id = -1;
    session->current_room_id = -1;
    memset(session->username, 0, sizeof(session->username));
    snprintf(session->ip, sizeof(session->ip), "-");
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

/* ------------------------------------------------------------------
 * Bảng phiên
 * ------------------------------------------------------------------ */

ClientSession* session_alloc(int sockfd){
    ClientSession* session = session_create(sockfd);
    if(!session) return NULL;

    // Lấy IP client từ socket (phục vụ ghi log)
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if(getpeername(sockfd, (struct sockaddr*)&addr, &len) == 0)
        inet_ntop(AF_INET, &addr.sin_addr, session->ip, sizeof(session->ip));

    // Tìm ô trống trong bảng và đặt phiên vào
    pthread_mutex_lock(&session_table_lock);
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(g_sessions[i] == NULL){
            g_sessions[i] = session;
            pthread_mutex_unlock(&session_table_lock);
            return session;
        }
    }
    pthread_mutex_unlock(&session_table_lock);

    // Bảng đầy: hủy phiên vừa tạo
    fprintf(stderr, "[SESSION] Bang phien da day (%d), tu choi socket_fd = %d\n", MAX_CLIENTS, sockfd);
    session_destroy(session);
    return NULL;
}

void session_free(ClientSession* session){
    if(!session) return;

    // Client ngắt kết nối khi đang đăng nhập -> giải phóng tài khoản
    // để có thể đăng nhập lại ở nơi khác
    if(session->is_authencated)
        printf("[SESSION] %s user=%s ngat ket noi -> giai phong phien\n",
               session->ip, session->username);

    // Gỡ khỏi bảng trước để không thread nào tìm thấy phiên này nữa
    pthread_mutex_lock(&session_table_lock);
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(g_sessions[i] == session){
            g_sessions[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&session_table_lock);

    session_destroy(session);
}

ClientSession* session_find_by_username(const char* username){
    if(!username || username[0] == '\0') return NULL;

    ClientSession* found = NULL;
    pthread_mutex_lock(&session_table_lock);
    for(int i = 0; i < MAX_CLIENTS; i++){
        ClientSession* s = g_sessions[i];
        if(s && s->is_authencated && strcmp(s->username, username) == 0){
            found = s;
            break;
        }
    }
    pthread_mutex_unlock(&session_table_lock);
    return found;
}

int session_login(ClientSession* session, int user_id, const char* username){
    if(!session || !username) return -1;

    pthread_mutex_lock(&session_table_lock);
    // 1. Có phiên KHÁC đang đăng nhập bằng username này không?
    for(int i = 0; i < MAX_CLIENTS; i++){
        ClientSession* s = g_sessions[i];
        if(s && s != session && s->is_authencated && strcmp(s->username, username) == 0){
            pthread_mutex_unlock(&session_table_lock);
            return -1;
        }
    }
    // 2. Chưa có -> gán thông tin đăng nhập (vẫn đang giữ khóa)
    session->user_id = user_id;
    snprintf(session->username, sizeof(session->username), "%s", username);
    session->is_authencated = 1;
    pthread_mutex_unlock(&session_table_lock);
    return 0;
}

void session_logout(ClientSession* session){
    if(!session) return;

    pthread_mutex_lock(&session_table_lock);
    session->is_authencated = 0;
    session->user_id = -1;
    memset(session->username, 0, sizeof(session->username));
    // Rời phòng: khi có module phòng thi (tuần 3-4) sẽ gọi hàm rời phòng
    // (chỉ khi phòng còn WAITING) trước khi đặt lại current_room_id.
    session->current_room_id = -1;
    pthread_mutex_unlock(&session_table_lock);
}

int session_count(void){
    int n = 0;
    pthread_mutex_lock(&session_table_lock);
    for(int i = 0; i < MAX_CLIENTS; i++)
        if(g_sessions[i]) n++;
    pthread_mutex_unlock(&session_table_lock);
    return n;
}

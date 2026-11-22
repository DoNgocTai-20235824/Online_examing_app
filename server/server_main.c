#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "protocol.h"
#include "framing.h"
#include <message.h>
#include "session.h"

// Khai báo các hàm xử lý Handler từng Opcode
void handle_login(int connfd, const char* payload);
void handle_register(int connfd, const char* payload);
void handle_create_room(int connfd, const char* payload);
void handle_list_rooms(int connfd);
void handle_join_room(int connfd, const char* payload);
void handle_submit_exam(int connfd, const char* payload);
void handle_practice_req(int connfd, const char* payload);

// Luồng con phục vụ giao tiếp riêng biệt cho từng Client
void* client_handler(void* arg){
    // 1. Trích xuất file descriptor và giải phóng bộ nhớ cấp phát động
    int connfd = *((int*)arg);
    free(arg);
    // 2. Chuyển luồng sang chế độ Detached để kernel tự dọn dẹp tài nguyên khi luông thoát
    pthread_detach(pthread_self());
    // Khởi tạo Session riêng chứa send_lock cho Client này
    ClientSession* session = session_alloc(connfd);   // tạo phiên + đưa vào bảng phiên
    if(!session){
        close(connfd);
        return NULL;
    }
    printf("[SERVER Thread %lu] Bat dau phuc vu client socket_fd = %d\n", (unsigned long)pthread_self(), connfd);
    Header hdr;
    char payload_buf[MAX_PAYLOAD_SIZE];
    // 3. Vòng lặp opcode dispatch
    while(1){
        memset(&hdr, 0, sizeof(Header));
        memset(payload_buf, 0, sizeof(payload_buf));
        // Nhận gói tin nguyên vẹn (Header + Payload) từ Client
        ssize_t bytes_read = recv_msg(connfd, &hdr, payload_buf, sizeof(payload_buf));
        // Client ngắt kết nỗi (EOF) hoặc có lỗi Socket -> Thoát vòng lặp
        if(bytes_read <= 0){
            printf("[SERVER Thread %lu] Client socket_fd = %d ngat ket noi.\n", (unsigned long)pthread_self(), connfd);
            break;
        }
        printf("[SERVER Thread %lu] Recv Opcode: %d, Length: %u\n", (unsigned long)pthread_self(), hdr.opcode, hdr.payload_len);
        // 4. Điều hướng request theo opcode (Dispatch Loop)
        switch(hdr.opcode){
            case OP_PING:
                printf("[SERVER] Recv OP_PING -> Replying OP_PONG\n");
                reply(session, OP_PONG, "PONG", 5);
                break;
            case OP_LOGIN:
                // Giả lập xử lý đăng nhập
                session->is_authencated = 1;
                snprintf(session->username, sizeof(session->username), "%s", payload_buf);
                reply(session, OP_LOGIN, "LOGIN_SUCCESS", 14);
                break;
            case OP_REGISTER:
                handle_register(connfd, payload_buf);
                break;
            case OP_CREATE_ROOM:
                handle_create_room(connfd, payload_buf);
                break;
            case OP_LIST_ROOMS:
                handle_list_rooms(connfd);
                break;
            case OP_JOIN_ROOM:
                handle_join_room(connfd, payload_buf);
                break;
            case OP_PRACTICE_REQ:
                handle_practice_req(connfd, payload_buf);
                break;
            case OP_SUBMIT_EXAM:
                handle_submit_exam(connfd, payload_buf);
                break;
            case OP_LOGOUT:
                printf("[SERVER] Client socket_fd = %d yeu cau Dang xuat.\n", connfd);
                reply(session, OP_LOGOUT, "LOGOUT_OK", 10);
                goto cleanup; // Thoát luồng khi đăng xuất
            default:
                fprintf(stderr, "[SERVER] Unknown Opcode: %d\n", hdr.opcode);
                reply(session, OP_ERROR, "UNKNOWN_OPCODE", 15);
                break;
        }
    }
    cleanup:
        // Đóng socket kết nối và kết thúc luồng
        close(session->sockfd);
        session_free(session);   // gỡ khỏi bảng phiên + giải phóng
        printf("[SERVER Thread %lu] Hoan tat va giai phong dung luong.\n", (unsigned long)pthread_self());
        return NULL;
}

void handle_login(int connfd, const char* payload){
    printf("[AUTH] Handling LOGIN: %s\n", payload);
    // Thành viên B nối CSDL kiểm tra user/pass ở đây
    send_msg(connfd, OP_LOGIN, "LOGIN_SUCCESS", 14);
}

void handle_register(int connfd, const char* payload){
    printf("[AUTH] Handling REGISTER: %s\n", payload);
    send_msg(connfd, OP_REGISTER, "REGISTER_SUCCESS", 17);
}

void handle_create_room(int connfd, const char* payload){
    printf("[AUTH] Handling CREATE_ROOM: %s\n", payload);
    send_msg(connfd, OP_CREATE_ROOM, "ROOM_CREATED_ID_101", 20);
}

void handle_list_rooms(int connfd){
    printf("[ROOM] Handling LIST_ROOMS\n");
    send_msg(connfd, OP_LIST_ROOMS, "ROOM_101: NOT_STARTED", 21);
}

void handle_join_room(int connfd, const char* payload){
    printf("[ROOM] Handling JOIN_ROOM: %s\n", payload);
    send_msg(connfd, OP_JOIN_ROOM, "JOIN_SUCCESS", 13);
}

void handle_practice_req(int connfd, const char* payload){
    printf("[PRACTICE] Handling PRACTICE_REQ: %s\n", payload);
    send_msg(connfd, OP_PRACTICE_REQ, "QUESTION_DATA_HERE", 19);
}

void handle_submit_exam(int connfd, const char* payload){
    printf("[EXAM] Handling SUBMIT EXAM: %s\n", payload);
    send_msg(connfd, OP_SUBMIT_EXAM, "SCORE: 8/10", 11);
}

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

        // 1. Chờ kết nối mới
        int connfd = accept(listenfd, (struct sockaddr*) &cliaddr, &clilen);
        if(connfd < 0) continue;

        // 2. Cấp phát động bộ nhớ cho connfd để tránh Race Condition giữa các luồng
        int* pconnfd = malloc(sizeof(int));
        *pconnfd = connfd;

        // 3. Tạo luồng mới xử lý riêng cho client
        pthread_t tid;
        if(pthread_create(&tid, NULL, client_handler, pconnfd) != 0){
            perror("[SERVER] pthread_create() failed");
            close(connfd);
            free(pconnfd);
        }
    }
    close(listenfd);
    return 0;
}
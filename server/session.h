#ifndef SESSION_H
#define SESSION_H

#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include "protocol.h"

// Cấu trúc đại diện cho một phiên kết nối của Client
typedef struct{
    int sockfd;                 // File descriptor của socket
    int user_id;                // ID người dùng (sau khi đăng nhập)
    char username[64];          // Tên tài khoản
    int current_room_id;        // ID phòng thi hiện tại (-1 nếu chưa vào phòng)
    int is_authencated;         // Trạng thái xác thực (0: chưa, 1: rồi)
    pthread_mutex_t send_lock;  // Mutex bảo vệ việc gửi dữ liệu ra socket
} ClientSession;

/**
 * Khởi tạo một phiên làm việc mới cho Client
 */
ClientSession* session_create(int sockfd);

/**
 * Giải phóng tài nguyên và hủy Mutex của phiên làm việc
 */
void session_destroy(ClientSession* session);

/**
 * Hàm gửi thông điệp an toàn đa luồng
 * Tự động khóa send_lock -> gọi send_msg() -> mở khóa send_lock
 */
ssize_t reply(ClientSession* session, uint16_t opcode, const void* payload, uint32_t payload_len);

#endif
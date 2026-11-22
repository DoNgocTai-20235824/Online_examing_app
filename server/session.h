#ifndef SESSION_H
#define SESSION_H

#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <netinet/in.h>     // INET_ADDRSTRLEN
#include "protocol.h"

#define MAX_CLIENTS 1000    // Số phiên (kết nối) tối đa server phục vụ cùng lúc
                            // (giới hạn mặc định của Linux ~1024 socket, xem ulimit -n)

// Cấu trúc đại diện cho một phiên kết nối của Client
typedef struct{
    int sockfd;                 // File descriptor của socket
    char ip[INET_ADDRSTRLEN];   // IP của client (dùng cho log)
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

/* ------------------------------------------------------------------
 * Bảng phiên dùng chung giữa các thread (bảo vệ bằng session_table_lock)
 * ------------------------------------------------------------------ */

/**
 * Cấp một phiên mới cho socket và đưa vào bảng phiên.
 * IP client được lấy tự động bằng getpeername().
 * @return con trỏ phiên, hoặc NULL nếu bảng đã đầy / hết bộ nhớ.
 */
ClientSession* session_alloc(int sockfd);

/**
 * Gỡ phiên khỏi bảng rồi giải phóng bộ nhớ (không đóng socket).
 */
void session_free(ClientSession* session);

/**
 * Tìm phiên ĐÃ ĐĂNG NHẬP đang dùng username này.
 * @return con trỏ phiên, hoặc NULL nếu không có ai đăng nhập bằng tên đó.
 */
ClientSession* session_find_by_username(const char* username);

/**
 * Số phiên đang có trong bảng (đếm cả phiên chưa đăng nhập).
 */
int session_count(void);

#endif

/**
 * @file    db_query.h
 * @brief   Tầng truy cập dữ liệu: người dùng và ngân hàng câu hỏi.
 *
 * Mọi truy vấn sử dụng prepared statement (sqlite3_prepare_v2 + bind)
 * nhằm ngăn chặn tấn công SQL injection.
 */
#ifndef DB_QUERY_H
#define DB_QUERY_H

#include "hash.h"

#define MAX_USERNAME_LEN  64
#define MAX_ROLE_LEN      16
#define MAX_TEXT_LEN      512
#define MAX_OPTION_LEN    256

/** Bản ghi bảng users */
typedef struct {
    int  id;
    char username[MAX_USERNAME_LEN];
    char password_hash[SHA256_HEX_LEN];
    char role[MAX_ROLE_LEN];
} User;

/** Bản ghi bảng questions; options[0..3] tương ứng phương án A..D */
typedef struct {
    int  id;
    char content[MAX_TEXT_LEN];
    char options[4][MAX_OPTION_LEN];
    char answer;
    char difficulty[16];
} Question;

/**
 * @brief  Tra cứu người dùng theo username.
 * @param  username  Tên đăng nhập.
 * @param  out       Bản ghi kết quả.
 * @return 0: tìm thấy; 1: không tồn tại; -1: lỗi.
 */
int db_get_user_by_username(const char *username, User *out);

/**
 * @brief  Tạo tài khoản mới; mật khẩu được băm SHA-256 trước khi lưu.
 * @return id người dùng (> 0); 0: username đã tồn tại; -1: lỗi.
 */
int db_create_user(const char *username, const char *password);

/**
 * @brief  Đăng ký tài khoản (dùng cho OP_REGISTER), an toàn khi nhiều luồng gọi cùng lúc.
 *         Mật khẩu được băm SHA-256 trước khi lưu.
 * @return ST_OK (100): thành công; ST_USER_EXISTS (201): username đã tồn tại;
 *         ST_UNKNOWN_ERROR (999): lỗi CSDL.
 */
int db_register_user(const char *username, const char *password);

/**
 * @brief  Kiểm tra đăng nhập (dùng cho OP_LOGIN): so mật khẩu băm với bảng users.
 * @param  out_user_id  Nhận id người dùng khi thành công (có thể NULL).
 * @return ST_OK (100): đúng; ST_WRONG_LOGIN (202): sai username hoặc mật khẩu;
 *         ST_UNKNOWN_ERROR (999): lỗi CSDL.
 */
int db_verify_login(const char *username, const char *password, int *out_user_id);

/**
 * @brief  Xác thực thông tin đăng nhập.
 * @return id người dùng nếu hợp lệ; 0: sai thông tin; -1: lỗi.
 */
int db_check_login(const char *username, const char *password);

/**
 * @brief  Nạp ngẫu nhiên tối đa max_count câu hỏi.
 * @param  out         Mảng nhận kết quả (kích thước >= max_count).
 * @param  max_count   Số câu tối đa.
 * @param  difficulty  Mức độ lọc ("easy" | "medium" | "hard"), NULL: không lọc.
 * @return Số câu đã nạp; -1: lỗi.
 */
int db_load_questions(Question *out, int max_count, const char *difficulty);

/**
 * @brief Tạo phòng thi trong bảng rooms.
 * @param name Tên phòng.
 * @param creator_id ID tài khoản tạo phòng.
 * @param num_questions Số câu hỏi trong phòng.
 * @param duration_sec Thời lượng làm bài tính bằng giây.
 * @param out_room_id Nhận ID mới nếu không NULL.
 * @return ST_OK; ST_BAD_REQUEST nếu tham số không hợp lệ;
 *         ST_UNKNOWN_ERROR nếu có lỗi CSDL.
 */
int db_create_room(const char *name, int creator_id, int num_questions,
                   int duration_sec, int *out_room_id);

/**
 * @brief Cập nhật trạng thái phòng theo chu trình waiting -> running -> finished.
 *        Chuyển sang running sẽ gán start_time nếu chưa có.
 * @param room_id ID phòng.
 * @param status Trạng thái đích: "waiting", "running" hoặc "finished".
 * @return ST_OK; ST_BAD_REQUEST nếu tham số không hợp lệ;
 *         ST_ROOM_NOT_FOUND nếu không có phòng;
 *         ST_ROOM_BAD_STATE nếu chuyển trạng thái không hợp lệ;
 *         ST_UNKNOWN_ERROR nếu có lỗi CSDL.
 */
int db_update_room_state(int room_id, const char *status);

#endif

/**
 * @file    ui.h
 * @brief   Giao diện dòng lệnh phía client: menu, nhập liệu và các màn hình tài khoản.
 */
#ifndef UI_H
#define UI_H

#include <stddef.h>

#define UI_USERNAME_MAX 31
#define UI_PASSWORD_MAX 64
#define UI_ROOM_NAME_MAX 63

/**
 * @brief  Đọc một dòng từ bàn phím (bỏ '\n'); dòng quá dài bị bỏ phần thừa.
 * @return 0: OK; -1: dòng quá dài; -2: EOF (Ctrl+D).
 */
int ui_read_line(char *buf, size_t cap);

/**
 * @brief  Đọc một lựa chọn số nguyên.
 * @return Giá trị đã nhập; -1: không phải số; -2: EOF.
 */
int ui_read_choice(void);

/** @brief Menu khi chưa đăng nhập. */
void ui_print_guest_menu(void);

/** @brief Menu khi đã đăng nhập. */
void ui_print_user_menu(const char *username);

/**
 * @brief  Màn hình đăng ký: nhập tên + mật khẩu (nhập lại), gửi OP_REGISTER.
 * @return 0: đã xử lý xong (thành công hay thất bại); -1: mất kết nối.
 */
int ui_register_screen(int sockfd);

/**
 * @brief  Màn hình đăng nhập: nhập tên + mật khẩu, gửi OP_LOGIN.
 * @param  out_username  Nhận tên đăng nhập khi thành công (>= UI_USERNAME_MAX + 1 byte).
 * @return 1: đăng nhập thành công; 0: thất bại; -1: mất kết nối.
 */
int ui_login_screen(int sockfd, char *out_username);

/**
 * @brief  Đăng xuất: gửi OP_LOGOUT (payload rỗng).
 * @return 1: đã đăng xuất; 0: thất bại; -1: mất kết nối.
 */
int ui_logout(int sockfd);

/**
 * @brief  Nhập tên, số câu hỏi và thời lượng rồi gửi OP_CREATE_ROOM.
 * @return 0: đã xử lý xong; -1: mất kết nối.
 */
int ui_create_room(int sockfd);

#endif

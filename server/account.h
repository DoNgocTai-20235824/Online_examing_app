/**
 * @file    account.h
 * @brief   Xử lý các yêu cầu tài khoản: đăng ký (và đăng nhập / đăng xuất ở các bước sau).
 */
#ifndef ACCOUNT_H
#define ACCOUNT_H

#include "session.h"

#define USERNAME_MIN_LEN  3
#define USERNAME_MAX_LEN  31
#define PASSWORD_MIN_LEN  1
#define PASSWORD_MAX_LEN  64

/**
 * @brief  Xử lý OP_REGISTER. Payload: "username|password".
 *         Phản hồi: "100|Register OK" | "200|<lý do>" | "201|Username already exists"
 *                   | "999|Server error".
 */
void handle_register(ClientSession *s, const char *payload);

/**
 * @brief  Xử lý OP_LOGIN. Payload: "username|password".
 *         Phản hồi: "100|Login OK" | "200|<lý do>" | "202|Wrong username or password"
 *                   | "203|Account is logged in elsewhere" | "999|Server error".
 */
void handle_login(ClientSession *s, const char *payload);

#endif

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

#endif

/**
 * @file    account.c
 * @brief   Cài đặt xử lý tài khoản (xem account.h).
 */
#include "account.h"
#include "status.h"
#include "db/db_query.h"
#include <stdio.h>
#include <string.h>

/* Gửi phản hồi dạng "<mã>|<thông báo>" qua reply() (có send_lock) */
static void send_status(ClientSession *s, uint16_t op, int code, const char *text){
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "%d|%s", code, text);
    reply(s, op, buf, (uint32_t)n);
}

/* Username hợp lệ: 3–31 ký tự, chỉ chữ, số, '_', '.', '-'
   (nên tự động không chứa '|', xuống dòng hay khoảng trắng) */
static int valid_username(const char *u){
    size_t len = strlen(u);
    if(len < USERNAME_MIN_LEN || len > USERNAME_MAX_LEN) return 0;
    for(size_t i = 0; i < len; i++){
        char c = u[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
        if(!ok) return 0;
    }
    return 1;
}

/* Mật khẩu hợp lệ: 1–64 ký tự, không chứa ký tự điều khiển (\n, \r, \t...) */
static int valid_password(const char *p){
    size_t len = strlen(p);
    if(len < PASSWORD_MIN_LEN || len > PASSWORD_MAX_LEN) return 0;
    for(size_t i = 0; i < len; i++)
        if((unsigned char)p[i] < 32 || p[i] == 127) return 0;
    return 1;
}

void handle_register(ClientSession *s, const char *payload){
    /* 1. Chép payload vào bộ đệm riêng (giới hạn độ dài) để tách trường */
    char buf[USERNAME_MAX_LEN + PASSWORD_MAX_LEN + 8];
    size_t plen = payload ? strnlen(payload, sizeof(buf)) : 0;
    if(plen == 0 || plen >= sizeof(buf)){
        send_status(s, OP_REGISTER, ST_BAD_REQUEST, "Invalid format, expected username|password");
        return;
    }
    memcpy(buf, payload, plen);
    buf[plen] = '\0';

    /* 2. Tách "username|password" tại dấu '|' đầu tiên */
    char *sep = strchr(buf, '|');
    if(!sep){
        send_status(s, OP_REGISTER, ST_BAD_REQUEST, "Invalid format, expected username|password");
        return;
    }
    *sep = '\0';
    const char *username = buf;
    const char *password = sep + 1;

    /* 3. Kiểm tra dữ liệu đầu vào */
    if(!valid_username(username)){
        send_status(s, OP_REGISTER, ST_BAD_REQUEST,
                    "Username must be 3-31 chars: letters, digits, _ . -");
        return;
    }
    if(!valid_password(password)){
        send_status(s, OP_REGISTER, ST_BAD_REQUEST, "Password must be 1-64 printable chars");
        return;
    }

    /* 4. Ghi vào CSDL (băm mật khẩu bên trong); trùng tên -> 201 */
    int code = db_register_user(username, password);
    switch(code){
        case ST_OK:
            send_status(s, OP_REGISTER, ST_OK, "Register OK");
            break;
        case ST_USER_EXISTS:
            send_status(s, OP_REGISTER, ST_USER_EXISTS, "Username already exists");
            break;
        default:
            send_status(s, OP_REGISTER, ST_UNKNOWN_ERROR, "Server error");
            break;
    }
    /* Không bao giờ in mật khẩu ra màn hình / log */
    printf("[ACCOUNT] %s REGISTER user=%s -> %d\n", s->ip, username, code);
}

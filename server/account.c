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

/* Bộ đệm đủ chứa "username|password" dài nhất hợp lệ */
#define CRED_BUF_LEN (USERNAME_MAX_LEN + PASSWORD_MAX_LEN + 8)

/* Chép payload vào buf rồi tách "username|password" tại dấu '|' đầu tiên.
   @return 0 nếu đúng định dạng, -1 nếu rỗng / quá dài / thiếu dấu '|'. */
static int split_credentials(const char *payload, char buf[CRED_BUF_LEN],
                             const char **username, const char **password){
    size_t plen = payload ? strnlen(payload, CRED_BUF_LEN) : 0;
    if(plen == 0 || plen >= CRED_BUF_LEN) return -1;
    memcpy(buf, payload, plen);
    buf[plen] = '\0';

    char *sep = strchr(buf, '|');
    if(!sep) return -1;
    *sep = '\0';
    *username = buf;
    *password = sep + 1;
    return 0;
}

void handle_register(ClientSession *s, const char *payload){
    /* 1-2. Tách "username|password" */
    char buf[CRED_BUF_LEN];
    const char *username, *password;
    if(split_credentials(payload, buf, &username, &password) != 0){
        send_status(s, OP_REGISTER, ST_BAD_REQUEST, "Invalid format, expected username|password");
        return;
    }

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

void handle_login(ClientSession *s, const char *payload){
    /* 0. Kết nối này đã đăng nhập rồi thì phải đăng xuất trước */
    if(s->is_authencated){
        send_status(s, OP_LOGIN, ST_ALREADY_LOGGED, "Already logged in on this connection, logout first");
        return;
    }

    /* 1. Tách "username|password" */
    char buf[CRED_BUF_LEN];
    const char *username, *password;
    if(split_credentials(payload, buf, &username, &password) != 0 ||
       username[0] == '\0' || password[0] == '\0'){
        send_status(s, OP_LOGIN, ST_BAD_REQUEST, "Invalid format, expected username|password");
        return;
    }

    /* 2. So mật khẩu băm trong CSDL -> sai thì 202 */
    int user_id = -1;
    int code = db_verify_login(username, password, &user_id);
    if(code == ST_WRONG_LOGIN){
        send_status(s, OP_LOGIN, ST_WRONG_LOGIN, "Wrong username or password");
    } else if(code != ST_OK){
        send_status(s, OP_LOGIN, ST_UNKNOWN_ERROR, "Server error");
    }
    /* 3. Đúng mật khẩu: gắn tài khoản vào phiên; đang đăng nhập nơi khác thì 203.
          (Kiểm tra mật khẩu TRƯỚC để người lạ không dò được ai đang online.) */
    else if(session_login(s, user_id, username) != 0){
        code = ST_ALREADY_LOGGED;
        send_status(s, OP_LOGIN, ST_ALREADY_LOGGED, "Account is logged in elsewhere");
    } else {
        send_status(s, OP_LOGIN, ST_OK, "Login OK");
    }
    printf("[ACCOUNT] %s LOGIN user=%s -> %d\n", s->ip, username, code);
}

void handle_logout(ClientSession *s){
    /* Chưa đăng nhập thì không có gì để đăng xuất */
    if(!s->is_authencated){
        send_status(s, OP_LOGOUT, ST_NOT_LOGGED_IN, "Not logged in");
        return;
    }

    char username[sizeof(s->username)];
    snprintf(username, sizeof(username), "%s", s->username);

    /* Xóa trạng thái đăng nhập; kết nối vẫn mở, client quay về menu Đăng ký / Đăng nhập */
    session_logout(s);
    send_status(s, OP_LOGOUT, ST_OK, "Bye");
    printf("[ACCOUNT] %s LOGOUT user=%s -> %d\n", s->ip, username, ST_OK);
}

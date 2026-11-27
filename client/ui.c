/**
 * @file    ui.c
 * @brief   Cài đặt giao diện dòng lệnh phía client (xem ui.h).
 */
#include "ui.h"
#include "client_net.h"
#include "protocol.h"
#include "status.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* ------------------------------------------------------------------
 * Nhập liệu
 * ------------------------------------------------------------------ */

int ui_read_line(char *buf, size_t cap){
    if(!fgets(buf, (int)cap, stdin)) return -2;
    size_t len = strlen(buf);
    if(len > 0 && buf[len - 1] == '\n'){
        buf[len - 1] = '\0';
        if(len > 1 && buf[len - 2] == '\r') buf[len - 2] = '\0';
        return 0;
    }
    /* Không thấy '\n' -> dòng dài hơn bộ đệm: bỏ phần còn lại của dòng */
    int c;
    while((c = getchar()) != '\n' && c != EOF) {}
    return -1;
}

int ui_read_choice(void){
    char line[32];
    int rc = ui_read_line(line, sizeof(line));
    if(rc == -2) return -2;
    if(rc == -1) return -1;
    char *end;
    long v = strtol(line, &end, 10);
    if(end == line) return -1;
    while(*end == ' ' || *end == '\t') end++;
    if(*end != '\0') return -1;        /* "1abc" không hợp lệ */
    return (int)v;
}

/* Nhập mật khẩu: tắt hiển thị ký tự trên terminal (giống lệnh sudo) */
static int read_password(const char *prompt, char *buf, size_t cap){
    printf("%s", prompt);
    fflush(stdout);

    struct termios old_t, new_t;
    int is_tty = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &old_t) == 0;
    if(is_tty){
        new_t = old_t;
        new_t.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_t);
    }
    int rc = ui_read_line(buf, cap);
    if(is_tty){
        tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
        printf("\n");
    }
    return rc;
}

/* Kiểm tra tên đăng nhập ngay ở client (server vẫn kiểm tra lại) */
static int valid_username(const char *u){
    size_t len = strlen(u);
    if(len < 3 || len > UI_USERNAME_MAX) return 0;
    for(size_t i = 0; i < len; i++){
        char c = u[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
        if(!ok) return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------
 * Menu và thông báo
 * ------------------------------------------------------------------ */

void ui_print_guest_menu(void){
    printf("\n========== THI TRẮC NGHIỆM ONLINE ==========\n");
    printf("  1. Đăng ký\n");
    printf("  2. Đăng nhập\n");
    printf("  0. Thoát\n");
    printf("============================================\n");
    printf("Lựa chọn: ");
    fflush(stdout);
}

void ui_print_user_menu(const char *username){
    printf("\n========== Xin chào, %s ==========\n", username);
    printf("  1. Luyện tập\n");
    printf("  2. Tạo phòng thi\n");
    printf("  3. Danh sách phòng\n");
    printf("  4. Vào phòng\n");
    printf("  5. Bắt đầu thi (chủ phòng)\n");
    printf("  6. Xem kết quả phòng\n");
    printf("  7. Đăng xuất\n");
    printf("  0. Thoát\n");
    printf("============================================\n");
    printf("Lựa chọn: ");
    fflush(stdout);
}

/* Đổi mã trạng thái server trả về thành câu tiếng Việt dễ hiểu */
static const char *status_text(int code){
    switch(code){
        case ST_OK:              return "Thành công.";
        case ST_BAD_REQUEST:     return "Dữ liệu không hợp lệ.";
        case ST_USER_EXISTS:     return "Tên đăng nhập đã tồn tại, hãy chọn tên khác.";
        case ST_WRONG_LOGIN:     return "Sai tên đăng nhập hoặc mật khẩu.";
        case ST_ALREADY_LOGGED:  return "Tài khoản đang đăng nhập ở nơi khác.";
        case ST_NOT_LOGGED_IN:   return "Bạn chưa đăng nhập.";
        case ST_ROOM_BAD_STATE:  return "Bạn đang ở trong một phòng khác.";
        default:                 return "Lỗi không xác định từ server.";
    }
}

/* In kết quả: [OK]/[LỖI] + câu tiếng Việt + (mã | thông báo gốc của server) */
static void print_result(int code, const char *resp){
    const char *detail = strchr(resp, '|');
    detail = detail ? detail + 1 : resp;
    printf("%s %s (%d: %s)\n", code == ST_OK ? "[OK]" : "[LỖI]",
           status_text(code), code, detail);
}

/* ------------------------------------------------------------------
 * Màn hình tài khoản
 * ------------------------------------------------------------------ */

int ui_register_screen(int sockfd){
    char user[UI_USERNAME_MAX + 2], pass[UI_PASSWORD_MAX + 2], pass2[UI_PASSWORD_MAX + 2];

    printf("\n----- ĐĂNG KÝ -----\n");
    printf("Tên đăng nhập (3-31 ký tự: chữ, số, _ . -): ");
    fflush(stdout);
    int rc = ui_read_line(user, sizeof(user));
    if(rc == -2) return 0;
    if(rc == -1 || !valid_username(user)){
        printf("[LỖI] Tên đăng nhập không hợp lệ.\n");
        return 0;
    }

    rc = read_password("Mật khẩu (1-64 ký tự): ", pass, sizeof(pass));
    if(rc == -2) return 0;
    if(rc == -1 || pass[0] == '\0'){
        printf("[LỖI] Mật khẩu phải dài 1-64 ký tự.\n");
        return 0;
    }
    if(read_password("Nhập lại mật khẩu: ", pass2, sizeof(pass2)) != 0 || strcmp(pass, pass2) != 0){
        printf("[LỖI] Hai lần nhập mật khẩu không khớp.\n");
        return 0;
    }

    char payload[sizeof(user) + sizeof(pass) + 2], resp[256];
    snprintf(payload, sizeof(payload), "%s|%s", user, pass);
    int code = client_request(sockfd, OP_REGISTER, payload, resp, sizeof(resp));
    if(code < 0) return -1;

    print_result(code, resp);
    if(code == ST_OK) printf("Bây giờ bạn có thể đăng nhập với tài khoản '%s'.\n", user);
    return 0;
}

int ui_login_screen(int sockfd, char *out_username){
    char user[UI_USERNAME_MAX + 2], pass[UI_PASSWORD_MAX + 2];

    printf("\n----- ĐĂNG NHẬP -----\n");
    printf("Tên đăng nhập: ");
    fflush(stdout);
    int rc = ui_read_line(user, sizeof(user));
    if(rc == -2) return 0;
    size_t ulen = strlen(user);
    if(rc == -1 || ulen == 0 || ulen > UI_USERNAME_MAX){
        printf("[LỖI] Tên đăng nhập không hợp lệ.\n");
        return 0;
    }
    rc = read_password("Mật khẩu: ", pass, sizeof(pass));
    if(rc == -2) return 0;
    if(rc == -1 || pass[0] == '\0'){
        printf("[LỖI] Mật khẩu không hợp lệ.\n");
        return 0;
    }

    char payload[sizeof(user) + sizeof(pass) + 2], resp[256];
    snprintf(payload, sizeof(payload), "%s|%s", user, pass);
    int code = client_request(sockfd, OP_LOGIN, payload, resp, sizeof(resp));
    if(code < 0) return -1;

    print_result(code, resp);
    if(code != ST_OK) return 0;
    memcpy(out_username, user, ulen + 1);   /* ulen <= UI_USERNAME_MAX đã kiểm tra ở trên */
    return 1;
}

int ui_logout(int sockfd){
    char resp[256];
    int code = client_request(sockfd, OP_LOGOUT, NULL, resp, sizeof(resp));
    if(code < 0) return -1;
    print_result(code, resp);
    return code == ST_OK;
}

static int parse_positive_int(const char *text, int min, int max, int *out){
    if(!text || !text[0] || !out) return 0;
    for(const unsigned char *p = (const unsigned char *)text; *p; p++)
        if(*p < '0' || *p > '9') return 0;

    errno = 0;
    char *end;
    long value = strtol(text, &end, 10);
    if(errno == ERANGE || *end != '\0' || value < min || value > max)
        return 0;
    *out = (int)value;
    return 1;
}

int ui_create_room(int sockfd){
    char name[UI_ROOM_NAME_MAX + 2];
    char line[32];
    int num_questions, duration;

    printf("\n----- TẠO PHÒNG THI -----\n");
    printf("Tên phòng (1-%d ký tự): ", UI_ROOM_NAME_MAX);
    fflush(stdout);
    int rc = ui_read_line(name, sizeof(name));
    if(rc == -2) return 0;
    if(rc != 0){
        printf("[LỖI] Tên phòng quá dài.\n");
        return 0;
    }

    size_t name_len = strlen(name);
    if(name_len == 0){
        printf("[LỖI] Tên phòng không được để trống.\n");
        return 0;
    }
    for(size_t i = 0; i < name_len; i++){
        unsigned char c = (unsigned char)name[i];
        if(name[i] == '|' || c < 32 || c == 127){
            printf("[LỖI] Tên phòng chứa ký tự không hợp lệ.\n");
            return 0;
        }
    }

    printf("Số câu hỏi (1-50): ");
    fflush(stdout);
    rc = ui_read_line(line, sizeof(line));
    if(rc == -2) return 0;
    if(rc != 0 || !parse_positive_int(line, 1, 50, &num_questions)){
        printf("[LỖI] Số câu hỏi phải là số nguyên từ 1 đến 50.\n");
        return 0;
    }

    printf("Thời lượng (giây, 30-3600): ");
    fflush(stdout);
    rc = ui_read_line(line, sizeof(line));
    if(rc == -2) return 0;
    if(rc != 0 || !parse_positive_int(line, 30, 3600, &duration)){
        printf("[LỖI] Thời lượng phải là số nguyên từ 30 đến 3600 giây.\n");
        return 0;
    }

    char payload[UI_ROOM_NAME_MAX + sizeof(line) * 2 + 2];
    char resp[256];
    int payload_len = snprintf(payload, sizeof(payload), "%s|%d|%d",
                               name, num_questions, duration);
    if(payload_len < 0 || (size_t)payload_len >= sizeof(payload)){
        fprintf(stderr, "[LỖI] Không thể tạo dữ liệu yêu cầu.\n");
        return 0;
    }

    int code = client_request(sockfd, OP_CREATE_ROOM, payload, resp, sizeof(resp));
    if(code < 0) return -1;
    print_result(code, resp);

    if(code == ST_OK){
        const char *room_id_text = strchr(resp, '|');
        room_id_text = room_id_text ? strchr(room_id_text + 1, '|') : NULL;
        int room_id;
        if(room_id_text &&
           parse_positive_int(room_id_text + 1, 1, INT_MAX, &room_id))
            printf("Mã phòng: %d\n", room_id);
    }
    return 0;
}

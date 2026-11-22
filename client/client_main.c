/**
 * @file    client_main.c
 * @brief   Điểm vào chương trình client: thiết lập kết nối và vòng lặp menu CLI.
 *
 * Cách dùng: ./client_app [host] [port]
 * Mặc định: host = 127.0.0.1, port = DEFAULT_PORT.
 *
 * Hai trạng thái menu:
 *   - Chưa đăng nhập: Đăng ký / Đăng nhập / Thoát
 *   - Đã đăng nhập : Luyện tập, phòng thi, ... / Đăng xuất / Thoát
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "protocol.h"
#include "client_net.h"
#include "ui.h"

int main(int argc, char *argv[]){
    const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;

    /* Server tắt giữa chừng: ghi vào socket đã đóng sinh SIGPIPE làm client chết
       im lặng. Bỏ qua tín hiệu để send_msg trả lỗi -> in "Mất kết nối". */
    signal(SIGPIPE, SIG_IGN);

    int sockfd = client_connect(host, port);
    if(sockfd < 0){
        fprintf(stderr, "Không kết nối được tới server %s:%d\n", host, port);
        return 1;
    }
    printf("Đã kết nối tới server %s:%d\n", host, port);

    char username[UI_USERNAME_MAX + 1] = "";
    int logged_in = 0;
    int running = 1;
    int rc = 0;                         /* -1 = mất kết nối tới server */

    while(running && rc >= 0){
        if(!logged_in){
            /* ----- Menu khi chưa đăng nhập ----- */
            ui_print_guest_menu();
            switch(ui_read_choice()){
                case 1:
                    rc = ui_register_screen(sockfd);
                    break;
                case 2:
                    rc = ui_login_screen(sockfd, username);
                    if(rc == 1) logged_in = 1;
                    break;
                case 0:
                case -2:                /* EOF (Ctrl+D) được xử lý như lệnh thoát */
                    running = 0;
                    break;
                default:
                    printf("Lựa chọn không hợp lệ, vui lòng thử lại.\n");
            }
        } else {
            /* ----- Menu khi đã đăng nhập ----- */
            ui_print_user_menu(username);
            int choice = ui_read_choice();
            switch(choice){
                case 1: case 2: case 3: case 4: case 5: case 6:
                    printf("Chức năng đang được phát triển.\n");
                    break;
                case 7:
                    rc = ui_logout(sockfd);
                    if(rc == 1) logged_in = 0;   /* Quay về menu Đăng ký / Đăng nhập */
                    break;
                case 0:
                case -2:
                    running = 0;
                    break;
                default:
                    printf("Lựa chọn không hợp lệ, vui lòng thử lại.\n");
            }
        }
    }

    if(rc < 0) printf("\nMất kết nối tới server.\n");
    client_disconnect(sockfd);
    printf("Đã ngắt kết nối. Tạm biệt!\n");
    return rc < 0 ? 1 : 0;
}

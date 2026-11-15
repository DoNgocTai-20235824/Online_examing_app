/**
 * @file    client_main.c
 * @brief   Điểm vào chương trình client: thiết lập kết nối và vòng lặp menu CLI.
 *
 * Cách dùng: ./client_app [host] [port]
 * Mặc định: host = 127.0.0.1, port = DEFAULT_PORT.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"
#include "client_net.h"

/**
 * @brief  Đọc một lựa chọn số nguyên từ stdin.
 * @return Giá trị đã nhập; -1: đầu vào không hợp lệ; -2: EOF.
 */
static int read_choice(void){
    char line[32];
    if(!fgets(line, sizeof(line), stdin)) return -2;
    char *end;
    long v = strtol(line, &end, 10);
    if(end == line) return -1;
    return (int)v;
}

/** @brief Hiển thị menu chính. */
static void print_main_menu(void){
    printf("\n========== THI TRẮC NGHIỆM ONLINE ==========\n");
    printf("  1. Đăng ký\n");
    printf("  2. Đăng nhập\n");
    printf("  3. Luyện tập\n");
    printf("  0. Thoát\n");
    printf("============================================\n");
    printf("Lựa chọn: ");
    fflush(stdout);
}

int main(int argc, char *argv[]){
    const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;

    int sockfd = client_connect(host, port);
    if(sockfd < 0){
        fprintf(stderr, "Không kết nối được tới server %s:%d\n", host, port);
        return 1;
    }
    printf("Đã kết nối tới server %s:%d\n", host, port);

    int running = 1;
    while(running){
        print_main_menu();
        int choice = read_choice();
        switch(choice){
            case 1:
                printf("[Đăng ký] Chức năng đang được phát triển.\n");
                break;
            case 2:
                printf("[Đăng nhập] Chức năng đang được phát triển.\n");
                break;
            case 3:
                printf("[Luyện tập] Chức năng đang được phát triển.\n");
                break;
            case 0:
            case -2:    /* EOF  được xử lý như lệnh thoát */
                running = 0;
                break;
            default:
                printf("Lựa chọn không hợp lệ, vui lòng thử lại.\n");
        }
    }

    client_disconnect(sockfd);
    printf("Đã ngắt kết nối. Tạm biệt!\n");
    return 0;
}

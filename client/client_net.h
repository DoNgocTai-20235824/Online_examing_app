/**
 * @file    client_net.h
 * @brief   Khởi tạo và giải phóng kết nối TCP phía client.
 */
#ifndef CLIENT_NET_H
#define CLIENT_NET_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief  Tạo socket TCP (IPv4) và kết nối tới server.
 * @param  host  Địa chỉ IPv4 dạng chuỗi, vd "127.0.0.1".
 * @param  port  Cổng dịch vụ.
 * @return Socket descriptor (>= 0) nếu thành công, -1 nếu thất bại.
 */
int client_connect(const char *host, int port);

/**
 * @brief  Đóng kết nối; bỏ qua descriptor không hợp lệ.
 */
void client_disconnect(int sockfd);

/**
 * @brief Gửi yêu cầu và chờ phản hồi, do luồng nhận nền chuyển tới.
 * @return Mã trạng thái đầu phản hồi; -1 nếu kết nối lỗi.
 */
int client_request(int sockfd, uint16_t opcode, const char *payload,
                   char *resp, size_t resp_cap);

#endif

#ifndef MESSAGE_H
#define MESSAGE_H

#include "protocol.h"
#include <sys/types.h>

/**
 * Gửi thông điệp trọn vẹn (Header + Payload) qua socket
 * 
 * @param sockfd Socket file descriptor
 * @param opcode Mã lệnh cần gửi
 * @param payload Con trỏ chứa dữ liệu (có thể NULL nếu payload_len = 0)
 * @param payload_len Độ dài của dữ liệu payload (bytes)
 * @return Tổng số bytes đã gửi thành công, hoặc -1 nếu lỗi socket
 */
ssize_t send_msg(int sockfd, uint16_t opcode, const void* payload, uint32_t payload_len);

/**
 * Nhận thông điệp trọn vẹn (Header + Payload) từ socket
 * 
 * @param sockfd Socket file descriptor
 * @param hdr Con trỏ nhận thông tin Header (đã chuyển về Host byte order)
 * @param payload_buf Bộ đệm nhận dữ liệu payload
 * @param max_payload_len Kích thước tối đa của payload_buf
 * @return Số byte payload đã đọc (0 = gói hợp lệ có payload rỗng, vd OP_LOGOUT),
 *         hoặc -1 nếu client ngắt kết nối (EOF) / lỗi socket / gói sai.
 */
ssize_t recv_msg(int sockfd, Header *hdr, void* payload_buf, uint32_t max_payload_len);
#endif
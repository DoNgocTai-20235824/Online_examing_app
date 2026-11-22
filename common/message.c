#include "message.h"
#include "framing.h"
#include <stdio.h>
#include <string.h>

ssize_t send_msg(int sockfd, uint16_t opcode, const void* payload, uint32_t payload_len){
    Header hdr;
    hdr.opcode = opcode;
    hdr.payload_len = payload_len;

    // 1. Chuyển Header sang Network Byte Order (Big-endian)
    header_to_network(&hdr);

    // 2. Gửi Header 6 bytes
    if(writen(sockfd, &hdr, sizeof(Header)) != sizeof(Header)){
        perror("send_msg: Failed to send header");
        return -1;
    }

    // 3. Gửi phần Payload nếu có
    if(payload_len > 0 && payload != NULL){
        if(writen(sockfd, payload, payload_len) != (ssize_t)payload_len){
            perror("send msg: Failed to send payload");
            return -1;
        }
    }
    return (sizeof(Header) + payload_len);
}

ssize_t recv_msg(int sockfd, Header* hdr, void* payload_buf, uint32_t max_payload_len){
    if(!hdr) return -1;
    // 1. Đọc đủ 6 bytes Header(chống xé gói TCP)
    ssize_t ret = readn(sockfd, hdr, sizeof(Header));
    if(ret != (ssize_t)sizeof(Header)){
        return -1; // Client ngắt kết nối (EOF), header bị cắt cụt hoặc lỗi socket
    }

    // 2. Chuyển Header từ Network về Host Byte Order
    header_to_host(hdr);

    // 3. Nếu thông điệp có chứa Payload
    if(hdr->payload_len > 0){
        // Kiểm tra nguy cơ tràn bộ đệm (Buffer Overflow)
        if(hdr->payload_len > max_payload_len){
            fprintf(stderr, "recv_msg: Payload size (%u) exceeds buffer limit (%u)\n", hdr->payload_len, max_payload_len);
            return -1;
        }
        // Đọc đủ payload_len bytes của Payload
        if(payload_buf != NULL){
            ssize_t pread = readn(sockfd, payload_buf, hdr->payload_len);
            if(pread != (ssize_t)hdr->payload_len){
                fprintf(stderr, "recv_msg: Failed to read complete payload\n");
                return -1;
            }
        }
    }
    return hdr->payload_len;
}
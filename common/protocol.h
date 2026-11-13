#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define DEFAULT_PORT 9000
#define MAX_PAYLOAD_SIZE 4096

// Danh sách các Mã câu lệnh (Opcode)
typedef enum {
    OP_PING         = 100,
    OP_PONG         = 101,
    OP_REGISTER     = 1,
    OP_LOGIN        = 2,
    OP_CREATE_ROOM  = 3,
    OP_LIST_ROOMS   = 4,
    OP_JOIN_ROOM    = 5,
    OP_START_EXAM   = 6,
    OP_SUBMIT_EXAM  = 7,
    OP_PRACTICE_REQ = 8,
    OP_ERROR        = 99
} Opcode;

// Cấu trúc Header cố định 6-byte (Network Byte Order)
typedef struct __attribute__((packed)) {
    uint32_t payload_len; // Độ dài phần nội dung phía sau (bytes)
    uint16_t opcode;      // Mã câu lệnh
} Header;

// Chuyển đổi định dạng byte giữa Host và Network
void header_to_network(Header *hdr);
void header_to_host(Header *hdr);

#endif
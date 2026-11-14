/**
 * @file    hash.c
 * @brief   Cài đặt SHA-256 theo đặc tả FIPS 180-4, không phụ thuộc thư viện ngoài.
 *
 * Quy trình: tiền xử lý (padding) -> chia khối 512 bit -> mở rộng lịch thông điệp
 * W[0..63] -> 64 vòng nén trên 8 thanh ghi trạng thái a..h.
 */
#include "hash.h"
#include <stdio.h>
#include <string.h>

/* Phép quay phải 32 bit */
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

/* Hằng số vòng K[t]: 32 bit đầu phần thập phân căn bậc ba của 64 số nguyên tố đầu tiên */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/**
 * @brief  Hàm nén: cập nhật trạng thái băm h[] với một khối 512 bit.
 * @param  h    Trạng thái băm hiện hành (8 từ 32 bit).
 * @param  blk  Khối dữ liệu 64 byte.
 */
static void sha256_block(uint32_t h[8], const uint8_t blk[64]){
    uint32_t w[64];

    /* W[0..15]: khối dữ liệu đọc theo thứ tự big-endian */
    for(int i = 0; i < 16; i++){
        w[i] = ((uint32_t)blk[i*4] << 24) | ((uint32_t)blk[i*4+1] << 16)
             | ((uint32_t)blk[i*4+2] << 8) | (uint32_t)blk[i*4+3];
    }
    /* W[16..63]: mở rộng bằng các hàm sigma0, sigma1 */
    for(int i = 16; i < 64; i++){
        uint32_t s0 = ROTR(w[i-15], 7) ^ ROTR(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = ROTR(w[i-2], 17) ^ ROTR(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

    /* 64 vòng nén: Ch(e,f,g), Maj(a,b,c), Sigma0(a), Sigma1(e) */
    for(int i = 0; i < 64; i++){
        uint32_t S1 = ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = hh + S1 + ch + K[i] + w[i];
        uint32_t S0 = ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void sha256(const uint8_t *data, size_t len, uint8_t out[SHA256_DIGEST_LEN]){
    /* Giá trị khởi tạo H(0): 32 bit đầu phần thập phân căn bậc hai của 8 số nguyên tố đầu tiên */
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    /* Xử lý các khối 64 byte đầy đủ */
    size_t i = 0;
    for(; i + 64 <= len; i += 64){
        sha256_block(h, data + i);
    }

    /* Padding: phần dư || 0x80 || 0x00... || độ dài thông điệp (bit, 64 bit big-endian).
       Nếu phần dư >= 56 byte thì không đủ chỗ cho trường độ dài -> cần 2 khối. */
    uint8_t tail[128] = {0};
    size_t rem = len - i;
    memcpy(tail, data + i, rem);
    tail[rem] = 0x80;
    size_t tail_len = (rem < 56) ? 64 : 128;
    uint64_t bits = (uint64_t)len * 8;
    for(int j = 0; j < 8; j++){
        tail[tail_len - 1 - j] = (uint8_t)(bits >> (8 * j));
    }
    sha256_block(h, tail);
    if(tail_len == 128) sha256_block(h, tail + 64);

    /* Xuất digest theo thứ tự big-endian */
    for(int j = 0; j < 8; j++){
        out[j*4]   = (uint8_t)(h[j] >> 24);
        out[j*4+1] = (uint8_t)(h[j] >> 16);
        out[j*4+2] = (uint8_t)(h[j] >> 8);
        out[j*4+3] = (uint8_t)(h[j]);
    }
}

void hash_password(const char *password, char out_hex[SHA256_HEX_LEN]){
    uint8_t digest[SHA256_DIGEST_LEN];
    if(!password) password = "";
    sha256((const uint8_t *)password, strlen(password), digest);
    for(int i = 0; i < SHA256_DIGEST_LEN; i++){
        sprintf(out_hex + i * 2, "%02x", digest[i]);
    }
    out_hex[64] = '\0';
}

int verify_password(const char *password, const char *stored_hex){
    if(!stored_hex) return 0;
    char hex[SHA256_HEX_LEN];
    hash_password(password, hex);
    return strcmp(hex, stored_hex) == 0;
}

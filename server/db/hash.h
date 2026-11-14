/**
 * @file    hash.h
 * @brief   Hàm băm SHA-256 (FIPS 180-4) và tiện ích xử lý mật khẩu.
 */
#ifndef HASH_H
#define HASH_H

#include <stddef.h>
#include <stdint.h>

#define SHA256_DIGEST_LEN 32   /* Độ dài digest (byte)            */
#define SHA256_HEX_LEN    65   /* 64 ký tự hex + ký tự kết thúc   */

/**
 * @brief  Tính giá trị băm SHA-256 của một khối dữ liệu.
 * @param  data  Dữ liệu đầu vào.
 * @param  len   Độ dài dữ liệu (byte).
 * @param  out   Bộ đệm nhận digest 32 byte.
 */
void sha256(const uint8_t *data, size_t len, uint8_t out[SHA256_DIGEST_LEN]);

/**
 * @brief  Băm mật khẩu thành chuỗi hex 64 ký tự (giá trị cột users.password_hash).
 * @param  password  Mật khẩu dạng rõ (NULL được xem là chuỗi rỗng).
 * @param  out_hex   Bộ đệm nhận chuỗi kết quả.
 */
void hash_password(const char *password, char out_hex[SHA256_HEX_LEN]);

/**
 * @brief  Đối chiếu mật khẩu với giá trị băm đã lưu.
 * @return 1 nếu khớp, 0 nếu không khớp.
 */
int verify_password(const char *password, const char *stored_hex);

#endif

#ifndef FRAMING_H
#define FRAMING_H

#include <sys/types.h>

// Đọc đủ n bytes từ socket (chống xé gói TCP)
ssize_t readn(int fd, void* vptr, size_t n);

// Gửi đủ n bytes vào socket (chống dính gói TCP)
ssize_t writen(int fd, const void* vptr, size_t n);

#endif
/**
 * @file    status.h
 * @brief   Mã trạng thái đứng đầu payload phản hồi của server ("<mã>|<thông báo>").
 *          Dùng chung cho server và client.
 */
#ifndef STATUS_H
#define STATUS_H

#define ST_OK               100  /* Thành công                                   */
#define ST_BAD_REQUEST      200  /* Sai cú pháp / thiếu trường                   */
#define ST_USER_EXISTS      201  /* Username đã tồn tại                          */
#define ST_WRONG_LOGIN      202  /* Sai username hoặc mật khẩu                   */
#define ST_ALREADY_LOGGED   203  /* Tài khoản đang đăng nhập ở nơi khác          */
#define ST_NOT_LOGGED_IN    204  /* Chưa đăng nhập                               */
#define ST_NO_PERMISSION    205  /* Không có quyền                               */
#define ST_ROOM_NOT_FOUND   206  /* Phòng không tồn tại                          */
#define ST_ROOM_BAD_STATE   207  /* Phòng sai trạng thái                         */
#define ST_SUBMIT_REJECTED  208  /* Đã nộp bài rồi hoặc đã hết giờ               */
#define ST_UNKNOWN_ERROR    999  /* Lỗi không xác định                           */

#endif

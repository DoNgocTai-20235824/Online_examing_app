/**
 * @file    db.h
 * @brief   Lớp kết nối CSDL SQLite dùng chung cho tiến trình server.
 *
 * Kết nối được mở ở chế độ SQLITE_OPEN_FULLMUTEX (serialized), do đó
 * một handle duy nhất có thể được chia sẻ an toàn giữa các luồng xử lý.
 */
#ifndef DB_H
#define DB_H

#include <sqlite3.h>

#define DB_DEFAULT_PATH   "database/exam.db"
#define DB_SCHEMA_PATH    "database/schema.sql"

/**
 * @brief  Mở kết nối tới file CSDL; tạo mới nếu chưa tồn tại.
 * @param  path  Đường dẫn file CSDL (NULL -> DB_DEFAULT_PATH).
 * @return 0 nếu thành công, -1 nếu thất bại.
 */
int db_connect(const char *path);

/**
 * @brief  Đóng kết nối hiện hành. Không có tác dụng nếu chưa kết nối.
 */
void db_disconnect(void);

/**
 * @brief  Truy xuất handle SQLite đang hoạt động.
 * @return Con trỏ sqlite3, hoặc NULL nếu chưa kết nối.
 */
sqlite3 *db_handle(void);

/**
 * @brief  Thực thi toàn bộ câu lệnh SQL trong một file (vd: schema.sql).
 * @param  sql_path  Đường dẫn file SQL.
 * @return 0 nếu thành công, -1 nếu thất bại.
 */
int db_exec_file(const char *sql_path);

#endif

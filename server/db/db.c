/**
 * @file    db.c
 * @brief   Cài đặt lớp kết nối CSDL SQLite (xem db.h).
 */
#include "db.h"
#include <stdio.h>
#include <stdlib.h>

/* Handle kết nối duy nhất của tiến trình (mô hình singleton) */
static sqlite3 *g_db = NULL;

int db_connect(const char *path){
    if(g_db) return 0;
    if(!path) path = DB_DEFAULT_PATH;

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if(sqlite3_open_v2(path, &g_db, flags, NULL) != SQLITE_OK){
        fprintf(stderr, "[DB] Không mở được '%s': %s\n", path, sqlite3_errmsg(g_db));
        sqlite3_close(g_db);
        g_db = NULL;
        return -1;
    }

    /* SQLite mặc định tắt kiểm tra khóa ngoại; bật cho từng kết nối.
       busy_timeout: chờ tối đa 3000 ms khi CSDL đang bị khóa ghi. */
    sqlite3_exec(g_db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    sqlite3_busy_timeout(g_db, 3000);
    return 0;
}

void db_disconnect(void){
    if(g_db){
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

sqlite3 *db_handle(void){
    return g_db;
}

int db_exec_file(const char *sql_path){
    if(!g_db || !sql_path) return -1;

    FILE *f = fopen(sql_path, "rb");
    if(!f){
        fprintf(stderr, "[DB] Không mở được file SQL '%s'\n", sql_path);
        return -1;
    }

    /* Xác định kích thước file để cấp phát bộ đệm một lần */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if(size < 0){
        fclose(f);
        return -1;
    }

    char *sql = malloc((size_t)size + 1);
    if(!sql){
        fclose(f);
        return -1;
    }
    size_t got = fread(sql, 1, (size_t)size, f);
    sql[got] = '\0';
    fclose(f);

    /* sqlite3_exec thực thi tuần tự mọi câu lệnh phân tách bởi ';' */
    char *err = NULL;
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, &err);
    free(sql);
    if(rc != SQLITE_OK){
        fprintf(stderr, "[DB] Lỗi thực thi '%s': %s\n", sql_path, err ? err : "unknown");
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

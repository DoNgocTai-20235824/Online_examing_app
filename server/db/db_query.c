/**
 * @file    db_query.c
 * @brief   Cài đặt các truy vấn người dùng và câu hỏi (xem db_query.h).
 */
#include "db_query.h"
#include "db.h"
#include <stdio.h>
#include <string.h>

/* Sao chép giá trị cột TEXT vào bộ đệm có giới hạn; cột NULL -> chuỗi rỗng */
static void copy_col(sqlite3_stmt *st, int col, char *dst, size_t size){
    const unsigned char *s = sqlite3_column_text(st, col);
    snprintf(dst, size, "%s", s ? (const char *)s : "");
}

int db_get_user_by_username(const char *username, User *out){
    sqlite3 *db = db_handle();
    if(!db || !username || !out) return -1;

    const char *sql = "SELECT id, username, password_hash, role FROM users WHERE username = ?;";
    sqlite3_stmt *st;
    if(sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, username, -1, SQLITE_STATIC);

    int rc = sqlite3_step(st);
    int ret;
    if(rc == SQLITE_ROW){
        out->id = sqlite3_column_int(st, 0);
        copy_col(st, 1, out->username, sizeof(out->username));
        copy_col(st, 2, out->password_hash, sizeof(out->password_hash));
        copy_col(st, 3, out->role, sizeof(out->role));
        ret = 0;
    } else if(rc == SQLITE_DONE){
        ret = 1;
    } else {
        ret = -1;
    }
    sqlite3_finalize(st);
    return ret;
}

int db_create_user(const char *username, const char *password){
    sqlite3 *db = db_handle();
    if(!db || !username || !password || username[0] == '\0') return -1;

    char hex[SHA256_HEX_LEN];
    hash_password(password, hex);

    const char *sql = "INSERT INTO users (username, password_hash) VALUES (?, ?);";
    sqlite3_stmt *st;
    if(sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, hex, -1, SQLITE_STATIC);

    int rc = sqlite3_step(st);
    int ret;
    if(rc == SQLITE_DONE){
        ret = (int)sqlite3_last_insert_rowid(db);
    } else if(sqlite3_extended_errcode(db) == SQLITE_CONSTRAINT_UNIQUE){
        ret = 0;    /* Vi phạm ràng buộc UNIQUE(username) */
    } else {
        ret = -1;
    }
    sqlite3_finalize(st);
    return ret;
}

int db_check_login(const char *username, const char *password){
    User u;
    int rc = db_get_user_by_username(username, &u);
    if(rc < 0) return -1;
    if(rc == 1) return 0;
    return verify_password(password, u.password_hash) ? u.id : 0;
}

int db_load_questions(Question *out, int max_count, const char *difficulty){
    sqlite3 *db = db_handle();
    if(!db || !out || max_count <= 0) return -1;

    /* Tham số ?1 = NULL vô hiệu hóa điều kiện lọc độ khó;
       ORDER BY RANDOM() xáo trộn thứ tự câu hỏi cho mỗi lượt nạp. */
    const char *sql =
        "SELECT id, content, option_a, option_b, option_c, option_d, answer, difficulty "
        "FROM questions WHERE (?1 IS NULL OR difficulty = ?1) "
        "ORDER BY RANDOM() LIMIT ?2;";
    sqlite3_stmt *st;
    if(sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return -1;
    if(difficulty) sqlite3_bind_text(st, 1, difficulty, -1, SQLITE_STATIC);
    else           sqlite3_bind_null(st, 1);
    sqlite3_bind_int(st, 2, max_count);

    int n = 0, rc;
    while((rc = sqlite3_step(st)) == SQLITE_ROW && n < max_count){
        Question *q = &out[n];
        q->id = sqlite3_column_int(st, 0);
        copy_col(st, 1, q->content, sizeof(q->content));
        for(int i = 0; i < 4; i++){
            copy_col(st, 2 + i, q->options[i], sizeof(q->options[i]));
        }
        const unsigned char *ans = sqlite3_column_text(st, 6);
        q->answer = ans ? (char)ans[0] : '?';
        copy_col(st, 7, q->difficulty, sizeof(q->difficulty));
        n++;
    }
    sqlite3_finalize(st);
    return (rc == SQLITE_ROW || rc == SQLITE_DONE) ? n : -1;
}

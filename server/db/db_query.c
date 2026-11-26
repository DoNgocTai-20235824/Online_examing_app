/**
 * @file    db_query.c
 * @brief   Cài đặt các truy vấn người dùng, câu hỏi và phòng thi.
 */
#include "db_query.h"
#include "db.h"
#include "status.h"
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

int db_register_user(const char *username, const char *password){
    sqlite3 *db = db_handle();
    if(!db) return ST_UNKNOWN_ERROR;

    /* Giữ mutex của kết nối trong suốt INSERT + đọc mã lỗi: nếu không, luồng khác
       có thể chen vào giữa và ghi đè mã lỗi UNIQUE trước khi ta kịp đọc.
       (Mutex này là loại đệ quy nên db_create_user khóa lại bên trong vẫn an toàn.) */
    sqlite3_mutex *m = sqlite3_db_mutex(db);
    sqlite3_mutex_enter(m);
    int rc = db_create_user(username, password);
    sqlite3_mutex_leave(m);

    if(rc > 0)  return ST_OK;            /* rc = id người dùng mới            */
    if(rc == 0) return ST_USER_EXISTS;   /* Vi phạm UNIQUE(username)          */
    return ST_UNKNOWN_ERROR;
}

int db_verify_login(const char *username, const char *password, int *out_user_id){
    int id = db_check_login(username, password);   /* id > 0 | 0 sai | -1 lỗi */
    if(id < 0) return ST_UNKNOWN_ERROR;
    if(id == 0) return ST_WRONG_LOGIN;             /* Không báo rõ sai tên hay sai mật khẩu */
    if(out_user_id) *out_user_id = id;
    return ST_OK;
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

int db_create_room(const char *name, int creator_id, int num_questions,
                   int duration_sec, int *out_room_id){
    if(!name || name[0] == '\0' || creator_id <= 0 ||
       num_questions <= 0 || duration_sec <= 0)
        return ST_BAD_REQUEST;

    sqlite3 *db = db_handle();
    if(!db) return ST_UNKNOWN_ERROR;

    const char *sql =
        "INSERT INTO rooms (name, creator_id, num_questions, duration_sec) "
        "VALUES (?, ?, ?, ?);";
    sqlite3_mutex *mutex = sqlite3_db_mutex(db);
    if(mutex) sqlite3_mutex_enter(mutex);

    sqlite3_stmt *st = NULL;
    int result = ST_UNKNOWN_ERROR;
    if(sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK &&
       sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
       sqlite3_bind_int(st, 2, creator_id) == SQLITE_OK &&
       sqlite3_bind_int(st, 3, num_questions) == SQLITE_OK &&
       sqlite3_bind_int(st, 4, duration_sec) == SQLITE_OK){
        int rc = sqlite3_step(st);
        if(rc == SQLITE_DONE){
            if(out_room_id)
                *out_room_id = (int)sqlite3_last_insert_rowid(db);
            result = ST_OK;
        } else if((sqlite3_extended_errcode(db) & 0xff) == SQLITE_CONSTRAINT){
            result = ST_BAD_REQUEST;
        }
    }

    sqlite3_finalize(st);
    if(mutex) sqlite3_mutex_leave(mutex);
    return result;
}

int db_update_room_state(int room_id, const char *status){
    if(room_id <= 0 || !status ||
       (strcmp(status, "waiting") != 0 &&
        strcmp(status, "running") != 0 &&
        strcmp(status, "finished") != 0))
        return ST_BAD_REQUEST;

    sqlite3 *db = db_handle();
    if(!db) return ST_UNKNOWN_ERROR;

    sqlite3_mutex *mutex = sqlite3_db_mutex(db);
    if(mutex) sqlite3_mutex_enter(mutex);

    sqlite3_stmt *st = NULL;
    int result = ST_UNKNOWN_ERROR;
    const char *select_sql = "SELECT status FROM rooms WHERE id = ?;";
    if(sqlite3_prepare_v2(db, select_sql, -1, &st, NULL) != SQLITE_OK)
        goto done;
    if(sqlite3_bind_int(st, 1, room_id) != SQLITE_OK)
        goto done;

    int rc = sqlite3_step(st);
    if(rc == SQLITE_DONE){
        result = ST_ROOM_NOT_FOUND;
        goto done;
    }
    if(rc != SQLITE_ROW)
        goto done;

    const unsigned char *current_text = sqlite3_column_text(st, 0);
    char current[16];
    if(!current_text || snprintf(current, sizeof(current), "%s", current_text) >= (int)sizeof(current))
        goto done;
    sqlite3_finalize(st);
    st = NULL;

    if(strcmp(current, status) == 0){
        result = ST_OK;
        goto done;
    }
    if(!((strcmp(current, "waiting") == 0 && strcmp(status, "running") == 0) ||
         (strcmp(current, "running") == 0 && strcmp(status, "finished") == 0))){
        result = ST_ROOM_BAD_STATE;
        goto done;
    }

    const char *update_sql =
        "UPDATE rooms SET status = ?, "
        "start_time = CASE WHEN ? = 'running' "
        "THEN COALESCE(start_time, CURRENT_TIMESTAMP) ELSE start_time END "
        "WHERE id = ? AND status = ?;";
    if(sqlite3_prepare_v2(db, update_sql, -1, &st, NULL) != SQLITE_OK)
        goto done;
    if(sqlite3_bind_text(st, 1, status, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
       sqlite3_bind_text(st, 2, status, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
       sqlite3_bind_int(st, 3, room_id) != SQLITE_OK ||
       sqlite3_bind_text(st, 4, current, -1, SQLITE_TRANSIENT) != SQLITE_OK)
        goto done;
    if(sqlite3_step(st) != SQLITE_DONE)
        goto done;
    result = sqlite3_changes(db) == 1 ? ST_OK : ST_ROOM_BAD_STATE;

done:
    sqlite3_finalize(st);
    if(mutex) sqlite3_mutex_leave(mutex);
    return result;
}

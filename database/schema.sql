-- =============================================================
-- schema.sql
-- Lược đồ CSDL SQLite cho hệ thống thi trắc nghiệm trực tuyến.
-- Thực thể: users, questions, rooms, results.
-- Script có tính lũy đẳng (IF NOT EXISTS / INSERT OR IGNORE),
-- có thể chạy lại nhiều lần mà không làm sai lệch dữ liệu.
-- =============================================================
PRAGMA foreign_keys = ON;

-- users: tài khoản người dùng; mật khẩu lưu dưới dạng băm SHA-256 (hex)
CREATE TABLE IF NOT EXISTS users (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT    NOT NULL UNIQUE,
    password_hash TEXT    NOT NULL,
    role          TEXT    NOT NULL DEFAULT 'student'
                          CHECK (role IN ('student', 'admin')),
    created_at    DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- questions: ngân hàng câu hỏi 4 phương án, answer ∈ {A, B, C, D}
CREATE TABLE IF NOT EXISTS questions (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    content    TEXT    NOT NULL,
    option_a   TEXT    NOT NULL,
    option_b   TEXT    NOT NULL,
    option_c   TEXT    NOT NULL,
    option_d   TEXT    NOT NULL,
    answer     CHAR(1) NOT NULL CHECK (answer IN ('A', 'B', 'C', 'D')),
    difficulty TEXT    NOT NULL DEFAULT 'easy'
                       CHECK (difficulty IN ('easy', 'medium', 'hard')),
    topic      TEXT
);

-- rooms: phòng thi; status chuyển trạng thái waiting -> running -> finished
CREATE TABLE IF NOT EXISTS rooms (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    name           TEXT    NOT NULL,
    creator_id     INTEGER NOT NULL REFERENCES users(id),
    num_questions  INTEGER NOT NULL DEFAULT 10,
    duration_sec   INTEGER NOT NULL DEFAULT 600,
    status         TEXT    NOT NULL DEFAULT 'waiting'
                           CHECK (status IN ('waiting', 'running', 'finished')),
    start_time     DATETIME,
    created_at     DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- results: kết quả bài làm; room_id = NULL tương ứng chế độ luyện tập
CREATE TABLE IF NOT EXISTS results (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id      INTEGER NOT NULL REFERENCES users(id),
    room_id      INTEGER REFERENCES rooms(id),
    score        INTEGER NOT NULL DEFAULT 0,
    total        INTEGER NOT NULL DEFAULT 0,
    submitted_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Dữ liệu khởi tạo phục vụ kiểm thử
INSERT OR IGNORE INTO questions (id, content, option_a, option_b, option_c, option_d, answer, difficulty, topic) VALUES
(1, 'TCP hoạt động ở tầng nào của mô hình OSI?', 'Tầng 2', 'Tầng 3', 'Tầng 4', 'Tầng 7', 'C', 'easy', 'network'),
(2, 'Cổng mặc định của HTTP là?', '21', '80', '443', '8080', 'B', 'easy', 'network'),
(3, 'Hàm nào dùng để chuyển số nguyên 32 bit sang thứ tự byte mạng?', 'htons', 'ntohl', 'htonl', 'inet_pton', 'C', 'medium', 'socket'),
(4, 'Hàm accept() trả về giá trị gì khi thành công?', 'Địa chỉ IP client', 'Socket descriptor mới', 'Số byte nhận', '0', 'B', 'medium', 'socket'),
(5, 'Giao thức nào không hướng kết nối?', 'TCP', 'UDP', 'FTP', 'SMTP', 'B', 'easy', 'network');

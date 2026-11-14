-- Schema CSDL cho ứng dụng thi trắc nghiệm trực tuyến (SQLite)
PRAGMA foreign_keys = ON;

-- Người dùng
CREATE TABLE IF NOT EXISTS users (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT    NOT NULL UNIQUE,
    password_hash TEXT    NOT NULL,           -- SHA256 dạng hex (64 ký tự)
    role          TEXT    NOT NULL DEFAULT 'student'
                          CHECK (role IN ('student', 'admin')),
    created_at    DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Ngân hàng câu hỏi
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

-- Phòng thi
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

-- Kết quả thi
CREATE TABLE IF NOT EXISTS results (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id      INTEGER NOT NULL REFERENCES users(id),
    room_id      INTEGER REFERENCES rooms(id),   -- NULL nếu là bài luyện tập
    score        INTEGER NOT NULL DEFAULT 0,     -- số câu đúng
    total        INTEGER NOT NULL DEFAULT 0,     -- tổng số câu
    submitted_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Dữ liệu mẫu để thử nghiệm
INSERT OR IGNORE INTO questions (id, content, option_a, option_b, option_c, option_d, answer, difficulty, topic) VALUES
(1, 'TCP hoạt động ở tầng nào của mô hình OSI?', 'Tầng 2', 'Tầng 3', 'Tầng 4', 'Tầng 7', 'C', 'easy', 'network'),
(2, 'Cổng mặc định của HTTP là?', '21', '80', '443', '8080', 'B', 'easy', 'network'),
(3, 'Hàm nào dùng để chuyển số nguyên 32 bit sang thứ tự byte mạng?', 'htons', 'ntohl', 'htonl', 'inet_pton', 'C', 'medium', 'socket'),
(4, 'Hàm accept() trả về giá trị gì khi thành công?', 'Địa chỉ IP client', 'Socket descriptor mới', 'Số byte nhận', '0', 'B', 'medium', 'socket'),
(5, 'Giao thức nào không hướng kết nối?', 'TCP', 'UDP', 'FTP', 'SMTP', 'B', 'easy', 'network');

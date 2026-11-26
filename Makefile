CC = gcc
CFLAGS = -Wall -Wextra -Icommon -pthread
LDLIBS_SERVER = -lsqlite3

DB_OBJS = server/db/db.o server/db/hash.o server/db/db_query.o

# Các mục tiêu
all: server_app client_app

# Biên dịch Server
server_app: server/server_main.o server/session.o server/account.o server/room.o common/framing.o common/protocol.o common/message.o $(DB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS_SERVER)

# Biên dịch Client
client_app: client/client_main.o client/client_net.o client/ui.o common/framing.o common/protocol.o common/message.o
	$(CC) $(CFLAGS) -o $@ $^

# Phụ thuộc header của giao thức và message
client/client_main.o server/server_main.o common/protocol.o: common/protocol.h
server/room.o: server/room.h server/session.h server/db/db_query.h
common/message.o: common/message.c common/message.h common/protocol.h common/framing.h

# Biên dịch file object
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f server/*.o server/db/*.o client/*.o common/*.o server_app client_app

.PHONY: all clean

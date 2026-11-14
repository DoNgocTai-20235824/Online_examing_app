CC = gcc
CFLAGS = -Wall -Wextra -Icommon -pthread
LDLIBS_SERVER = -lsqlite3

DB_OBJS = server/db/db.o server/db/hash.o server/db/db_query.o

# Các mục tiêu
all: server_app client_app

# Biên dịch Server
server_app: server/server_main.o common/framing.o common/protocol.o $(DB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS_SERVER)

# Biên dịch Client
client_app: client/client_main.o client/client_net.o common/framing.o common/protocol.o
	$(CC) $(CFLAGS) -o $@ $^

# Biên dịch file object
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f server/*.o server/db/*.o client/*.o common/*.o server_app client_app

.PHONY: all clean
CC = gcc
CFLAGS = -Wall -Wextra -Icommon -pthread

# Các mục tiêu
all: server_app client_app

# Biên dịch Server
server_app: server/server_main.o common/framing.o common/protocol.o
	$(CC) $(CFLAGS) -o $@ $^

# Biên dịch Client
client_app: client/client_main.o common/framing.o common/protocol.o
	$(CC) $(CFLAGS) -o $@ $^

# Biên dịch file object
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f server/*.o client/*.o common/*.o server_app client_app

.PHONY: all clean
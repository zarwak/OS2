CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
SRC = src/ls-v1.0.0.c
BIN = bin/ls

all: $(BIN)

$(BIN): $(SRC)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

clean:
	rm -f $(BIN)

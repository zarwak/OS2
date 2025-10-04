CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g

# build both: main assignment ls.c and starter ls-v1.0.0.c
all: bin/ls bin/ls-v1.0.0

bin/ls: src/ls.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $<

bin/ls-v1.0.0: src/ls-v1.0.0.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f bin/ls bin/ls-v1.0.0

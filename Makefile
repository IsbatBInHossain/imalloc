CC     = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

imalloc: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c include/malloc.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o imalloc

.PHONY: clean
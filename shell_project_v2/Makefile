CC = gcc
CFLAGS = -Wall -g -Iinclude
TARGET = shell
SRCS = src/main.c src/shell.c
OBJS = $(patsubst src/%.c,object/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

object/%.o: src/%.c include/shell.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

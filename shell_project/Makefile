CC      = gcc
CFLAGS  = -Wall -Wextra -g -Iinclude
TARGET  = shell
SRCS    = src/main.c src/exec.c src/npipe.c src/builtin.c
OBJS    = $(patsubst src/%.c,object/%.o,$(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

object/%.o: src/%.c include/shell.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

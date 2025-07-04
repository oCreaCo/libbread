CC=gcc
CFLAGS= -Wall -Wextra -O2
CPPFLAGS=-I./include -D_GNU_SOURCE -DNDEBUG -DBREAD_IO -DBREAD_MEM -DOUTSTREAM=stderr
LDFLAGS=-shared
TARGET_BIN=test

SRC_DIR=src
OBJ_DIR=obj
SRCS=$(wildcard $(SRC_DIR)/*.c)
OBJS=$(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

all: build

# Build target
build: $(OBJ_DIR) libbread $(TARGET_BIN)
	sudo cp libbread.so /usr/lib

# Create obj directory if it doesn't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Build the shared library (libbread.so)
libbread: $(OBJS)
	$(CC) $(LDFLAGS) -o libbread.so $(OBJS)

# Compile bread.c with PIC option
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -fPIC -ldl $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Build the main executable and link with the shared library
$(TARGET_BIN): test.o
	$(CC) -o $(TARGET_BIN) test.o -L. -lbread

# Compile test.c
test.o:
	sudo cp include/libbread.h /usr/include
	$(CC) $(CFLAGS) $(CPPFLAGS) -c test.c -DBREAD -L. -lbread

# Clean build files
clean:
	rm -rf $(OBJ_DIR)
	rm -f test.o $(TARGET_BIN) libbread*.so

# Remove installed shared libraries and libbread.h
purge: clean
	sudo rm -f /usr/include/libbread.h
	sudo rm -f /usr/lib/libbread.so

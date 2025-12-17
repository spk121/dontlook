# Stipple VM Makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c2x -O2 -Isrc
LDFLAGS = -lm
BUILD_DIR = build
VM_EXE = $(BUILD_DIR)/stipple-vm

.PHONY: all clean

all: $(BUILD_DIR) $(VM_EXE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/vm.o: src/vm.c src/stipple.h
	$(CC) $(CFLAGS) -c src/vm.c -o $(BUILD_DIR)/vm.o

$(BUILD_DIR)/vm-main.o: src/vm-main.c src/stipple.h
	$(CC) $(CFLAGS) -c src/vm-main.c -o $(BUILD_DIR)/vm-main.o

$(VM_EXE): $(BUILD_DIR)/vm.o $(BUILD_DIR)/vm-main.o
	$(CC) $(BUILD_DIR)/vm.o $(BUILD_DIR)/vm-main.o -o $(VM_EXE) $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)

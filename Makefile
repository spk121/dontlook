# Makefile for MISRA-C compliant VM

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -pedantic -O2
AR = ar
ARFLAGS = rcs

TARGET_LIB = libvm.a
TARGET_TEST = test_vm
OBJS = vm.o

.PHONY: all clean test

all: $(TARGET_LIB) $(TARGET_TEST)

test: $(TARGET_TEST)
	./$(TARGET_TEST)

$(TARGET_LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

vm.o: vm.c vm.h
	$(CC) $(CFLAGS) -c vm.c -o vm.o

$(TARGET_TEST): test_vm.c $(TARGET_LIB)
	$(CC) $(CFLAGS) test_vm.c -L. -lvm -o $(TARGET_TEST)

clean:
	rm -f $(OBJS) $(TARGET_LIB) $(TARGET_TEST)

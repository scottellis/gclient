# Makefile for gserver

CC = gcc
CFLAGS = -Wall -O2

OBJS = main.o

TARGET = gclient

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<


.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)



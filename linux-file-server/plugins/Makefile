# Makefile for plugins
CC = gcc
CFLAGS = -Wall -fPIC -I../include

all: echo_module.so

%.so: %.c
	$(CC) $(CFLAGS) -shared -o $@ $<

clean:
	rm -f *.so

.PHONY: all clean

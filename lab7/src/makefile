CC=gcc
CFLAGS=-Wall -Wextra -O2

# Задание 1: make all собирает оба приложения (TCP и UDP)
all: tcp udp

tcp: tcpserver tcpclient
udp: udpserver udpclient

tcpserver : tcpserver.c args.h
	$(CC) -o tcpserver tcpserver.c $(CFLAGS)

tcpclient : tcpclient.c args.h
	$(CC) -o tcpclient tcpclient.c $(CFLAGS)

udpserver : udpserver.c args.h
	$(CC) -o udpserver udpserver.c $(CFLAGS)

udpclient : udpclient.c args.h
	$(CC) -o udpclient udpclient.c $(CFLAGS)

clean :
	rm -f tcpserver tcpclient udpserver udpclient

.PHONY: all tcp udp clean

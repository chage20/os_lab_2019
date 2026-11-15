CC=gcc
CFLAGS=-Wall -Wextra -O2 -I.
LDFLAGS=-pthread

# Задание 2: make all собирает и клиент, и сервер
all: server client

# Задание 3: код, общий для клиента и сервера (MultModulo, разбор чисел,
# надёжные send/recv, упаковка uint64), собран в статическую библиотеку
common.o : common.c common.h
	$(CC) -o common.o -c common.c $(CFLAGS)

libcommon.a : common.o
	ar rcs libcommon.a common.o

# Библиотека указывается ПОСЛЕ исходников: линковщик берёт из неё только то,
# что к этому моменту уже запрошено
server : server.c libcommon.a common.h
	$(CC) -o server server.c -L. -lcommon $(CFLAGS) $(LDFLAGS)

client : client.c libcommon.a common.h
	$(CC) -o client client.c -L. -lcommon $(CFLAGS) $(LDFLAGS)

clean :
	rm -f common.o libcommon.a server client

.PHONY: all clean

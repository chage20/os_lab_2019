CC=gcc
CFLAGS=-Wall -Wextra -O0
LDFLAGS=-pthread

# make all собирает все программы лабораторной
all: mutex_nolock mutex_lock factorial deadlock

# Задание 1: один и тот же mutex.c, но по-разному:
#  - без -DUSE_MUTEX критическая секция НЕ защищена (гонка данных);
#  - с -DUSE_MUTEX вызовы pthread_mutex_lock/unlock включены.
mutex_nolock : mutex.c
	$(CC) -o mutex_nolock mutex.c $(CFLAGS) $(LDFLAGS)

mutex_lock : mutex.c
	$(CC) -o mutex_lock mutex.c -DUSE_MUTEX $(CFLAGS) $(LDFLAGS)

# Задание 2
factorial : factorial.c
	$(CC) -o factorial factorial.c $(CFLAGS) $(LDFLAGS)

# Задание 3
deadlock : deadlock.c
	$(CC) -o deadlock deadlock.c $(CFLAGS) $(LDFLAGS)

clean :
	rm -f mutex_nolock mutex_lock factorial deadlock

.PHONY: all clean

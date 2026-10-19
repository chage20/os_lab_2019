CC=gcc
CFLAGS=-I. -Wall -Wextra

# Задание 4: собирает программы из Задания 1 (parallel_min_max, с поддержкой
# --timeout) и Задания 3 (process_memory). zombie_demo добавлен сюда же,
# чтобы всё собиралось одной командой make all.
all: parallel_min_max process_memory zombie_demo

parallel_min_max : utils.o find_min_max.o utils.h find_min_max.h parallel_min_max.c
	$(CC) -o parallel_min_max utils.o find_min_max.o parallel_min_max.c $(CFLAGS)

process_memory : process_memory.c
	$(CC) -o process_memory process_memory.c $(CFLAGS)

zombie_demo : zombie_demo.c
	$(CC) -o zombie_demo zombie_demo.c $(CFLAGS)

utils.o : utils.c utils.h
	$(CC) -o utils.o -c utils.c $(CFLAGS)

find_min_max.o : find_min_max.c utils.h find_min_max.h
	$(CC) -o find_min_max.o -c find_min_max.c $(CFLAGS)

clean :
	rm -f utils.o find_min_max.o parallel_min_max process_memory zombie_demo min_max_*.txt

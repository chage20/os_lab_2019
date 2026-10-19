CC=gcc
CFLAGS=-I. -Wall -Wextra
LDFLAGS=-pthread

# Задание 6: makefile для parallel_sum.c. Функция подсчёта суммы вынесена
# в отдельную статическую библиотеку libsum.a (Задание 5).
all: psum

utils.o : utils.c utils.h
	$(CC) -o utils.o -c utils.c $(CFLAGS)

sum.o : sum.c sum.h
	$(CC) -o sum.o -c sum.c $(CFLAGS)

libsum.a : sum.o
	ar rcs libsum.a sum.o

psum : parallel_sum.c utils.o libsum.a sum.h utils.h
	$(CC) -o psum parallel_sum.c utils.o -L. -lsum $(CFLAGS) $(LDFLAGS)

clean :
	rm -f utils.o sum.o libsum.a psum

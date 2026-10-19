#ifndef SUM_H
#define SUM_H

struct SumArgs {
  int *array;
  int begin;
  int end;
};

// Считает сумму элементов array[begin..end)
int Sum(const struct SumArgs *args);

// Обёртка над Sum с сигнатурой, которую ожидает pthread_create:
// void *(*)(void *). Возвращает сумму, упакованную в void*.
void *ThreadSum(void *args);

#endif

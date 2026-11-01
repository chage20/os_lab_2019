#include <getopt.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Задание 2: параллельное вычисление k! по модулю mod на pnum потоках.
//
// Идея: k! = 1 * 2 * ... * k. Умножение ассоциативно, поэтому диапазон
// [1, k] можно разбить на pnum непрерывных кусков, каждый поток перемножит
// числа своего куска (по модулю mod), а затем результаты потоков нужно
// перемножить между собой. Общая переменная result - разделяемый ресурс,
// поэтому её обновление - критическая секция, защищённая мьютексом.

static uint64_t result = 1;  // общий результат (1 - нейтральный элемент умножения)
static uint64_t mod_value = 1;
static pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;

struct Range {
  uint64_t begin;  // включительно
  uint64_t end;    // включительно
};

// Умножение по модулю через 128-битный тип: a * b не переполнится даже
// если mod близок к 2^64.
static uint64_t MulMod(uint64_t a, uint64_t b, uint64_t mod) {
  return (uint64_t)(((unsigned __int128)a * b) % mod);
}

static void *FactorialPart(void *arg) {
  const struct Range *range = (const struct Range *)arg;

  // Локальное произведение считаем БЕЗ блокировки - у каждого потока своя
  // локальная переменная, гонки нет, а мьютекс держим как можно короче.
  uint64_t local = 1 % mod_value;
  for (uint64_t i = range->begin; i <= range->end; i++) {
    local = MulMod(local, i % mod_value, mod_value);
  }

  // Критическая секция: только обновление общего результата.
  pthread_mutex_lock(&result_mutex);
  result = MulMod(result, local, mod_value);
  pthread_mutex_unlock(&result_mutex);

  return NULL;
}

static bool ParsePositive(const char *s, uint64_t *out) {
  char *endptr = NULL;
  long long v = strtoll(s, &endptr, 10);
  if (*s == '\0' || *endptr != '\0' || v < 0) return false;
  *out = (uint64_t)v;
  return true;
}

int main(int argc, char **argv) {
  uint64_t k = 0, pnum = 0, mod = 0;
  bool has_k = false, has_pnum = false, has_mod = false;

  // Пример запуска: ./factorial -k 10 --pnum=4 --mod=10
  static struct option options[] = {{"pnum", required_argument, 0, 'p'},
                                    {"mod", required_argument, 0, 'm'},
                                    {"k", required_argument, 0, 'k'},
                                    {0, 0, 0, 0}};
  int c;
  while ((c = getopt_long(argc, argv, "k:", options, NULL)) != -1) {
    switch (c) {
      case 'k':
        if (!ParsePositive(optarg, &k)) {
          printf("k must be a non-negative integer\n");
          return 1;
        }
        has_k = true;
        break;
      case 'p':
        if (!ParsePositive(optarg, &pnum) || pnum == 0) {
          printf("pnum must be a positive integer\n");
          return 1;
        }
        has_pnum = true;
        break;
      case 'm':
        if (!ParsePositive(optarg, &mod) || mod == 0) {
          printf("mod must be a positive integer\n");
          return 1;
        }
        has_mod = true;
        break;
      default:
        break;
    }
  }

  if (!has_k || !has_pnum || !has_mod) {
    printf("Usage: %s -k <num> --pnum=<num> --mod=<num>\n", argv[0]);
    return 1;
  }

  mod_value = mod;
  result = 1 % mod;  // при mod == 1 любой остаток равен 0

  // Потоков больше, чем множителей, не нужно (при k == 0 множителей нет).
  uint64_t threads_count = pnum;
  if (k > 0 && threads_count > k) threads_count = k;
  if (k == 0) threads_count = 0;

  pthread_t *threads = malloc(sizeof(pthread_t) * (threads_count + 1));
  struct Range *ranges = malloc(sizeof(struct Range) * (threads_count + 1));
  if (threads == NULL || ranges == NULL) {
    printf("Allocation error\n");
    return 1;
  }

  // Делим [1, k] на threads_count кусков; остаток достаётся последнему.
  uint64_t chunk = threads_count ? k / threads_count : 0;
  for (uint64_t i = 0; i < threads_count; i++) {
    ranges[i].begin = i * chunk + 1;
    ranges[i].end = (i == threads_count - 1) ? k : (i + 1) * chunk;
    if (pthread_create(&threads[i], NULL, FactorialPart, &ranges[i]) != 0) {
      printf("Error: pthread_create failed!\n");
      return 1;
    }
  }

  for (uint64_t i = 0; i < threads_count; i++) {
    pthread_join(threads[i], NULL);
  }

  printf("%llu! mod %llu = %llu\n", (unsigned long long)k,
         (unsigned long long)mod, (unsigned long long)result);

  free(threads);
  free(ranges);
  return 0;
}

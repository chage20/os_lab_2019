#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include <sys/time.h>

#include <getopt.h>

#include "sum.h"
#include "utils.h"

int main(int argc, char **argv) {
  /*
   *  threads_num, array_size, seed - из аргументов командной строки:
   *  ./psum --threads_num "num" --seed "num" --array_size "num"
   */
  int threads_num = -1;
  int array_size = -1;
  int seed = -1;

  while (true) {
    static struct option options[] = {{"threads_num", required_argument, 0, 0},
                                      {"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            threads_num = atoi(optarg);
            if (threads_num <= 0) {
              printf("threads_num is a positive number\n");
              return 1;
            }
            break;
          case 1:
            seed = atoi(optarg);
            if (seed <= 0) {
              printf("seed is a positive number\n");
              return 1;
            }
            break;
          case 2:
            array_size = atoi(optarg);
            if (array_size <= 0) {
              printf("array_size is a positive number\n");
              return 1;
            }
            break;
          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;

      case '?':
        break;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (threads_num == -1 || seed == -1 || array_size == -1) {
    printf("Usage: %s --threads_num \"num\" --seed \"num\" --array_size \"num\"\n",
           argv[0]);
    return 1;
  }

  // Не имеет смысла создавать больше потоков, чем элементов в массиве
  if (threads_num > array_size) threads_num = array_size;

  uint32_t threads_num_u = (uint32_t)threads_num;
  uint32_t array_size_u = (uint32_t)array_size;
  pthread_t threads[threads_num_u];

  // Генерация массива идёт ДО старта замера времени - в замер должен попасть
  // только собственно параллельный подсчёт суммы.
  int *array = malloc(sizeof(int) * array_size_u);
  GenerateArray(array, array_size_u, (uint32_t)seed);

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  int chunk_size = array_size / threads_num;
  struct SumArgs args[threads_num_u];

  for (uint32_t i = 0; i < threads_num_u; i++) {
    args[i].array = array;
    args[i].begin = (int)i * chunk_size;
    args[i].end = ((int)i == threads_num - 1) ? array_size
                                               : ((int)i + 1) * chunk_size;

    if (pthread_create(&threads[i], NULL, ThreadSum, (void *)&args[i])) {
      printf("Error: pthread_create failed!\n");
      return 1;
    }
  }

  int total_sum = 0;
  for (uint32_t i = 0; i < threads_num_u; i++) {
    int sum = 0;
    pthread_join(threads[i], (void **)&sum);
    total_sum += sum;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);

  printf("Total: %d\n", total_sum);
  printf("Elapsed time: %fms\n", elapsed_time);

  return 0;
}

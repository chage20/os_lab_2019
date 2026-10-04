#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

#define FILENAME_LEN 64
#define MSG_BUF_LEN 64

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  bool with_files = false;

  while (true) {
    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            if (seed <= 0) {
              printf("seed is a positive number\n");
              return 1;
            }
            break;
          case 1:
            array_size = atoi(optarg);
            if (array_size <= 0) {
              printf("array_size is a positive number\n");
              return 1;
            }
            break;
          case 2:
            pnum = atoi(optarg);
            if (pnum <= 0) {
              printf("pnum is a positive number\n");
              return 1;
            }
            break;
          case 3:
            with_files = true;
            break;

          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case 'f':
        with_files = true;
        break;

      case '?':
        break;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" \n",
           argv[0]);
    return 1;
  }

  // Не имеет смысла запускать больше процессов, чем элементов в массиве
  if (pnum > array_size) pnum = array_size;

  int *array = malloc(sizeof(int) * array_size);
  GenerateArray(array, array_size, seed);
  int active_child_processes = 0;

  // По одному каналу на каждого потомка: pipefd[i][0] - чтение, pipefd[i][1] - запись
  int pipefd[pnum][2];
  if (!with_files) {
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipefd[i]) == -1) {
        printf("Pipe failed!\n");
        return 1;
      }
    }
  }

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  // Делим массив на pnum примерно равных частей; остаток уходит последнему куску
  int chunk_size = array_size / pnum;

  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();
    if (child_pid >= 0) {
      // successful fork
      active_child_processes += 1;
      if (child_pid == 0) {
        // child process

        // parallel somehow
        unsigned int begin = (unsigned int)(i * chunk_size);
        unsigned int end = (i == pnum - 1) ? (unsigned int)array_size
                                            : (unsigned int)((i + 1) * chunk_size);

        struct MinMax local_min_max = GetMinMax(array, begin, end);

        if (with_files) {
          // use files here
          char filename[FILENAME_LEN];
          snprintf(filename, FILENAME_LEN, "min_max_%d.txt", i);

          FILE *fp = fopen(filename, "w");
          if (fp == NULL) {
            printf("Could not open file %s for writing\n", filename);
            _exit(1);
          }
          fprintf(fp, "%d %d", local_min_max.min, local_min_max.max);
          fclose(fp);
        } else {
          // use pipe here
          close(pipefd[i][0]);  // потомку не нужен конец на чтение

          char msg[MSG_BUF_LEN];
          int len = snprintf(msg, MSG_BUF_LEN, "%d %d", local_min_max.min,
                              local_min_max.max);
          write(pipefd[i][1], msg, (size_t)len + 1);
          close(pipefd[i][1]);
        }

        free(array);
        _exit(0);
      } else {
        // parent process: закрываем ненужный конец пайпа сразу после fork
        if (!with_files) close(pipefd[i][1]);
      }

    } else {
      printf("Fork failed!\n");
      return 1;
    }
  }

  while (active_child_processes > 0) {
    wait(NULL);

    active_child_processes -= 1;
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;

    if (with_files) {
      // read from files
      char filename[FILENAME_LEN];
      snprintf(filename, FILENAME_LEN, "min_max_%d.txt", i);

      FILE *fp = fopen(filename, "r");
      if (fp == NULL) {
        printf("Could not open file %s for reading\n", filename);
        return 1;
      }
      fscanf(fp, "%d %d", &min, &max);
      fclose(fp);
      remove(filename);
    } else {
      // read from pipes
      char msg[MSG_BUF_LEN];
      ssize_t bytes_read = read(pipefd[i][0], msg, MSG_BUF_LEN);
      if (bytes_read > 0) {
        sscanf(msg, "%d %d", &min, &max);
      }
      close(pipefd[i][0]);
    }

    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);

  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}

#include <ctype.h>
#include <limits.h>
#include <signal.h>
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

// Задание 1 (лаб. 4): список PID-ов потомков и их число нужны в обработчике
// SIGALRM, а туда нельзя передать произвольные аргументы - поэтому это
// глобальные переменные. sig_atomic_t - тип, с которым безопасно работать
// внутри обработчика сигнала.
static pid_t *g_child_pids = NULL;
static int g_pnum = 0;
static volatile sig_atomic_t g_timeout_fired = 0;

// Обработчик SIGALRM: вызывается асинхронно, когда истекает alarm(timeout).
// Внутри обработчика можно использовать только async-signal-safe функции
// (kill - безопасна, printf - нет), поэтому просто убиваем всех ещё не
// завершившихся потомков и поднимаем флаг для основного кода.
void HandleAlarm(int signum) {
  (void)signum;
  g_timeout_fired = 1;
  for (int i = 0; i < g_pnum; i++) {
    if (g_child_pids[i] > 0) kill(g_child_pids[i], SIGKILL);
  }
}

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  bool with_files = false;
  int timeout = -1;  // -1 означает "таймаут не задан"

  while (true) {
    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {"timeout", required_argument, 0, 0},
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
          case 4:
            timeout = atoi(optarg);
            if (timeout <= 0) {
              printf("timeout is a positive number\n");
              return 1;
            }
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
    printf(
        "Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" "
        "[--timeout \"num\"] [--by_files]\n",
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

  // Задание 1: массив PID-ов потомков. Нужен и обработчику SIGALRM (чтобы
  // разослать SIGKILL), и основному циклу (чтобы вызывать waitpid по
  // конкретному PID с WNOHANG).
  pid_t *pids = malloc(sizeof(pid_t) * pnum);
  int *statuses = malloc(sizeof(int) * pnum);
  for (int i = 0; i < pnum; i++) {
    pids[i] = -1;
    statuses[i] = -1;
  }
  g_child_pids = pids;
  g_pnum = pnum;

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  // Если задан --timeout, регистрируем обработчик SIGALRM и заводим будильник
  // ДО запуска потомков, чтобы таймаут отсчитывался от начала всей параллельной
  // работы. Если таймаут не задан - ничего не меняется, alarm не взводится,
  // и поведение программы полностью совпадает с лабораторной работой №3.
  if (timeout != -1) {
    signal(SIGALRM, HandleAlarm);
    alarm((unsigned int)timeout);
  }

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
        //if (i == pnum - 1) sleep(30);
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
        free(pids);
        free(statuses);
        _exit(0);
      } else {
        // parent process: закрываем ненужный конец пайпа сразу после fork
        if (!with_files) close(pipefd[i][1]);
        pids[i] = child_pid;
      }

    } else {
      printf("Fork failed!\n");
      return 1;
    }
  }

  if (timeout != -1) {
    // Задание 1: неблокирующий опрос через WNOHANG. Если за время опроса
    // сработает SIGALRM, обработчик пошлёт потомкам SIGKILL, и ближайший
    // следующий опрос их уже реально "соберёт" (пожнёт) как завершившихся.
    while (active_child_processes > 0) {
      for (int i = 0; i < pnum; i++) {
        if (pids[i] > 0) {
          int status;
          pid_t res = waitpid(pids[i], &status, WNOHANG);
          if (res > 0) {
            statuses[i] = status;
            pids[i] = -1;
            active_child_processes--;
          }
        }
      }
      if (active_child_processes > 0) usleep(1000);  // не грузим CPU пустым опросом
    }
  } else {
    // Таймаут не задан - поведение полностью как в лабораторной работе №3
    while (active_child_processes > 0) {
      wait(NULL);
      active_child_processes -= 1;
    }
  }

  if (g_timeout_fired) {
    printf("Timeout of %d seconds reached: unfinished children were killed\n",
           timeout);
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;

    // Процесс, убитый по таймауту, ничего не успел записать - пропускаем
    // чтение его результата, чтобы не упасть на несуществующем файле или
    // не зависнуть на пустом пайпе. min/max остаются сентинелами и не
    // повлияют на итоговый min_max ниже.
    bool killed_by_timeout = timeout != -1 && statuses[i] != -1 &&
                              WIFSIGNALED(statuses[i]);

    if (!killed_by_timeout) {
      if (with_files) {
        // read from files
        char filename[FILENAME_LEN];
        snprintf(filename, FILENAME_LEN, "min_max_%d.txt", i);

        FILE *fp = fopen(filename, "r");
        if (fp == NULL) {
          printf("Process %d: could not open file %s (likely killed by timeout)\n",
                 i, filename);
        } else {
          fscanf(fp, "%d %d", &min, &max);
          fclose(fp);
          remove(filename);
        }
      } else {
        // read from pipes
        char msg[MSG_BUF_LEN];
        ssize_t bytes_read = read(pipefd[i][0], msg, MSG_BUF_LEN);
        if (bytes_read > 0) {
          sscanf(msg, "%d %d", &min, &max);
        }
        close(pipefd[i][0]);
      }
    }

    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);
  free(pids);
  free(statuses);

  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}

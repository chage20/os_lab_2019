#include <getopt.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

// Задание работы одного потока сервера: перемножить числа begin..end по
// модулю mod. Результат поток кладёт в поле result (так проще и безопаснее,
// чем упаковывать число в возвращаемый void*).
struct FactorialArgs {
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result;
};

// Произведение begin * (begin+1) * ... * end по модулю mod.
// Пустой диапазон (begin > end) даёт 1 % mod - нейтральный элемент умножения.
uint64_t Factorial(const struct FactorialArgs *args) {
  uint64_t ans = 1 % args->mod;  // "% mod" нужен для случая mod == 1
  for (uint64_t i = args->begin; i <= args->end; i++) {
    ans = MultModulo(ans, i % args->mod, args->mod);
    if (i == UINT64_MAX) break;  // иначе i++ переполнилось бы и цикл не кончился
  }
  return ans;
}

void *ThreadFactorial(void *args) {
  struct FactorialArgs *fargs = (struct FactorialArgs *)args;
  fargs->result = Factorial(fargs);
  return NULL;
}

// Делит [begin, end] между tnum потоками, запускает их и объединяет результаты.
// Возвращает false при ошибке создания потока.
static bool ParallelFactorial(uint64_t begin, uint64_t end, uint64_t mod,
                              unsigned tnum, uint64_t *out) {
  pthread_t *threads = malloc(sizeof(pthread_t) * tnum);
  struct FactorialArgs *args = malloc(sizeof(struct FactorialArgs) * tnum);
  if (threads == NULL || args == NULL) {
    free(threads);
    free(args);
    return false;
  }

  // Количество чисел в диапазоне (0, если диапазон пуст).
  uint64_t count = begin <= end ? end - begin + 1 : 0;
  uint64_t chunk = count / tnum;

  unsigned created = 0;
  bool ok = true;
  for (unsigned i = 0; i < tnum; i++) {
    args[i].mod = mod;
    if (count == 0 || (i != tnum - 1 && chunk == 0)) {
      // Пустой диапазон (begin > end): чисел нет вообще, либо чисел меньше,
      // чем потоков, и этому потоку не хватило.
      args[i].begin = 1;
      args[i].end = 0;
    } else {
      args[i].begin = begin + (uint64_t)i * chunk;
      // последний поток забирает остаток от деления
      args[i].end = (i == tnum - 1) ? end : args[i].begin + chunk - 1;
    }

    if (pthread_create(&threads[i], NULL, ThreadFactorial, &args[i]) != 0) {
      ok = false;
      break;
    }
    created++;
  }

  // Дожидаемся всех созданных потоков и объединяем результаты по модулю.
  uint64_t total = 1 % mod;
  for (unsigned i = 0; i < created; i++) {
    pthread_join(threads[i], NULL);
    total = MultModulo(total, args[i].result, mod);
  }

  free(threads);
  free(args);
  *out = total;
  return ok;
}

int main(int argc, char **argv) {
  uint64_t tnum = 0;
  uint64_t port = 0;
  bool has_tnum = false, has_port = false;

  while (true) {
    static struct option options[] = {{"port", required_argument, 0, 0},
                                      {"tnum", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);
    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            if (!ConvertStringToUI64(optarg, &port) || port < 1 || port > 65535) {
              fprintf(stderr, "port must be a number in 1..65535\n");
              return 1;
            }
            has_port = true;
            break;
          case 1:
            if (!ConvertStringToUI64(optarg, &tnum) || tnum < 1 || tnum > 1024) {
              fprintf(stderr, "tnum must be a number in 1..1024\n");
              return 1;
            }
            has_tnum = true;
            break;
          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case '?':
        fprintf(stderr, "Unknown argument\n");
        return 1;
      default:
        fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }

  if (!has_port || !has_tnum) {
    fprintf(stderr, "Using: %s --port 20001 --tnum 4\n", argv[0]);
    return 1;
  }

  // 1. socket: создаём TCP-сокет (SOCK_STREAM - надёжный потоковый протокол).
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    fprintf(stderr, "Can not create server socket!\n");
    return 1;
  }

  struct sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons((uint16_t)port);        // порядок байт: host -> network
  server.sin_addr.s_addr = htonl(INADDR_ANY);     // слушаем на всех интерфейсах

  // SO_REUSEADDR: позволяет перезапустить сервер сразу, не дожидаясь, пока
  // ОС освободит порт после предыдущего запуска (состояние TIME_WAIT).
  int opt_val = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

  // 2. bind: привязываем сокет к порту.
  if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
    fprintf(stderr, "Can not bind to socket!\n");
    return 1;
  }

  // 3. listen: сокет начинает принимать соединения (очередь до 128 ожидающих).
  if (listen(server_fd, 128) < 0) {
    fprintf(stderr, "Could not listen on socket\n");
    return 1;
  }

  printf("Server listening at %d\n", (int)port);
  fflush(stdout);

  while (true) {
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    // 4. accept: ждём клиента; возвращает НОВЫЙ сокет для общения с ним.
    int client_fd = accept(server_fd, (struct sockaddr *)&client, &client_len);
    if (client_fd < 0) {
      fprintf(stderr, "Could not establish new connection\n");
      continue;
    }

    // Обслуживаем запросы клиента, пока он не закроет соединение.
    while (true) {
      char from_client[TASK_SIZE];
      // RecvAll дочитывает ровно TASK_SIZE байт: recv мог вернуть только
      // часть запроса, и в исходной заготовке это считалось ошибкой формата.
      int r = RecvAll(client_fd, from_client, sizeof(from_client));
      if (r == 0) break;  // клиент корректно закрыл соединение
      if (r < 0) {
        fprintf(stderr, "Client read failed or sent wrong data format\n");
        break;
      }

      uint64_t begin = UnpackU64(from_client);
      uint64_t end = UnpackU64(from_client + U64_SIZE);
      uint64_t mod = UnpackU64(from_client + 2 * U64_SIZE);

      printf("Receive: %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", begin, end, mod);
      fflush(stdout);

      // mod == 0 привёл бы к делению на ноль и аварийному завершению сервера
      // из-за одного некорректного запроса - такое соединение просто закрываем.
      if (mod == 0) {
        fprintf(stderr, "Client sent mod = 0, closing connection\n");
        break;
      }

      uint64_t total = 1;
      if (!ParallelFactorial(begin, end, mod, (unsigned)tnum, &total)) {
        fprintf(stderr, "Error: pthread_create failed!\n");
        break;
      }

      printf("Total: %" PRIu64 "\n", total);
      fflush(stdout);

      char buffer[RESPONSE_SIZE];
      PackU64(buffer, total);
      if (!SendAll(client_fd, buffer, sizeof(buffer))) {
        fprintf(stderr, "Can't send data to client\n");
        break;
      }
    }

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);  // 5. close: освобождаем сокет клиента
  }

  return 0;
}

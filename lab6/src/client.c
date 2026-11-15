#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <netdb.h>
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

struct Server {
  char ip[255];  // имя хоста или IP-адрес (в DNS имя хоста не длиннее 253 символов)
  int port;
};

// Всё, что нужно одному потоку клиента для работы со своим сервером.
// Поток читает begin/end/mod и пишет result/ok. Разные потоки пишут в РАЗНЫЕ
// структуры массива, поэтому гонок данных и мьютексов не требуется.
struct ClientTask {
  struct Server server;
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result;
  bool ok;
};

// Работа с одним сервером: connect -> send задание -> recv ответ -> close.
// Выполняется в отдельном потоке, поэтому все серверы считают ОДНОВРЕМЕННО.
static void *TalkToServer(void *arg) {
  struct ClientTask *task = (struct ClientTask *)arg;
  task->ok = false;

  // getaddrinfo вместо gethostbyname из заготовки: gethostbyname возвращает
  // указатель на общий статический буфер и небезопасна при вызове из
  // нескольких потоков одновременно, а getaddrinfo потокобезопасна.
  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", task->server.port);
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = NULL;
  if (getaddrinfo(task->server.ip, port_str, &hints, &res) != 0 || res == NULL) {
    fprintf(stderr, "[%s:%d] cannot resolve host\n", task->server.ip, task->server.port);
    return NULL;
  }

  int sck = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sck < 0) {
    fprintf(stderr, "[%s:%d] socket creation failed\n", task->server.ip, task->server.port);
    freeaddrinfo(res);
    return NULL;
  }

  if (connect(sck, res->ai_addr, res->ai_addrlen) < 0) {
    fprintf(stderr, "[%s:%d] connection failed: %s\n", task->server.ip,
            task->server.port, strerror(errno));
    close(sck);
    freeaddrinfo(res);
    return NULL;
  }
  freeaddrinfo(res);

  // Запрос: begin, end, mod в сетевом порядке байт.
  char request[TASK_SIZE];
  PackU64(request, task->begin);
  PackU64(request + U64_SIZE, task->end);
  PackU64(request + 2 * U64_SIZE, task->mod);
  if (!SendAll(sck, request, sizeof(request))) {
    fprintf(stderr, "[%s:%d] send failed\n", task->server.ip, task->server.port);
    close(sck);
    return NULL;
  }

  char response[RESPONSE_SIZE];
  if (RecvAll(sck, response, sizeof(response)) != 1) {
    fprintf(stderr, "[%s:%d] receive failed\n", task->server.ip, task->server.port);
    close(sck);
    return NULL;
  }
  close(sck);

  task->result = UnpackU64(response);
  task->ok = true;
  return NULL;
}

// Читает файл со списком серверов: по одному "ip:port" в строке. Пустые строки
// и строки, начинающиеся с '#', пропускаются. Возвращает число серверов.
static int ReadServers(const char *path, struct Server **out) {
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    fprintf(stderr, "Cannot open servers file %s: %s\n", path, strerror(errno));
    return -1;
  }

  int capacity = 8, count = 0;
  struct Server *list = malloc(sizeof(struct Server) * capacity);
  char line[512];

  while (fgets(line, sizeof(line), f) != NULL) {
    // убираем пробелы и перевод строки по краям
    char *start = line;
    while (isspace((unsigned char)*start)) start++;
    char *tail = start + strlen(start);
    while (tail > start && isspace((unsigned char)tail[-1])) *--tail = '\0';
    if (*start == '\0' || *start == '#') continue;

    // Делим по ПОСЛЕДНЕМУ двоеточию: порт - всё, что после него.
    char *colon = strrchr(start, ':');
    uint64_t port = 0;
    if (colon == NULL || colon == start || !ConvertStringToUI64(colon + 1, &port) ||
        port < 1 || port > 65535 || (size_t)(colon - start) >= sizeof(list[0].ip)) {
      fprintf(stderr, "Bad line in servers file (expected ip:port): %s\n", start);
      free(list);
      fclose(f);
      return -1;
    }

    if (count == capacity) {
      capacity *= 2;
      list = realloc(list, sizeof(struct Server) * capacity);
    }
    memcpy(list[count].ip, start, (size_t)(colon - start));
    list[count].ip[colon - start] = '\0';
    list[count].port = (int)port;
    count++;
  }
  fclose(f);

  if (count == 0) {
    fprintf(stderr, "Servers file %s contains no servers\n", path);
    free(list);
    return -1;
  }
  *out = list;
  return count;
}

int main(int argc, char **argv) {
  uint64_t k = 0, mod = 0;
  bool has_k = false, has_mod = false;
  // 255 - максимальная длина ИМЕНИ файла в Linux (NAME_MAX). Более длинный
  // путь мы теперь отклоняем явной проверкой (раньше memcpy переполнил бы буфер).
  char servers[255] = {'\0'};

  while (true) {
    static struct option options[] = {{"k", required_argument, 0, 0},
                                      {"mod", required_argument, 0, 0},
                                      {"servers", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);
    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            if (!ConvertStringToUI64(optarg, &k)) {
              fprintf(stderr, "k must be a non-negative integer\n");
              return 1;
            }
            has_k = true;
            break;
          case 1:
            if (!ConvertStringToUI64(optarg, &mod) || mod == 0) {
              fprintf(stderr, "mod must be a positive integer\n");
              return 1;
            }
            has_mod = true;
            break;
          case 2:
            if (strlen(optarg) >= sizeof(servers)) {
              fprintf(stderr, "servers path is too long (max %zu)\n", sizeof(servers) - 1);
              return 1;
            }
            strcpy(servers, optarg);
            break;
          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case '?':
        fprintf(stderr, "Arguments error\n");
        return 1;
      default:
        fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }

  // В заготовке отсутствие параметра определялось сравнением с -1; теперь для
  // этого есть отдельные флаги, поэтому значение k == UINT64_MAX тоже допустимо.
  if (!has_k || !has_mod || !strlen(servers)) {
    fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n", argv[0]);
    return 1;
  }

  struct Server *to = NULL;
  int servers_num = ReadServers(servers, &to);
  if (servers_num < 0) return 1;

  // Серверов больше, чем множителей, не нужно: лишним нечего считать.
  // При k == 0 (0! = 1) используем один сервер с пустым диапазоном.
  unsigned used = (unsigned)servers_num;
  if (k == 0) used = 1;
  else if (used > k) used = (unsigned)k;

  struct ClientTask *tasks = calloc(used, sizeof(struct ClientTask));
  pthread_t *threads = malloc(sizeof(pthread_t) * used);

  // Делим [1, k] на used частей, остаток уходит последнему серверу.
  uint64_t chunk = k / used;
  for (unsigned i = 0; i < used; i++) {
    tasks[i].server = to[i];
    tasks[i].mod = mod;
    if (k == 0) {
      tasks[i].begin = 1;
      tasks[i].end = 0;  // пустой диапазон: сервер вернёт 1 % mod
    } else {
      tasks[i].begin = (uint64_t)i * chunk + 1;
      tasks[i].end = (i == used - 1) ? k : (uint64_t)(i + 1) * chunk;
    }
  }

  // ПАРАЛЛЕЛЬНАЯ работа с серверами: сначала запускаем ВСЕ потоки, и только
  // потом ждём их. В заготовке connect/send/recv шли в цикле друг за другом,
  // и следующий сервер получал задание лишь когда предыдущий уже ответил.
  unsigned started = 0;
  for (unsigned i = 0; i < used; i++) {
    if (pthread_create(&threads[i], NULL, TalkToServer, &tasks[i]) != 0) {
      fprintf(stderr, "pthread_create failed\n");
      break;
    }
    started++;
  }
  for (unsigned i = 0; i < started; i++) pthread_join(threads[i], NULL);

  // Объединяем частичные результаты: (r1 * r2 * ... * rN) mod mod.
  uint64_t answer = 1 % mod;
  bool all_ok = (started == used);
  for (unsigned i = 0; i < started; i++) {
    if (!tasks[i].ok) {
      all_ok = false;
      continue;
    }
    answer = MultModulo(answer, tasks[i].result, mod);
  }

  free(threads);
  free(tasks);
  free(to);

  if (!all_ok) {
    fprintf(stderr, "Some servers failed, the answer cannot be computed\n");
    return 1;
  }

  printf("answer: %" PRIu64 "\n", answer);
  return 0;
}

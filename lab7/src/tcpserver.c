#include <getopt.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "args.h"

// Все константы (бывшие #define) вынесены в аргументы командной строки.
// Псевдоним типа SADDR (это не константа) заменён на обычный typedef.
typedef struct sockaddr SADDR;

static void Usage(const char *prog) {
  fprintf(stderr, "Usage: %s --port 10050 --bufsize 100\n", prog);
}

int main(int argc, char **argv) {
  unsigned long port = 0, bufsize = 0;
  bool has_port = false, has_bufsize = false;

  static struct option options[] = {{"port", required_argument, 0, 'p'},
                                    {"bufsize", required_argument, 0, 'b'},
                                    {0, 0, 0, 0}};
  int c;
  while ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {
    switch (c) {
      case 'p':
        if (!ParseUlong(optarg, 1, 65535, &port)) {
          fprintf(stderr, "port must be a number in 1..65535\n");
          return 1;
        }
        has_port = true;
        break;
      case 'b':
        if (!ParseUlong(optarg, 1, 1048576, &bufsize)) {
          fprintf(stderr, "bufsize must be a number in 1..1048576\n");
          return 1;
        }
        has_bufsize = true;
        break;
      default:
        Usage(argv[0]);
        return 1;
    }
  }
  if (!has_port || !has_bufsize) {
    Usage(argv[0]);
    return 1;
  }

  const size_t kSize = sizeof(struct sockaddr_in);

  int lfd, cfd;
  int nread;
  char *buf = malloc(bufsize);  // размер буфера теперь задаётся аргументом
  if (buf == NULL) {
    perror("malloc");
    exit(1);
  }
  struct sockaddr_in servaddr;
  struct sockaddr_in cliaddr;

  // socket: SOCK_STREAM = TCP (надёжный потоковый протокол с соединением)
  if ((lfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }

  // Позволяет перезапустить сервер сразу после остановки, не ожидая, пока
  // ОС освободит порт (иначе bind падает с "Address already in use").
  int opt_val = 1;
  setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

  memset(&servaddr, 0, kSize);
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);  // принимать на всех интерфейсах
  servaddr.sin_port = htons((uint16_t)port);

  if (bind(lfd, (SADDR *)&servaddr, kSize) < 0) {
    perror("bind");
    exit(1);
  }

  // listen: очередь до 5 ожидающих соединений
  if (listen(lfd, 5) < 0) {
    perror("listen");
    exit(1);
  }

  while (1) {
    socklen_t clilen = kSize;

    // accept блокируется, пока не придёт клиент; возвращает НОВЫЙ сокет cfd
    // для общения именно с ним. Клиенты обслуживаются строго по одному.
    if ((cfd = accept(lfd, (SADDR *)&cliaddr, &clilen)) < 0) {
      perror("accept");
      exit(1);
    }
    printf("connection established\n");
    fflush(stdout);

    // read возвращает 0, когда клиент закрыл соединение (получен FIN),
    // и -1 при ошибке (например, ECONNRESET, если пришёл RST).
    while ((nread = read(cfd, buf, bufsize)) > 0) {
      if (write(1, buf, nread) < 0) {  // вывод принятых байт в stdout
        perror("write");
        exit(1);
      }
    }

    if (nread == -1) {
      perror("read");
      exit(1);
    }
    close(cfd);
  }
}

#include <arpa/inet.h>
#include <getopt.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "args.h"

typedef struct sockaddr SADDR;

static void Usage(const char *prog) {
  fprintf(stderr, "Usage: %s --ip 127.0.0.1 --port 10050 --bufsize 100\n", prog);
}

int main(int argc, char *argv[]) {
  const char *ip = NULL;
  unsigned long port = 0, bufsize = 0;
  bool has_port = false, has_bufsize = false;

  static struct option options[] = {{"ip", required_argument, 0, 'i'},
                                    {"port", required_argument, 0, 'p'},
                                    {"bufsize", required_argument, 0, 'b'},
                                    {0, 0, 0, 0}};
  int c;
  while ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {
    switch (c) {
      case 'i':
        ip = optarg;
        break;
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
  if (ip == NULL || !has_port || !has_bufsize) {
    Usage(argv[0]);
    return 1;
  }

  const size_t kSize = sizeof(struct sockaddr_in);
  int fd;
  int nread;
  char *buf = malloc(bufsize);
  if (buf == NULL) {
    perror("malloc");
    exit(1);
  }
  struct sockaddr_in servaddr;

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket creating");
    exit(1);
  }

  memset(&servaddr, 0, kSize);
  servaddr.sin_family = AF_INET;

  // inet_pton: текстовый IP -> бинарный. 1 = успех, 0 = строка не является IP,
  // -1 = ошибка. Поэтому ошибкой считается всё, что <= 0.
  if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) {
    fprintf(stderr, "bad address: %s\n", ip);
    exit(1);
  }

  servaddr.sin_port = htons((uint16_t)port);

  // connect: трёхстороннее рукопожатие TCP. Если сервера нет - здесь и будет
  // ошибка (отказ сразу либо долгое ожидание таймаута).
  if (connect(fd, (SADDR *)&servaddr, kSize) < 0) {
    perror("connect");
    exit(1);
  }

  if (write(1, "Input message to send\n", 22) < 0) perror("write");
  while ((nread = read(0, buf, bufsize)) > 0) {
    if (write(fd, buf, nread) < 0) {
      perror("write");
      exit(1);
    }
  }

  close(fd);
  exit(0);
}

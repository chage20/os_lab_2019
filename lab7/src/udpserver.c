#include <arpa/inet.h>
#include <getopt.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "args.h"

typedef struct sockaddr SADDR;

static void Usage(const char *prog) {
  fprintf(stderr, "Usage: %s --port 20001 --bufsize 1024\n", prog);
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
        // 65507 - максимальный размер полезной нагрузки UDP-датаграммы
        if (!ParseUlong(optarg, 1, 65507, &bufsize)) {
          fprintf(stderr, "bufsize must be a number in 1..65507\n");
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

  const size_t kLen = sizeof(struct sockaddr_in);
  int sockfd, n;
  // +1 байт под завершающий '\0': в заготовке при датаграмме ровно в BUFSIZE
  // байт запись mesg[n] = 0 выходила за границу массива.
  char *mesg = malloc(bufsize + 1);
  char ipadr[INET_ADDRSTRLEN];
  if (mesg == NULL) {
    perror("malloc");
    exit(1);
  }
  struct sockaddr_in servaddr;
  struct sockaddr_in cliaddr;

  // SOCK_DGRAM = UDP: без соединений, каждый пакет самостоятельная датаграмма
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket problem");
    exit(1);
  }

  memset(&servaddr, 0, kLen);
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons((uint16_t)port);

  // bind есть, а listen/accept НЕТ: у UDP нет соединений, сервер просто
  // читает датаграммы с порта от кого угодно
  if (bind(sockfd, (SADDR *)&servaddr, kLen) < 0) {
    perror("bind problem");
    exit(1);
  }
  printf("SERVER starts...\n");
  fflush(stdout);

  while (1) {
    socklen_t len = kLen;

    // recvfrom возвращает ровно ОДНУ датаграмму и адрес отправителя. Если она
    // длиннее bufsize, лишняя часть молча отбрасывается.
    if ((n = recvfrom(sockfd, mesg, bufsize, 0, (SADDR *)&cliaddr, &len)) < 0) {
      perror("recvfrom");
      exit(1);
    }
    mesg[n] = 0;

    printf("REQUEST %s      FROM %s : %d\n", mesg,
           inet_ntop(AF_INET, (void *)&cliaddr.sin_addr.s_addr, ipadr, sizeof(ipadr)),
           ntohs(cliaddr.sin_port));
    fflush(stdout);

    // эхо: отправляем те же байты обратно по адресу, откуда пришла датаграмма
    if (sendto(sockfd, mesg, n, 0, (SADDR *)&cliaddr, len) < 0) {
      perror("sendto");
      exit(1);
    }
  }
}

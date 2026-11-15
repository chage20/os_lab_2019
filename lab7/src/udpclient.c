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
  fprintf(stderr, "Usage: %s --ip 127.0.0.1 --port 20001 --bufsize 1024\n", prog);
}

int main(int argc, char **argv) {
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
  if (ip == NULL || !has_port || !has_bufsize) {
    Usage(argv[0]);
    return 1;
  }

  const size_t kLen = sizeof(struct sockaddr_in);
  int sockfd, n, got;
  char *sendline = malloc(bufsize);
  char *recvline = malloc(bufsize + 1);  // +1 под '\0'
  if (sendline == NULL || recvline == NULL) {
    perror("malloc");
    exit(1);
  }
  struct sockaddr_in servaddr;

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons((uint16_t)port);

  // В заготовке проверялось "< 0", но inet_pton возвращает 0 для строки, не
  // являющейся IP-адресом, - такой случай пропускался.
  if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) {
    fprintf(stderr, "bad address: %s\n", ip);
    exit(1);
  }
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket problem");
    exit(1);
  }

  if (write(1, "Enter string\n", 13) < 0) perror("write");

  while ((n = read(0, sendline, bufsize)) > 0) {
    // sendto: отправить датаграмму. Для UDP успех здесь означает лишь "пакет
    // передан в сеть", а НЕ "сервер его получил".
    if (sendto(sockfd, sendline, n, 0, (SADDR *)&servaddr, kLen) == -1) {
      perror("sendto problem");
      exit(1);
    }

    // recvfrom блокируется, пока не придёт ответ. Если сервера нет или ответ
    // потерян, клиент будет ждать здесь бесконечно (таймаута в UDP нет).
    if ((got = recvfrom(sockfd, recvline, bufsize, 0, NULL, NULL)) == -1) {
      perror("recvfrom problem");
      exit(1);
    }
    // recvfrom не добавляет '\0': в заготовке printf("%s") печатал бы мусор
    // за концом принятых данных.
    recvline[got] = 0;

    printf("REPLY FROM SERVER= %s\n", recvline);
  }
  close(sockfd);
}

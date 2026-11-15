#include "common.h"

#include <endian.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod) {
  // 128-битное промежуточное значение: исходный вариант с удвоением a мог
  // переполниться при mod > 2^63.
  return (uint64_t)(((unsigned __int128)a * b) % mod);
}

bool ConvertStringToUI64(const char *str, uint64_t *val) {
  // strtoull молча принимает "-5" (превращая в огромное число), пустую строку
  // и "12abc", поэтому проверяем всё явно.
  if (str == NULL || *str == '\0' || *str == '-') return false;

  char *end = NULL;
  errno = 0;  // errno мог остаться от прошлых вызовов
  unsigned long long parsed = strtoull(str, &end, 10);
  if (errno == ERANGE || *end != '\0') return false;

  *val = parsed;
  return true;
}

bool SendAll(int fd, const void *buf, size_t len) {
  const char *p = (const char *)buf;
  while (len > 0) {
    // MSG_NOSIGNAL: если собеседник закрыл соединение, получим ошибку EPIPE,
    // а не сигнал SIGPIPE, который по умолчанию убил бы весь процесс.
    ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    p += n;
    len -= (size_t)n;
  }
  return true;
}

int RecvAll(int fd, void *buf, size_t len) {
  char *p = (char *)buf;
  size_t got = 0;
  while (got < len) {
    ssize_t n = recv(fd, p + got, len - got, 0);
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (n == 0) return got == 0 ? 0 : -1;  // закрыли до или посреди сообщения
    got += (size_t)n;
  }
  return 1;
}

void PackU64(char *dst, uint64_t value) {
  uint64_t be = htobe64(value);
  memcpy(dst, &be, sizeof(be));
}

uint64_t UnpackU64(const char *src) {
  uint64_t be;
  memcpy(&be, src, sizeof(be));
  return be64toh(be);
}

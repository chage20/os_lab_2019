#ifndef ARGS_H
#define ARGS_H

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

// Строгий разбор числа из аргумента командной строки в диапазоне [min, max].
// atoi из заготовок молча превращал "abc" в 0, а "-5" в -5; здесь любая
// некорректная строка (пустая, с минусом, с мусором после числа, вне
// диапазона) отвергается.
static inline bool ParseUlong(const char *s, unsigned long min,
                              unsigned long max, unsigned long *out) {
  if (s == NULL || *s == '\0' || *s == '-') return false;
  char *end = NULL;
  errno = 0;
  unsigned long v = strtoul(s, &end, 10);
  if (errno != 0 || *end != '\0' || v < min || v > max) return false;
  *out = v;
  return true;
}

#endif

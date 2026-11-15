#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Протокол клиент <-> сервер (одинаков для обеих программ, поэтому лежит здесь):
//   запрос  : 3 числа uint64 подряд - begin, end, mod   (TASK_SIZE байт)
//   ответ   : 1 число  uint64       - (begin * ... * end) mod mod
// Числа передаются в сетевом порядке байт (big-endian), чтобы клиент и сервер
// работали корректно даже на машинах с разной архитектурой.
#define U64_SIZE sizeof(uint64_t)
#define TASK_SIZE (U64_SIZE * 3)
#define RESPONSE_SIZE U64_SIZE

// (a * b) mod mod без переполнения даже при mod близком к 2^64.
uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod);

// Строгий разбор беззнакового числа из строки: отказ при пустой строке,
// знаке минус, мусоре после числа и переполнении.
bool ConvertStringToUI64(const char *str, uint64_t *val);

// TCP - потоковый протокол: один send/recv может передать меньше байт, чем
// запрошено. Эти функции повторяют вызов, пока не передадут всё.
bool SendAll(int fd, const void *buf, size_t len);
// Возвращает 1 - прочитано ровно len байт; 0 - соединение корректно закрыто до
// первого байта; -1 - ошибка или обрыв посреди сообщения.
int RecvAll(int fd, void *buf, size_t len);

// Запись/чтение uint64 в сетевом порядке байт.
void PackU64(char *dst, uint64_t value);
uint64_t UnpackU64(const char *src);

#endif

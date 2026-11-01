#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Задание 3: демонстрация deadlock (взаимной блокировки).
//
// Deadlock возникает, когда одновременно выполнены условия Коффмана:
//   1. взаимное исключение  - мьютекс может держать только один поток;
//   2. удержание и ожидание - поток держит один ресурс и ждёт другой;
//   3. отсутствие принудительного отъёма - мьютекс нельзя отобрать;
//   4. циклическое ожидание - поток 1 ждёт ресурс потока 2, а поток 2 -
//      ресурс потока 1.
//
// Здесь два мьютекса A и B. Поток 1 берёт их в порядке A -> B, поток 2 - в
// порядке B -> A. sleep() между захватами гарантирует, что к моменту второго
// lock() каждый поток уже держит свой первый мьютекс - и оба зависают навсегда.
//
// Запуск:
//   ./deadlock           - воспроизводит deadlock (сторож завершит программу)
//   ./deadlock --fixed   - одинаковый порядок захвата A -> B: deadlock нет

static pthread_mutex_t mutex_a = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_b = PTHREAD_MUTEX_INITIALIZER;
static int fixed_order = 0;

static void *Thread1(void *arg) {
  (void)arg;
  pthread_mutex_lock(&mutex_a);
  printf("Thread 1: locked A, waiting for B...\n");
  sleep(1);  // даём потоку 2 успеть захватить свой первый мьютекс
  pthread_mutex_lock(&mutex_b);
  printf("Thread 1: locked A and B, doing work\n");
  pthread_mutex_unlock(&mutex_b);
  pthread_mutex_unlock(&mutex_a);
  return NULL;
}

static void *Thread2(void *arg) {
  (void)arg;
  // Причина deadlock: здесь порядок обратный (B -> A).
  // В режиме --fixed порядок такой же, как у потока 1 (A -> B).
  pthread_mutex_t *first = fixed_order ? &mutex_a : &mutex_b;
  pthread_mutex_t *second = fixed_order ? &mutex_b : &mutex_a;
  const char *first_name = fixed_order ? "A" : "B";
  const char *second_name = fixed_order ? "B" : "A";

  pthread_mutex_lock(first);
  printf("Thread 2: locked %s, waiting for %s...\n", first_name, second_name);
  sleep(1);
  pthread_mutex_lock(second);
  printf("Thread 2: locked %s and %s, doing work\n", first_name, second_name);
  pthread_mutex_unlock(second);
  pthread_mutex_unlock(first);
  return NULL;
}

int main(int argc, char **argv) {
  if (argc > 1 && strcmp(argv[1], "--fixed") == 0) fixed_order = 1;

  pthread_t t1, t2;
  pthread_create(&t1, NULL, Thread1, NULL);
  pthread_create(&t2, NULL, Thread2, NULL);

  // Сторож: в обычном режиме потоки ждут друг друга вечно, и pthread_join
  // никогда не вернётся. Чтобы демонстрацию можно было спокойно запускать,
  // через 5 секунд главный поток сообщает о взаимной блокировке и выходит.
  for (int i = 0; i < 5; i++) {
    sleep(1);
    if (fixed_order) break;
  }

  if (fixed_order) {
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("Both threads finished: no deadlock\n");
    return 0;
  }

  printf("DEADLOCK: both threads are blocked forever (Thread 1 waits for B, "
         "Thread 2 waits for A)\n");
  fflush(stdout);
  _exit(1);
}

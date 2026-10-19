#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Задание 2: демонстрация зомби-процесса.
//
// Зомби (zombie / defunct) - это процесс, который уже завершился (вызвал
// exit либо получил сигнал), но запись о нём в таблице процессов ядра ещё
// не удалена, потому что родитель не вызвал wait()/waitpid() и не забрал
// код возврата. Ядро обязано хранить эту запись (PID, код завершения,
// использованные ресурсы), пока родитель её не прочитает - именно поэтому
// зомби занимает слот в таблице процессов, хоть и не потребляет CPU и
// почти не потребляет память.
int main(void) {
  pid_t child_pid = fork();

  if (child_pid < 0) {
    printf("Fork failed!\n");
    return 1;
  }

  if (child_pid == 0) {
    // Потомок мгновенно завершается...
    printf("Child (pid %d) exits immediately\n", getpid());
    _exit(0);
  }

  // ...а родитель НАРОЧНО не вызывает wait()/waitpid() и просто спит.
  // Всё это время потомок будет висеть в состоянии зомби (Z в выводе ps).
  printf("Parent (pid %d): child pid is %d\n", getpid(), child_pid);
  printf("Parent is sleeping for 60 seconds WITHOUT calling wait().\n");
  printf("In another terminal run:  ps -o pid,ppid,stat,cmd -p %d\n",
         child_pid);
  printf("You should see STAT = Z (zombie / <defunct>)\n");

  sleep(60);

  // Если досидеть до конца - наконец собираем (reap) потомка, и запись
  // зомби исчезает из таблицы процессов.
  wait(NULL);
  printf("Parent called wait(): zombie is gone\n");

  return 0;
}

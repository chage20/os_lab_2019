#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

// Задание 5: запускает sequential_min_max в отдельном процессе с помощью
// fork + exec. Аргументы (seed, array_size) просто пробрасываются дальше.
int main(int argc, char **argv) {
  if (argc != 3) {
    printf("Usage: %s seed array_size\n", argv[0]);
    return 1;
  }

  pid_t child_pid = fork();

  if (child_pid < 0) {
    printf("Fork failed!\n");
    return 1;
  }

  if (child_pid == 0) {
    // child process: заменяем образ процесса на sequential_min_max
    execl("./sequential_min_max", "sequential_min_max", argv[1], argv[2],
          (char *)NULL);

    // сюда попадаем только если exec не смог запустить программу
    printf("exec failed: make sure ./sequential_min_max is built\n");
    _exit(1);
  }

  // parent process: ждём завершения потомка и печатаем его код возврата
  int status;
  waitpid(child_pid, &status, 0);

  if (WIFEXITED(status)) {
    printf("sequential_min_max finished with exit code %d\n",
           WEXITSTATUS(status));
  }

  return 0;
}

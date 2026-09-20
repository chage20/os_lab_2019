# Лабораторная работа №2

## Структура
```
swap/           Задание 1  — Swap (swap.c)
revert_string/  Задание 2  — RevertString (revert_string.c) + main.c
tests/          Задание 4  — CUnit-тесты
Makefile        сборка всех заданий
```
Сборка: `make` — всё в папку `build/` (она в `.gitignore`, бинарники в git не попадают).
Запуск всего: `make run`. Тесты требуют CUnit: `sudo apt -y install libcunit1 libcunit1-doc libcunit1-dev`.

## Задание 1 — Swap
В Си аргументы передаются **по значению**, поэтому чтобы изменить переменные вызывающей функции, передают их адреса (указатели) и пишут через разыменование:
```c
char tmp = *left; *left = *right; *right = tmp;
```
```
gcc -o build/swap swap/main.c swap/swap.c && ./build/swap   ->   b a
```

## Задание 2 — RevertString
Два индекса (с начала и с конца) идут навстречу друг другу, обмениваясь символами, пока `i < j`. Работает in-place за O(n), корректна для пустой строки, одного символа, чётной и нечётной длины.

### Как работает main.c
1. Проверяет `argc != 2` — нужен ровно один аргумент командной строки (`argv[0]` — имя программы, `argv[1]` — строка). Иначе печатает подсказку и возвращает `-1`.
2. `malloc(strlen(argv[1]) + 1)` выделяет память **в куче** под копию строки (+1 байт под завершающий `\0`).
3. `strcpy` копирует туда аргумент. Копия нужна, потому что `argv[1]` менять нельзя/небезопасно, а `RevertString` переворачивает строку на месте.
4. `RevertString` переворачивает копию, `printf` выводит результат.
5. `free` возвращает память (иначе утечка).

`./build/revert_plain Hello` -> `Reverted: olleH`

### Стек и куча (вопрос на защите)
| | Стек | Куча |
|---|---|---|
| Что там | локальные переменные, аргументы, адреса возврата | данные, выделенные `malloc`/`calloc` |
| Управление | автоматически: освобождается при выходе из функции | вручную: `malloc` / `free` |
| Скорость | очень быстрая (сдвиг указателя стека) | медленнее (аллокатор) |
| Размер | небольшой, фиксированный (обычно ~8 МБ) → возможен stack overflow | ограничен доступной памятью |
| Время жизни | до конца функции | пока не вызвать `free` |
| Риски | переполнение стека, возврат указателя на локальную переменную | утечки, double free, use-after-free |

## Задание 3 — библиотеки
Этапы сборки: **препроцессор** (`#include`, `#define` -> единый .i) -> **компилятор** (.c -> .o, машинный код с неразрешёнными ссылками на внешние символы) -> **линковщик** (объединяет .o и библиотеки, разрешает символы, выдаёт исполняемый файл).

**Статическая** (`libreverse.a`) — код функции копируется в исполняемый файл на этапе линковки. Программа самодостаточна, но больше по размеру, а для обновления библиотеки её нужно пересобирать.
**Динамическая** (`libreverse.so`) — в exe лишь ссылка на библиотеку; загружается в момент запуска, один .so делят несколько программ, обновляется без пересборки.

```bash
# статическая
gcc -c revert_string/revert_string.c -o build/revert_string_static.o
ar rcs build/libreverse.a build/revert_string_static.o
gcc -Irevert_string revert_string/main.c -Lbuild -l:libreverse.a -o build/revert_static

# динамическая
gcc -fPIC -c revert_string/revert_string.c -o build/revert_string_pic.o
gcc -shared -o build/libreverse.so build/revert_string_pic.o
gcc -Irevert_string revert_string/main.c -Lbuild -lreverse -o build/revert_dynamic

# запуск динамической версии
LD_LIBRARY_PATH=build ./build/revert_dynamic Hello
```
Опции: `-I<dir>` — где искать заголовки; `-L<dir>` — где искать библиотеки; `-l<name>` — линковаться с `lib<name>.so/.a`; `-c` — только компиляция в .o, без линковки; `-o` — имя выходного файла; `-fPIC` — позиционно-независимый код (нужен для .so, т.к. библиотека грузится по произвольному адресу); `-shared` — собрать разделяемую библиотеку. `ar rcs` — создать архив (`r` — вставить, `c` — создать, `s` — индекс символов).

Примечание: если рядом лежат и `.a`, и `.so`, то `-lreverse` предпочтёт `.so`, поэтому для статической версии использован `-l:libreverse.a`. Проверка: `ldd build/revert_static` не показывает libreverse, `ldd build/revert_dynamic` — показывает.

## Задание 4 — CUnit
```bash
gcc -Irevert_string tests/tests.c -Lbuild -lreverse -lcunit -o build/tests
LD_LIBRARY_PATH=build ./build/tests
```
Тесты и `revert_dynamic` линкуются с **одним и тем же файлом** `build/libreverse.so`.
`LD_LIBRARY_PATH` — список каталогов, где динамический загрузчик (`ld.so`) ищет `.so` при запуске, помимо стандартных (`/lib`, `/usr/lib`, ...). Без неё будет `error while loading shared libraries: libreverse.so: cannot open shared object file`. Альтернативы: `-Wl,-rpath,<dir>` при линковке или установка в `/usr/local/lib` + `ldconfig`.

Ожидаемый вывод: `tests 1 1 1 0 0`, `asserts 4 4 4 0 n/a`.

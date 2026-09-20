CC      = gcc
CFLAGS  = -Wall -Wextra -O2
RS_DIR  = revert_string
BUILD   = build

all: swap_app static_app dynamic_app tests_app

$(BUILD):
	mkdir -p $(BUILD)

# ---------- Задание 1 ----------
swap_app: swap/main.c swap/swap.c swap/swap.h | $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/swap swap/main.c swap/swap.c

# ---------- Задание 2 (без библиотек, для проверки) ----------
revert_plain: $(RS_DIR)/main.c $(RS_DIR)/revert_string.c | $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/revert_plain $(RS_DIR)/main.c $(RS_DIR)/revert_string.c

# ---------- Задание 3 ----------
# Статическая: .c -> .o -> libreverse.a (ar), затем линковка с -L -l
$(BUILD)/revert_string_static.o: $(RS_DIR)/revert_string.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/libreverse.a: $(BUILD)/revert_string_static.o
	ar rcs $@ $<

static_app: $(RS_DIR)/main.c $(BUILD)/libreverse.a
	$(CC) $(CFLAGS) -I$(RS_DIR) $(RS_DIR)/main.c -L$(BUILD) -l:libreverse.a -o $(BUILD)/revert_static

# Динамическая: позиционно-независимый код (-fPIC) + -shared
$(BUILD)/revert_string_pic.o: $(RS_DIR)/revert_string.c | $(BUILD)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(BUILD)/libreverse.so: $(BUILD)/revert_string_pic.o
	$(CC) -shared -o $@ $<

dynamic_app: $(RS_DIR)/main.c $(BUILD)/libreverse.so
	$(CC) $(CFLAGS) -I$(RS_DIR) $(RS_DIR)/main.c -L$(BUILD) -lreverse -o $(BUILD)/revert_dynamic

# ---------- Задание 4 ----------
# Тесты линкуются с ТЕМ ЖЕ файлом build/libreverse.so, что и revert_dynamic
tests_app: tests/tests.c $(BUILD)/libreverse.so
	$(CC) $(CFLAGS) -I$(RS_DIR) tests/tests.c -L$(BUILD) -lreverse -lcunit -o $(BUILD)/tests

# Запуск (LD_LIBRARY_PATH указывает, где искать libreverse.so)
run: all
	LD_LIBRARY_PATH=$(BUILD) ./$(BUILD)/revert_dynamic "Hello"
	./$(BUILD)/revert_static "Hello"
	LD_LIBRARY_PATH=$(BUILD) ./$(BUILD)/tests

clean:
	rm -rf $(BUILD)

.PHONY: all swap_app revert_plain static_app dynamic_app tests_app run clean

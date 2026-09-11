CC        ?= gcc
CFLAGS    := -Wall -Wextra -Werror -pedantic -std=c11 -g -Iinclude -MMD -MP -D_POSIX_C_SOURCE=200809L
SANFLAGS  := -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS   :=

TARGET       := protocol7
SANTARGET    := protocol7-sanitized
SRC_DIR      := src
PYTHON       := python3
BUILD_DIR    := build
SAN_BUILD_DIR:= build-sanitize

SRCS      := $(wildcard $(SRC_DIR)/*.c)
OBJS      := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
SAN_OBJS  := $(SRCS:$(SRC_DIR)/%.c=$(SAN_BUILD_DIR)/%.o)

DEPS      := $(OBJS:.o=.d) $(SAN_OBJS:.o=.d)
-include $(DEPS)

.PHONY: all clean run valgrind compiledb sanitize

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

sanitize: $(SANTARGET)

$(SANTARGET): $(SAN_OBJS)
	$(CC) $(CFLAGS) $(SANFLAGS) $^ -o $@ $(LDFLAGS) $(SANFLAGS)

$(SAN_BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(SAN_BUILD_DIR)
	$(CC) $(CFLAGS) $(SANFLAGS) -c $< -o $@

$(SAN_BUILD_DIR):
	mkdir -p $(SAN_BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

valgrind: $(TARGET)
	valgrind --leak-check=full --track-origins=yes ./$(TARGET)

compiledb: clean
	bear -- make all

clean:
	rm -rf $(BUILD_DIR) $(SAN_BUILD_DIR) $(TARGET) $(SANTARGET)

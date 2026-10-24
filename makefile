CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude -MMD -MP
LDFLAGS = -lpthread

SRC_DIR = src
BUILD_DIR = build
TARGET_SEM = supermarket
TARGET_COND = supermarket_condvar

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJS_COND = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.cond.o)

.PHONY: all clean run test

all: $(TARGET_SEM) $(TARGET_COND)
	@echo "Build complete: $(TARGET_SEM) (semaphores) and $(TARGET_COND) (condvars)"

$(TARGET_SEM): $(BUILD_DIR) $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(TARGET_COND): $(BUILD_DIR) $(OBJS_COND)
	$(CC) $(CFLAGS) -DUSE_CONDVAR $(LDFLAGS) -o $@ $(OBJS_COND)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.cond.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -DUSE_CONDVAR -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET_SEM) $(TARGET_COND) simulation.log
	@echo "Cleaned"

run: $(TARGET_SEM)
	./$(TARGET_SEM) tests/test_small.txt

test: $(TARGET_SEM)
	@echo "Running tests..."
	@bash tests/run_tests.sh

-include $(DEPS)
CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude -MMD -MP
LDFLAGS = 

SRC_DIR = src
BUILD_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJS_NAMED = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.named.o)
DEPS = $(OBJS:.o=.d) $(OBJS_NAMED:.o=.d)

.PHONY: all clean run test help

all: supermarket supermarket_named
	@echo "Build complete: supermarket (unnamed) and supermarket_named (named)"

supermarket: $(BUILD_DIR) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

supermarket_named: $(BUILD_DIR) $(OBJS_NAMED)
	$(CC) $(CFLAGS) -DUSE_NAMED_SEM -o $@ $(OBJS_NAMED) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.named.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -DUSE_NAMED_SEM -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) supermarket supermarket_named simulation.log
	@echo "Cleaned"

run: all
	@echo "Starting independent processes via run.sh..."
	@chmod +x run.sh
	./run.sh

test: all
	@echo "Running tests..."
	@bash tests/run_tests.sh

help:
	@echo "Available targets:"
	@echo "  make all     - Build both binaries (unnamed and named semaphores)"
	@echo "  make run     - Run the simulation using run.sh (independent processes)"
	@echo "  make test    - Run the test suite"
	@echo "  make clean   - Remove build artifacts and binaries"

-include $(DEPS)
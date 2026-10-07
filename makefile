CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude -MMD -MP
LDFLAGS =

SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include

TARGET = supermarket

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

.PHONY: all clean run debug test help

all: $(TARGET)

$(TARGET): $(BUILD_DIR) $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)
	@echo "Build complete: $(TARGET)"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

run-test: $(TARGET)
	./$(TARGET) tests/test_config.txt

debug: CFLAGS += -g -O0 -DDEBUG
debug: clean all
	@echo "Debug build complete"

clean:
	rm -rf $(BUILD_DIR) $(TARGET) simulation.log
	@echo "Cleaned"

test: $(TARGET)
	@echo "Running tests..."
	@bash tests/run_tests.sh

help:
	@echo "Available targets:"
	@echo "  make          - Build the project"
	@echo "  make run      - Build and run with defaults"
	@echo "  make run-test - Build and run with test config"
	@echo "  make debug    - Build with debug symbols"
	@echo "  make test     - Run test suite"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make help     - Show this help"

-include $(DEPS)
CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude -MMD -MP
LDFLAGS =

SRC_DIR = src
BUILD_DIR = build

CORE_SRCS = $(filter-out $(SRC_DIR)/server_main.c $(SRC_DIR)/client_main.c $(SRC_DIR)/network_server.c $(SRC_DIR)/network_client.c,$(wildcard $(SRC_DIR)/*.c))
OBJS = $(CORE_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJS_NAMED = $(CORE_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.named.o)
SERVER_OBJS = $(BUILD_DIR)/server_main.o $(BUILD_DIR)/network_server.o
CLIENT_OBJS = $(BUILD_DIR)/client_main.o $(BUILD_DIR)/network_client.o
DEPS = $(OBJS:.o=.d) $(OBJS_NAMED:.o=.d) $(SERVER_OBJS:.o=.d) $(CLIENT_OBJS:.o=.d)

.PHONY: all clean run run-network test test-network help

all: supermarket supermarket_named supermarket_server supermarket_client
	@echo "Build complete: simulation binaries, TCP server and observer client"

supermarket: $(BUILD_DIR) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

supermarket_named: $(BUILD_DIR) $(OBJS_NAMED)
	$(CC) $(CFLAGS) -DUSE_NAMED_SEM -o $@ $(OBJS_NAMED) $(LDFLAGS)

supermarket_server: $(BUILD_DIR) $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJS) $(LDFLAGS)

supermarket_client: $(BUILD_DIR) $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_OBJS) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.named.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -DUSE_NAMED_SEM -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) supermarket supermarket_named supermarket_server supermarket_client simulation.log
	@echo "Cleaned"

run: all
	@chmod +x run.sh
	./run.sh

run-network: all
	@chmod +x run_network.sh
	./run_network.sh

test: all
	@echo "Running IDZ-3 regression tests..."
	@bash tests/run_tests.sh

test-network: all
	@echo "Running IDZ-4 TCP integration test..."
	@bash tests/run_network_tests.sh

help:
	@echo "Available targets:"
	@echo "  make all          - Build IDZ-3 and IDZ-4 binaries"
	@echo "  make run          - Run the original IDZ-3 simulation"
	@echo "  make run-network  - Run the simulation with TCP server and local observer"
	@echo "  make test         - Run the IDZ-3 regression test suite"
	@echo "  make test-network - Test TCP publisher, multiple observers and reconnect"
	@echo "  ./supermarket_server [bind_ip] [port] - Start TCP monitoring server"
	@echo "  ./supermarket_client <server_ip> <port> - Connect an observer client"

-include $(DEPS)

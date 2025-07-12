# Makefile for RHelix
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -I./src/runtime
LDFLAGS = 

# Directories
BUILD_DIR = build
SRC_DIR = src
RUNTIME_DIR = $(SRC_DIR)/runtime

# Files
RUNTIME_SRCS = $(RUNTIME_DIR)/memory_manager.c
RUNTIME_OBJS = $(BUILD_DIR)/memory_manager.o

# Targets
.PHONY: all clean test runtime

all: runtime

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(RUNTIME_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

runtime: $(RUNTIME_OBJS)
	ar rcs $(BUILD_DIR)/librhelix_runtime.a $(RUNTIME_OBJS)
	@echo "Runtime library built successfully!"

test: $(BUILD_DIR)
	$(CC) $(CFLAGS) -DTEST_MEMORY_MANAGER $(RUNTIME_DIR)/memory_manager.c -o $(BUILD_DIR)/test_memory
	./$(BUILD_DIR)/test_memory
	@echo "All tests passed!"

clean:
	rm -rf $(BUILD_DIR)/*

# Makefile for RHelix
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -I./src/runtime -I./src/compiler
LDFLAGS =

# Directories
BUILD_DIR = build
SRC_DIR = src
RUNTIME_DIR = $(SRC_DIR)/runtime
COMPILER_DIR = $(SRC_DIR)/compiler

# Runtime files
RUNTIME_SRCS = $(RUNTIME_DIR)/memory_manager.c
RUNTIME_OBJS = $(BUILD_DIR)/memory_manager.o
RUNTIME_TEST_SRC = $(RUNTIME_DIR)/test_memory.c

# Compiler files
COMPILER_SRCS = $(COMPILER_DIR)/token.c $(COMPILER_DIR)/lexer.c
COMPILER_OBJS = $(BUILD_DIR)/token.o $(BUILD_DIR)/lexer.o
LEXER_TEST_SRC = $(COMPILER_DIR)/test_lexer.c

.PHONY: all clean test test-lexer runtime compiler

all: runtime compiler

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Object file rules
$(BUILD_DIR)/memory_manager.o: $(RUNTIME_DIR)/memory_manager.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/token.o: $(COMPILER_DIR)/token.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/lexer.o: $(COMPILER_DIR)/lexer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Library targets
runtime: $(RUNTIME_OBJS)
	ar rcs $(BUILD_DIR)/librhelix_runtime.a $(RUNTIME_OBJS)
	@echo "Runtime library built successfully!"

compiler: $(COMPILER_OBJS)
	ar rcs $(BUILD_DIR)/librhelix_compiler.a $(COMPILER_OBJS)
	@echo "Compiler library built successfully!"

# Test targets
test: | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(RUNTIME_SRCS) $(RUNTIME_TEST_SRC) -o $(BUILD_DIR)/test_memory
	./$(BUILD_DIR)/test_memory

test-lexer: | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COMPILER_SRCS) $(LEXER_TEST_SRC) -o $(BUILD_DIR)/test_lexer
	./$(BUILD_DIR)/test_lexer

clean:
	rm -rf $(BUILD_DIR)/*

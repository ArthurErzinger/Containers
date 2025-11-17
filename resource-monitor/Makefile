# Compiler and flags
CC = gcc
# Per the project PDF: -Wall, -Wextra. Also adding -g for debugging, -Iinclude to find headers, and -std=c23 for the C23 standard.
CFLAGS = -Wall -Wextra -g -Iinclude -std=c2x -D_DEFAULT_SOURCE
LDFLAGS =

# Project name
TARGET = resource-monitor

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj
TESTDIR = tests
TESTBINDIR = $(TESTDIR)/bin

# Find all .c files in the source directory
SOURCES = $(wildcard $(SRCDIR)/*.c)

# Create a list of object files in the obj directory
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

# Test/workload binaries
TEST_SOURCES = $(wildcard $(TESTDIR)/*.c)
TEST_BINARIES = $(patsubst $(TESTDIR)/%.c,$(TESTBINDIR)/%,$(TEST_SOURCES))

# Phony targets are not files
.PHONY: all clean run workloads tests-clean experimentos exp1 exp2 exp3 exp4 exp5

# Default target: build the executable
all: $(TARGET)

# Rule to link the executable
$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJECTS)

# Pattern rule to compile .c files into .o files
# This also creates the object directory if it doesn't exist
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Target to clean up build files
clean:
	rm -rf $(OBJDIR) $(TARGET) $(TESTBINDIR)

# Target to run the executable
run: all
	./$(TARGET)

# Build workload/test helpers
workloads: $(TEST_BINARIES)

$(TESTBINDIR)/%: $(TESTDIR)/%.c
	@mkdir -p $(TESTBINDIR)
	$(CC) $(CFLAGS) -pthread $< -o $@

tests-clean:
	rm -rf $(TESTBINDIR)

# Targets para executar experimentos
experimentos: all workloads
	@bash scripts/run_experiments.sh

exp1: all workloads
	@bash scripts/experimento1_overhead.sh

exp2: all workloads
	@bash scripts/experimento2_namespaces.sh

exp3: all workloads
	@sudo bash scripts/experimento3_cpu_throttle.sh

exp4: all workloads
	@sudo bash scripts/experimento4_mem_limit.sh

exp5: all workloads
	@sudo bash scripts/experimento5_io_limit.sh

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

# Find all .c files in the source directory
SOURCES = $(wildcard $(SRCDIR)/*.c)

# Create a list of object files in the obj directory
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

# Phony targets are not files
.PHONY: all clean run

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
	rm -rf $(OBJDIR) $(TARGET)

# Target to run the executable
run: all
	./$(TARGET)

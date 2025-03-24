CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O2
LDFLAGS = -lmicrohttpd -lpq

# Directories
SRCDIR = .
BUILDDIR = ../build
BINDIR = $(BUILDDIR)/bin
OBJDIR = $(BUILDDIR)/obj

# Sources and objects
SRCS = $(wildcard $(SRCDIR)/*.c) \
       $(wildcard $(SRCDIR)/common/*.c) \
       $(wildcard $(SRCDIR)/http/*.c) \
       $(wildcard $(SRCDIR)/pg/*.c)
       
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

# Target binary
TARGET = $(BINDIR)/pg_http_bridge

# Includes
INCLUDES = -I$(SRCDIR)

# Default target
all: dirs $(TARGET)

# Create build directories
dirs:
	@mkdir -p $(BINDIR)
	@mkdir -p $(OBJDIR)
	@mkdir -p $(OBJDIR)/common
	@mkdir -p $(OBJDIR)/http
	@mkdir -p $(OBJDIR)/pg

# Compile
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Link
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Clean
clean:
	rm -rf $(BUILDDIR)

# Run
run: all
	$(TARGET)

# Install
install: all
	cp $(TARGET) /usr/local/bin/pg_http_bridge

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: all

# Docker build
docker:
	docker-compose up --build

.PHONY: all dirs clean run install debug docker 
CC = gcc
CFLAGS = -Wall -Werror -g
LDFLAGS = -lpq -lmicrohttpd

# Try to find PostgreSQL using pg_config
PG_CONFIG := $(shell which pg_config 2>/dev/null)
ifdef PG_CONFIG
    PG_INCLUDEDIR := $(shell $(PG_CONFIG) --includedir)
    PG_LIBDIR := $(shell $(PG_CONFIG) --libdir)
    CFLAGS += -I$(PG_INCLUDEDIR)
    LDFLAGS := -L$(PG_LIBDIR) $(LDFLAGS)
endif

# For macOS with Homebrew
ifeq ($(shell uname), Darwin)
    # Check for homebrew libpq
    ifneq ($(wildcard /usr/local/opt/libpq/include/libpq-fe.h),)
        CFLAGS += -I/usr/local/opt/libpq/include
        LDFLAGS := -L/usr/local/opt/libpq/lib $(LDFLAGS)
    endif
    
    # Check for Homebrew libpq on Apple Silicon
    ifneq ($(wildcard /opt/homebrew/opt/libpq/include/libpq-fe.h),)
        CFLAGS += -I/opt/homebrew/opt/libpq/include
        LDFLAGS := -L/opt/homebrew/opt/libpq/lib $(LDFLAGS)
    endif
    
    # Check for Homebrew libmicrohttpd
    ifneq ($(wildcard /usr/local/opt/libmicrohttpd/include/microhttpd.h),)
        CFLAGS += -I/usr/local/opt/libmicrohttpd/include
        LDFLAGS := -L/usr/local/opt/libmicrohttpd/lib $(LDFLAGS)
    endif
    
    # Check for Homebrew libmicrohttpd on Apple Silicon
    ifneq ($(wildcard /opt/homebrew/opt/libmicrohttpd/include/microhttpd.h),)
        CFLAGS += -I/opt/homebrew/opt/libmicrohttpd/include
        LDFLAGS := -L/opt/homebrew/opt/libmicrohttpd/lib $(LDFLAGS)
    endif
endif

SRCDIR = src
OBJDIR = obj
BINDIR = bin

SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/pg/pg_client.c \
          $(SRCDIR)/pg/s3_api.c \
          $(SRCDIR)/http/http_server.c

OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

TARGET = $(BINDIR)/pgs3

# Installation paths
PREFIX ?= /usr/local
INSTALL_BIN = $(PREFIX)/bin

all: directories $(TARGET)

directories:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(OBJDIR)/pg
	@mkdir -p $(OBJDIR)/http
	@mkdir -p $(BINDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

install: $(TARGET)
	@mkdir -p $(INSTALL_BIN)
	cp $(TARGET) $(INSTALL_BIN)/pgs3
	@echo "Installed pgs3 to $(INSTALL_BIN)"

clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all clean directories install 
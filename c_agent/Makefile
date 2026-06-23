# Ejecutable
TARGET = build/c_agent

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

INCLUDES = -Iinclude

SRCS = main.c \
       src/comms/event_handler.c \
       src/comms/resource_adapter.c \
       src/comms/server_init.c \
       src/comms/sys_epoll.c \
       src/comms/sys_socket.c \
       src/comms/thread_pool.c \
       src/resources/node-structures.c \
       src/resources/queues.c \
       src/resources/tablahash.c

OBJS = $(SRCS:%.c=build/%.o)

all: $(TARGET)

# Crear directorios automáticamente
build:
	mkdir -p build

build/src/comms:
	mkdir -p build/src/comms

build/src/resources:
	mkdir -p build/src/resources

# Link
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile
build/%.o: %.c | build build/src/comms build/src/resources
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

run: $(TARGET)
	./build/c_agent 127.0.0.1 8000 4 8192 1

clean:
	rm -rf build

clear: clean

.PHONY: all run clean clear